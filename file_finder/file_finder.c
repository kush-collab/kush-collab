#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#include "file_finder.h"

/* Seconds between periodic dumps; override at build time with -DFILE_FINDER_DUMP_PERIOD_S=<n>. */
#ifndef FILE_FINDER_DUMP_PERIOD_S
#define FILE_FINDER_DUMP_PERIOD_S 1
#endif

#if FILE_FINDER_DUMP_PERIOD_S < 1
#error "FILE_FINDER_DUMP_PERIOD_S must be at least 1"
#endif

/* Path buffer size; the assignment assumes paths are at most 255 characters. */
#define MAX_PATH_SIZE 256
/* The dumper wakes this often to check for exit; the dump period is counted in ticks. */
#define DUMPER_TICK_MS 100
#define TICKS_PER_SECOND (1000 / DUMPER_TICK_MS)
#define NS_PER_MS 1000000L

/* Checks for a directory and at least one non-empty substring; returns 0 if valid. */
int validate_arguments(int argc, char **argv)
{
    if (argc <= FIRST_SUBSTRING_ARG) {
        return 1;
    }

    /* An empty substring would match every filename. */
    for (int index = FIRST_SUBSTRING_ARG; index < argc; ++index) {
        if (argv[index] == NULL || argv[index][0] == '\0') {
            return 1;
        }
    }

    return 0;
}

/* Prints how to run the program and which commands it accepts while scanning. */
void print_usage(const char *program_name)
{
    printf("Usage: %s <directory> <substring1> [substring2 ...]\n",
           program_name);
    printf("While scanning, enter dump to print pending matches or exit to stop.\n");
}

/* Sets up an empty shared match list and its mutex; returns 0 on success. */
int initialize_match_list(MatchList *matches)
{
    if (matches == NULL) {
        return 1;
    }

    matches->head = NULL;
    matches->tail = NULL;
    return mtx_init(&matches->mutex, mtx_plain) == thrd_success ? 0 : 1;
}

/* Releases the match list mutex and exits the process if unlocking fails. */
static void unlock_matches(MatchList *matches)
{
    if (mtx_unlock(&matches->mutex) != thrd_success) {
        /* A broken lock can leave other threads blocked, making cleanup unsafe. */
        fprintf(stderr, "Failed to unlock the match container.\n");
        _Exit(EXIT_FAILURE);
    }
}

/* Prints every pending match, then empties the list. Used by both periodic and interactive dumps. */
int dump_matches(MatchList *matches)
{
    MatchNode *node;
    int status = 0;

    if (matches == NULL || mtx_lock(&matches->mutex) != thrd_success) {
        fprintf(stderr, "Failed to lock the match container for dumping.\n");
        return 1;
    }

    /* Hold the lock until the printed entries have been removed. */
    node = matches->head;
    while (node != NULL) {
        MatchNode *next = node->next;
        if (status == 0 && printf("%s\n", node->filename) < 0) {
            perror("Failed to print matches");
            status = 1;
        }
        free(node->filename);
        free(node);
        node = next;
    }

    matches->head = NULL;
    matches->tail = NULL;
    /* Make each batch visible immediately, even when output goes to a pipe or file. */
    if (fflush(stdout) == EOF) {
        perror("Failed to flush matches");
        status = 1;
    }
    unlock_matches(matches);
    return status;
}

/* Copies a matching filename and appends it to the end of the shared list. */
int add_match(MatchList *matches, const char *filename)
{
    MatchNode *node;
    char *filename_copy;
    size_t filename_length;

    if (matches == NULL || filename == NULL) {
        fprintf(stderr, "Invalid match arguments.\n");
        return 1;
    }

    /* Copy the name because readdir reuses its entry storage on later calls. */
    filename_length = strlen(filename);
    filename_copy = (char *)malloc(filename_length + 1);
    node = (MatchNode *)malloc(sizeof(*node));
    if (filename_copy == NULL || node == NULL) {
        free(filename_copy);
        free(node);
        fprintf(stderr, "Failed to allocate a match.\n");
        return 1;
    }

    memcpy(filename_copy, filename, filename_length + 1);
    node->filename = filename_copy;
    node->next = NULL;

    if (mtx_lock(&matches->mutex) != thrd_success) {
        free(filename_copy);
        free(node);
        fprintf(stderr, "Failed to lock the match container for insertion.\n");
        return 1;
    }

    /* Append under the lock so other workers and dumps cannot change the list here. */
    if (matches->tail == NULL) {
        matches->head = node;
    } else {
        matches->tail->next = node;
    }
    matches->tail = node;
    unlock_matches(matches);
    return 0;
}

/* Frees any matches still in the list and destroys its mutex. Call only after all threads are joined. */
void free_match_list(MatchList *matches)
{
    MatchNode *node;

    if (matches == NULL) {
        return;
    }

    /* All threads have been joined before cleanup, so no lock is needed here. */
    node = matches->head;
    while (node != NULL) {
        MatchNode *next = node->next;
        free(node->filename);
        free(node);
        node = next;
    }

    matches->head = NULL;
    matches->tail = NULL;
    mtx_destroy(&matches->mutex);
}

/* Recursively walks a directory and adds every filename containing the substring.
   Unreadable folders are skipped with a warning; returns non-zero only on major errors. */
int scan_directory(const char *directory, const char *substring,
                   MatchList *matches, atomic_bool *keep_running)
{
    DIR *directory_handle;
    struct dirent *entry;
    char full_path[MAX_PATH_SIZE];
    struct stat file_status;
    int status = 0;

    if (directory == NULL || substring == NULL || matches == NULL ||
        keep_running == NULL) {
        fprintf(stderr, "Invalid scan arguments.\n");
        return 1;
    }
    if (!atomic_load(keep_running)) {
        return 0;
    }

    if (strlen(directory) >= sizeof(full_path)) {
        fprintf(stderr, "Directory path is too long: %s\n", directory);
        return 1;
    }

    /* main already checked the starting directory, so an unreadable folder here
       is a subfolder: warn, skip it and let the search continue. */
    directory_handle = opendir(directory);
    if (directory_handle == NULL) {
        fprintf(stderr, "Warning: skipping directory %s: %s\n", directory, strerror(errno));
        return 0;
    }

    /* On exit, unwind normally so each open directory is closed below. */
    while (atomic_load(keep_running)) {
        int written;
        size_t name_length;

        /* NULL means either end-of-directory or an error; errno distinguishes them. */
        errno = 0;
        entry = readdir(directory_handle);
        if (entry == NULL) {
            /* A read error skips the rest of this folder instead of stopping the search. */
            if (errno != 0) {
                fprintf(stderr, "Warning: stopped reading directory %s: %s\n",
                        directory, strerror(errno));
            }
            break;
        }

        // Skip the current and parent directory entries to avoid infinite recursion.
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        name_length = strlen(entry->d_name);
        if (strlen(directory) + 1 + name_length >= sizeof(full_path)) {
            fprintf(stderr, "File path is too long: %s/%s\n",
                    directory, entry->d_name);
            status = 1;
            break;
        }

        written = snprintf(full_path, sizeof(full_path), "%s/%s",
                           directory, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(full_path)) {
            fprintf(stderr, "Failed to construct path: %s/%s\n", directory, entry->d_name);
            status = 1;
            break;
        }

        /* An entry we cannot inspect is skipped; the rest of the folder is still searched. */
        if (stat(full_path, &file_status) != 0) {
            fprintf(stderr, "Warning: skipping %s: %s\n", full_path, strerror(errno));
            continue;
        }

        /* Recurse into directories; match only the filename, not its parent path. */
        if (S_ISDIR(file_status.st_mode)) {
            status = scan_directory(full_path, substring, matches, keep_running);
        } else if (strstr(entry->d_name, substring) != NULL) {
            status = add_match(matches, entry->d_name);
#ifdef FILE_FINDER_TEST_DELAY
            /* Test builds only: wait FILE_FINDER_TEST_DELAY ms per match so dump and exit can be tried by hand. */
            if (status == 0) {
                const struct timespec delay = {
                    FILE_FINDER_TEST_DELAY / 1000,
                    (FILE_FINDER_TEST_DELAY % 1000) * NS_PER_MS
                };
                thrd_sleep(&delay, NULL);
            }
#endif
        }
        if (status != 0) {
            break;
        }
    }

    if (closedir(directory_handle) != 0) {
        fprintf(stderr, "Failed to close directory %s: %s\n", directory, strerror(errno));
        status = 1;
    }
    return status;
}

/* Worker thread entry point: searches the whole tree for one substring. */
int search_thread(void *argument)
{
    SearchThreadArgs *args = (SearchThreadArgs *)argument;
    int status;

    if (args == NULL) {
        fprintf(stderr, "Missing search thread arguments.\n");
        return 1;
    }

    status = scan_directory(args->directory, args->substring, args->matches,
                            args->keep_running);
    /* A failed scan stops the other threads too; main collects this return status. */
    if (status != 0) {
        atomic_store(args->keep_running, false);
    }
    /* Decrement on both success and failure so main knows this worker is finished. */
    atomic_fetch_sub(args->active_searches, 1);
    return status;
}

/* Dumper thread entry point: prints pending matches every FILE_FINDER_DUMP_PERIOD_S seconds until stopped. */
int periodic_dumper_thread(void *argument)
{
    DumperThreadArgs *args = (DumperThreadArgs *)argument;
    int ticks = 0;

    if (args == NULL || args->matches == NULL ||
        args->keep_running == NULL) {
        fprintf(stderr, "Invalid dumper thread arguments.\n");
        return 1;
    }

    while (atomic_load(args->keep_running)) {
        struct timespec remaining = {0, DUMPER_TICK_MS * NS_PER_MS};
        int result;

        /* Check stop every tick, but dump only once per FILE_FINDER_DUMP_PERIOD_S seconds. */
        do {
            result = thrd_sleep(&remaining, &remaining);
        } while (result == -1 && atomic_load(args->keep_running));
        /* -1 means interrupted; any other negative value is an error. */
        if (result < -1) {
            fprintf(stderr, "Failed to sleep in the dumper thread.\n");
            atomic_store(args->keep_running, false);
            return 1;
        }
        if (++ticks == FILE_FINDER_DUMP_PERIOD_S * TICKS_PER_SECOND &&
            atomic_load(args->keep_running)) {
            ticks = 0;
            if (dump_matches(args->matches) != 0) {
                atomic_store(args->keep_running, false);
                return 1;
            }
        }
    }

    return 0;
}

/* Releases everything owned by the shared container at program shutdown. */
void free_resources(MatchList *matches)
{
    free_match_list(matches);
}

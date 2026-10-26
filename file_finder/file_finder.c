#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "file_finder.h"

int validate_arguments(int argc, char **argv)
{
    (void)argv;

    if (argc < 3) {
        return 1;
    }

    return 0;
}

void print_usage(const char *program_name)
{
    printf("Usage: %s <directory> <substring1> [substring2 ...]\n",
           program_name);
}

int initialize_match_list(MatchList *matches)
{
    if (matches == NULL) {
        return 1;
    }

    matches->head = NULL;
    matches->tail = NULL;
    return mtx_init(&matches->mutex, mtx_plain) == thrd_success ? 0 : 1;
}

void dump_matches(MatchList *matches)
{
    MatchNode *node;

    if (matches == NULL || mtx_lock(&matches->mutex) != thrd_success) {
        return;
    }

    for (node = matches->head; node != NULL; node = node->next) {
        printf("%s\n", node->filename);
    }

    mtx_unlock(&matches->mutex);
}

void add_match(MatchList *matches, const char *filename)
{
    MatchNode *node;
    char *filename_copy;
    size_t filename_length;

    if (matches == NULL || filename == NULL) {
        return;
    }

    filename_length = strlen(filename);
    filename_copy = (char *)malloc(filename_length + 1);
    node = (MatchNode *)malloc(sizeof(*node));
    if (filename_copy == NULL || node == NULL) {
        free(filename_copy);
        free(node);
        return;
    }

    memcpy(filename_copy, filename, filename_length + 1);
    node->filename = filename_copy;
    node->next = NULL;

    if (mtx_lock(&matches->mutex) != thrd_success) {
        free(filename_copy);
        free(node);
        return;
    }

    if (matches->tail == NULL) {
        matches->head = node;
    } else {
        matches->tail->next = node;
    }
    matches->tail = node;
    mtx_unlock(&matches->mutex);
}

void free_match_list(MatchList *matches)
{
    MatchNode *node;

    if (matches == NULL) {
        return;
    }

    (void)mtx_lock(&matches->mutex);

    node = matches->head;
    while (node != NULL) {
        MatchNode *next = node->next;
        free(node->filename);
        free(node);
        node = next;
    }

    matches->head = NULL;
    matches->tail = NULL;
    mtx_unlock(&matches->mutex);
    mtx_destroy(&matches->mutex);
}

void scan_directory(const char *directory, const char *substring,
                    MatchList *matches)
{
    DIR *directory_handle;
    struct dirent *entry;
    char full_path[256];
    struct stat file_status;

    if (directory == NULL || substring == NULL || matches == NULL) {
        return;
    }

    if (strlen(directory) >= sizeof(full_path)) {
        fprintf(stderr, "Directory path is too long: %s\n", directory);
        return;
    }

    directory_handle = opendir(directory);
    if (directory_handle == NULL) {
        fprintf(stderr, "Failed to scan directory: %s\n", directory);
        return;
    }

    while ((entry = readdir(directory_handle)) != NULL) {
        int written;
        size_t name_length;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        name_length = strlen(entry->d_name);
        if (strlen(directory) + 1 + name_length >= sizeof(full_path)) {
            fprintf(stderr, "File path is too long: %s/%s\n",
                    directory, entry->d_name);
            continue;
        }

        written = snprintf(full_path, sizeof(full_path), "%s/%s",
                           directory, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(full_path)) {
            continue;
        }

        if (stat(full_path, &file_status) != 0) {
            continue;
        }

        if (S_ISDIR(file_status.st_mode)) {
            scan_directory(full_path, substring, matches);
        } else if (strstr(entry->d_name, substring) != NULL) {
            add_match(matches, entry->d_name);
        }
    }

    closedir(directory_handle);
}

int search_thread(void *argument)
{
    SearchThreadArgs *args = (SearchThreadArgs *)argument;

    if (args == NULL) {
        return 1;
    }

    scan_directory(args->directory, args->substring, args->matches);
    return 0;
}

int periodic_dumper_thread(void *argument)
{
    (void)argument;
    /* TODO: Periodically dump and clear the shared match container. */
    return 0;
}

void free_resources(MatchList *matches)
{
    free_match_list(matches);
}

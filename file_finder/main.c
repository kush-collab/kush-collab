#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>

#include "file_finder.h"

/* How often the console loop wakes to check whether all searches have finished. */
#define INPUT_POLL_MS 100
/* Longest accepted command line, including the terminating null byte. */
#define COMMAND_SIZE 32

/* Set by Ctrl+C. A signal handler may only safely set a flag like this;
   the console loop turns it into the same shutdown as "exit". */
static volatile sig_atomic_t interrupted = 0;

static void handle_interrupt(int signal_number)
{
    (void)signal_number;
    interrupted = 1;
}

static int handle_commands(MatchList *matches, atomic_bool *keep_running,
                           atomic_size_t *active_searches)
{
    struct pollfd input = {STDIN_FILENO, POLLIN, 0};
    char command[COMMAND_SIZE];
    size_t length = 0;
    bool too_long = false;

    while (atomic_load(keep_running) && atomic_load(active_searches) != 0) {
        /* Periodically check completion instead of blocking forever on input. */
        int ready = poll(&input, 1, INPUT_POLL_MS);
        char character;
        ssize_t count;

        /* Ctrl+C behaves exactly like typing exit. */
        if (interrupted) {
            atomic_store(keep_running, false);
            break;
        }
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("Failed to wait for console input");
            return 1;
        }
        if (ready == 0) {
            continue;
        }
        if (input.revents & (POLLERR | POLLNVAL)) {
            fprintf(stderr, "Console input is unavailable.\n");
            return 1;
        }

        /* Read available characters only, so a partial command cannot block shutdown. */
        count = read(input.fd, &character, 1);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("Failed to read console input");
            return 1;
        }
        if (count == 0) {
            input.fd = -1; /* EOF disables input; searches still finish normally. */
            character = '\n';
        }
        if (character == '\r') {
            continue;
        }
        if (character != '\n') {
            if (length < sizeof(command) - 1 && character != '\0') {
                command[length++] = character;
            } else {
                too_long = true;
            }
            continue;
        }

        /* Process a complete line, then reuse the buffer for the next command. */
        command[length] = '\0';
        if (!too_long && strcmp(command, "dump") == 0) {
            if (dump_matches(matches) != 0) {
                return 1;
            }
        } else if (!too_long && strcmp(command, "exit") == 0) {
            atomic_store(keep_running, false);
        } else if (length != 0 || too_long) {
            fprintf(stderr, "Unknown command. Use dump or exit.\n");
        }
        length = 0;
        too_long = false;
    }
    return 0;
}

static int join_thread(thrd_t thread)
{
    int result;

    if (thrd_join(thread, &result) != thrd_success) {
        /* Without a successful join, shared memory may still be in use. */
        fprintf(stderr, "Failed to join a thread; cannot safely clean up.\n");
        _Exit(EXIT_FAILURE);
    }
    return result;
}

int main(int argc, char **argv)
{
    size_t substring_count;
    size_t index;
    MatchList matches = {0};
    thrd_t *threads;
    thrd_t dumper_thread;
    SearchThreadArgs *thread_args;
    int *thread_created;
    int dumper_created = 0;
    DumperThreadArgs dumper_args;
    atomic_bool keep_running;
    atomic_size_t active_searches;
    int status = 0;

    /* Check required arguments and reject empty substrings before allocating resources. */
    if (validate_arguments(argc, argv) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    substring_count = (size_t)argc - FIRST_SUBSTRING_ARG;

    /* A bad starting directory is a real error; unreadable subfolders are only skipped later. */
    DIR *start_directory = opendir(argv[1]);
    if (start_directory == NULL) {
        report_errno("Cannot open directory", argv[1]);
        return 1;
    }
    closedir(start_directory);

    /* Let output failures reach normal error handling instead of killing the process. */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("Failed to configure output error handling");
        return 1;
    }
    /* Route Ctrl+C through the normal shutdown so threads are joined and memory freed. */
    if (signal(SIGINT, handle_interrupt) == SIG_ERR) {
        perror("Failed to configure Ctrl+C handling");
        return 1;
    }
    if (printf("Search directory: %s\nSubstring count: %zu\n"
               "Commands while scanning:\n"
               "  dump + Enter : print and clear pending matches\n"
               "  exit + Enter : stop searching and release resources\n",
               argv[1], substring_count) < 0 || fflush(stdout) == EOF) {
        perror("Failed to print search information");
        return 1;
    }

    /* Every search thread and the dumper use this one mutex-protected list. */
    if (initialize_match_list(&matches) != 0) {
        fprintf(stderr, "Failed to initialize the shared match container.\n");
        return 1;
    }

    /* Keep each worker's arguments alive until it is joined.
       The creation flags tell cleanup which thread handles are valid. */
    threads = (thrd_t *)malloc(substring_count * sizeof(*threads));
    if (threads == NULL) {
        fprintf(stderr, "Failed to allocate search thread resources.\n");
        free_resources(&matches);
        return 1;
    }
    
    thread_args = (SearchThreadArgs *)malloc(substring_count *
                                             sizeof(*thread_args));
    if(thread_args == NULL) {
        fprintf(stderr, "Failed to allocate search thread resources.\n");
        free(threads);
        free_resources(&matches);
        return 1;
    }

    thread_created = (int *)calloc(substring_count, sizeof(*thread_created));
    if(thread_created == NULL) {
        fprintf(stderr, "Failed to allocate search thread resources.\n");
        free(threads);
        free(thread_args);
        free_resources(&matches);
        return 1;
    }

    /* The stop flag is shared by all threads.
       The worker count lets console handling detect normal search completion. */
    atomic_init(&keep_running, true);
    atomic_init(&active_searches, 0);
    dumper_args.matches = &matches;
    dumper_args.keep_running = &keep_running;
    if (thrd_create(&dumper_thread, periodic_dumper_thread,
                    &dumper_args) != thrd_success) {
        fprintf(stderr, "Failed to create periodic dumper thread.\n");
        atomic_store(&keep_running, false);
        status = 1;
    } else {
        dumper_created = 1;
    }

    /* Each substring gets its own worker, but all workers share the same results. */
    for (index = 0; index < substring_count &&
         atomic_load(&keep_running); ++index) {
        thread_args[index].directory = argv[1];
        thread_args[index].substring = argv[index + FIRST_SUBSTRING_ARG];
        thread_args[index].matches = &matches;
        thread_args[index].keep_running = &keep_running;
        thread_args[index].active_searches = &active_searches;

        /* Count before creation because a worker may finish as soon as it starts. */
        atomic_fetch_add(&active_searches, 1);
        if (thrd_create(&threads[index], search_thread,
                        &thread_args[index]) != thrd_success) {
            fprintf(stderr, "Failed to create search thread for substring: %s\n",
                    argv[index + FIRST_SUBSTRING_ARG]);
            atomic_fetch_sub(&active_searches, 1);
            atomic_store(&keep_running, false);
            status = 1;
        } else {
            thread_created[index] = 1;
        }
    }

    /* The main thread accepts commands while the search threads do their work. */
    if (status == 0) {
        status = handle_commands(&matches, &keep_running, &active_searches);
    }
    /* Remember normal completion before stopping the dumper and remaining workers. */
    bool completed = status == 0 && atomic_load(&keep_running);
    atomic_store(&keep_running, false);

    /* Stop and join all users of the shared container before freeing it. */
    for (index = 0; index < substring_count; ++index) {
        if (thread_created[index]) {
            if (join_thread(threads[index]) != 0) {
                status = 1;
            }
        }
    }

    if (dumper_created) {
        if (join_thread(dumper_thread) != 0) {
            status = 1;
        }
    }

    /* Print leftovers only after a successful scan; exit discards pending results. */
    if (completed && status == 0) {
        status = dump_matches(&matches);
    }
    free_resources(&matches);
    free(threads);
    free(thread_args);
    free(thread_created);
    return status;
}

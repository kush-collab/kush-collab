#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>

#include "file_finder.h"

int main(int argc, char **argv)
{
    size_t substring_count;
    size_t index;
    MatchList matches = {NULL, NULL, {0}};
    thrd_t *threads;
    thrd_t dumper_thread;
    SearchThreadArgs *thread_args;
    int *thread_created;
    int dumper_created = 0;
    DumperThreadArgs dumper_args;
    atomic_bool keep_dumper_running;

    if (validate_arguments(argc, argv) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    substring_count = (size_t)argc - 2;

    printf("Search directory: %s\n", argv[1]);
    printf("Substring count: %zu\n", substring_count);

    if (initialize_match_list(&matches) != 0) {
        fprintf(stderr, "Failed to initialize the shared match container.\n");
        return 1;
    }

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

    atomic_init(&keep_dumper_running, true);
    dumper_args.matches = &matches;
    dumper_args.keep_running = &keep_dumper_running;
    if (thrd_create(&dumper_thread, periodic_dumper_thread,
                    &dumper_args) != thrd_success) {
        fprintf(stderr, "Failed to create periodic dumper thread.\n");
        atomic_store(&keep_dumper_running, false);
    } else {
        dumper_created = 1;
    }

    for (index = 0; index < substring_count; ++index) {
        thread_args[index].directory = argv[1];
        thread_args[index].substring = argv[index + 2];
        thread_args[index].matches = &matches;

        if (thrd_create(&threads[index], search_thread,
                        &thread_args[index]) != thrd_success) {
            fprintf(stderr, "Failed to create search thread for substring: %s\n",
                    argv[index + 2]);
        } else {
            thread_created[index] = 1;
        }
    }

    for (index = 0; index < substring_count; ++index) {
        if (thread_created[index]) {
            thrd_join(threads[index], NULL);
        }
    }

    atomic_store(&keep_dumper_running, false);
    if (dumper_created) {
        thrd_join(dumper_thread, NULL);
    }

    dump_matches(&matches);
    free_resources(&matches);
    free(threads);
    free(thread_args);
    free(thread_created);
    return 0;
}

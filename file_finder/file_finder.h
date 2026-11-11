#ifndef FILE_FINDER_H
#define FILE_FINDER_H

#include <stddef.h>
#include <stdatomic.h>
#include <threads.h>

/* argv layout: argv[1] is the directory, substrings start at argv[2]. */
#define FIRST_SUBSTRING_ARG 2

typedef struct MatchNode {
    char *filename;
    struct MatchNode *next;
} MatchNode;

typedef struct {
    MatchNode *head;
    MatchNode *tail;
    mtx_t mutex;
} MatchList;

typedef struct {
    const char *directory;
    const char *substring;
    MatchList *matches;
    atomic_bool *keep_running;
    atomic_size_t *active_searches;
} SearchThreadArgs;

typedef struct {
    MatchList *matches;
    atomic_bool *keep_running;
} DumperThreadArgs;

int validate_arguments(int argc, char **argv);
void print_usage(const char *program_name);

int initialize_match_list(MatchList *matches);
int dump_matches(MatchList *matches);
int add_match(MatchList *matches, const char *filename);
void free_match_list(MatchList *matches);

int scan_directory(const char *directory, const char *substring,
                   MatchList *matches, atomic_bool *keep_running);
int search_thread(void *argument);
int periodic_dumper_thread(void *argument);

void free_resources(MatchList *matches);

#endif

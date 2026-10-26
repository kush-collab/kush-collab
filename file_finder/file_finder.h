#ifndef FILE_FINDER_H
#define FILE_FINDER_H

#include <stddef.h>
#include <threads.h>

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
} SearchThreadArgs;

typedef struct {
    MatchList *matches;
} DumperThreadArgs;

int validate_arguments(int argc, char **argv);
void print_usage(const char *program_name);

int initialize_match_list(MatchList *matches);
void dump_matches(MatchList *matches);
void add_match(MatchList *matches, const char *filename);
void free_match_list(MatchList *matches);

void scan_directory(const char *directory, const char *substring,
                    MatchList *matches);
int search_thread(void *argument);
int periodic_dumper_thread(void *argument);

void free_resources(MatchList *matches);

#endif

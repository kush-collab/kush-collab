Threaded File Finder
========================================================================

A C11 program that searches a directory tree for files whose names contain
one or more substrings. Each substring is searched by its own thread, matches
are collected in a shared container, and results are printed periodically or
on request.


Build and run
------------------------------------------------------------------------

    make
    ./file-finder <directory> <substring1> [substring2 ...]

Example:

    ./file-finder /home/user/projects report .log

While the search is running, type one of these commands and press Enter:

    dump   print the matches found so far and clear the container
    exit   stop all searches, release resources and quit

The program also exits on its own once every search thread has finished.


Requested features and how they are implemented
--------------------------------------------------------------------------

1. Command-line arguments
   - argv[1] is the directory, argv[2..] are the substrings.
   - At least one substring is required, and empty substrings are rejected
     because they would match every file.
   - The starting directory is opened once before any thread is created, so
     a wrong path is reported immediately.

2. Recursive filename search
   - scan_directory() walks the tree with opendir/readdir and calls itself
     for every subdirectory.
   - Only the file name is matched (strstr), not the full path, so a folder
     name containing the substring does not make every file inside it match.
   - "." and ".." are skipped to avoid looping forever.

3. One thread per substring
   - main() creates one search thread for each substring. Each thread walks
     the whole tree independently for its own substring.

4. Shared container
   - All matches go into one linked list (head/tail pointers) protected by a
     single mutex.
   - add_match() copies the file name before locking, because readdir reuses
     its buffer, then appends to the tail under the lock.

5. Periodic dumper thread
   - A separate thread prints and clears the container every
     FILE_FINDER_DUMP_PERIOD_S seconds (default 1).
   - It sleeps in short 100 ms ticks so it can notice "exit" quickly even
     when the dump period is long.

6. Interactive "dump"
   - The main thread reads commands from the console while the workers run.
   - "dump" calls the same dump_matches() function as the periodic thread.
     Printing and clearing happen under one lock, so each match is printed
     exactly once no matter which of the two dumps it first.

7. Interactive "exit"
   - "exit" sets a shared atomic flag (keep_running) to false.
   - Search threads check this flag before reading each directory entry and
     unwind normally, closing every directory they opened.
   - The dumper thread checks it on every tick.
   - Matches still pending at exit are discarded, not printed.

8. Resource cleanup
   - main() joins every thread that was created before freeing anything, so
     no thread can still be using the shared list when it is released.
   - The list, its mutex and all per-thread arrays are freed on every path:
     normal completion, "exit", and error.
   - On normal completion, any matches left in the container are printed
     once more before cleanup.


Design notes
----------------------------------------------------------------------------

Console input without blocking shutdown
   The main thread uses poll() on stdin with a 100 ms timeout. This lets it
   accept commands and also notice when all search threads have finished,
   instead of waiting forever for the user to type something.

Knowing when searches are done
   An atomic counter (active_searches) is incremented before each search
   thread is created and decremented when it finishes. When it reaches
   zero, the console loop ends.

Error handling
   - A missing or unreadable starting directory is an error (exit code 1).
   - An unreadable subfolder or file is reported as a warning and skipped;
     the rest of the tree is still searched.
   - Major errors (out of memory, path too long, failure to create a thread)
     stop all threads through the same keep_running flag. The error is
     reported and the program returns a non-zero exit code.
   - If joining a thread or unlocking the mutex fails, the program exits
     immediately, because freeing shared memory that another thread may
     still use would be unsafe.

Why a single mutex and a linked list
   Searching the file system is much slower than appending to a list, so a
   single lock does not become a bottleneck. A linked list with a tail
   pointer gives O(1) appends and is simple to print and clear.


Assumptions (from the assignment)
--------------------------------------------------------------------------

- Paths are at most 255 characters (buffer size MAX_PATH_SIZE = 256).
- There are no symbolic links or circular paths.
- Substrings do not overlap, so a file is expected to match only one of
  them.
- The file system does not change during the search.


Known behaviour
---------------------------------------------------------------------------

- Output order is not deterministic, because several threads add matches
  at the same time.
- Every thread walks the whole tree, so a warning about an unreadable
  folder can appear once per substring.


Testing the dumps by hand
---------------------------------------------------------------------------

On a small directory the search finishes almost instantly, which makes the
periodic and interactive dumps hard to observe. Two build-time options
make this easier (run "make clean" when switching between builds):

    make DUMP_PERIOD_S=10
        Normal build with a 10 second dump period.

    make slow DELAY_MS=200 DUMP_PERIOD_S=10
        Test build: each search thread waits 200 ms after every match.
        Matches build up between periodic dumps, so typing "dump" clearly
        shows them, and "exit" can be tried mid-search.

The delay is only compiled in by the "slow" target
(-DFILE_FINDER_TEST_DELAY) and is not present in the normal build.


Files
--------------------------------------------------------------------------

    main.c          argument checks, thread creation, console commands,
                    cleanup
    file_finder.c   shared container, recursive search, search and dumper
                    threads
    file_finder.h   shared types and function declarations
    Makefile        normal build, "slow" test build and clean

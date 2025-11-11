/**
 * pci_bar_dump_test.c
 *
 * Simple user-space test application to interact with the pci_bar_dump kernel driver.
 *
 * Author: Kushal Bandi
 * License: GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROC_CONTROL_PATH "/proc/pci_bar_dump_control"
#define PROC_DUMP_PATH    "/proc/pci_bar_dump"

void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s <BDF> <BAR> <OFFSET_HEX> <DUMP_SIZE_BYTES> [output_file]\n", prog);
    printf("\nExample:\n");
    printf("  sudo %s 0000:01:00.0 0 0x0 128 dump.dat\n", prog);
    printf("\nIf no output_file is provided, dump is printed to stdout.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        print_usage(argv[0]);
        return 1;
    }

    const char *bdf = argv[1];
    const char *bar = argv[2];
    const char *offset = argv[3];
    const char *dump_size = argv[4];
    const char *output_file = (argc >= 6) ? argv[5] : NULL;

    char command[256];
    snprintf(command, sizeof(command), "%s %s %s %s", bdf, bar, offset, dump_size);

    FILE *ctl = fopen(PROC_CONTROL_PATH, "w");
    if (!ctl) {
        perror("Failed to open /proc/pci_bar_dump_control for writing");
        return 1;
    }

    fprintf(ctl, "%s\n", command);
    fclose(ctl);

    printf("[INFO] Configuration written: %s\n", command);
    printf("[INFO] Reading dump from %s...\n\n", PROC_DUMP_PATH);

    FILE *dump = fopen(PROC_DUMP_PATH, "r");
    if (!dump) {
        perror("Failed to open /proc/pci_bar_dump for reading");
        return 1;
    }

    FILE *out = stdout;
    if (output_file) {
        out = fopen(output_file, "w");
        if (!out) {
            perror("Failed to open output file");
            fclose(dump);
            return 1;
        }
        printf("[INFO] Writing dump to %s\n", output_file);
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), dump)) {
        fputs(buffer, out);
    }

    if (output_file)
        fclose(out);

    fclose(dump);
    printf("\n[INFO] Dump completed successfully.\n");

    return 0;
}

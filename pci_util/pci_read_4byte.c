#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

// Extract BAR0 base address from /sys/bus/pci/devices/.../resource
unsigned long get_bar0_base(const char *resource_dir) {
    char path[256];
    snprintf(path, sizeof(path), "%s/resource", resource_dir);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("Failed to open BAR resource file");
        exit(EXIT_FAILURE);
    }

    unsigned long start, end, flags;
    if (fscanf(fp, "%lx %lx %lx", &start, &end, &flags) != 3) {
        fprintf(stderr, "Failed to parse BAR0 base address\n");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    return start;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <resource0_path> <offset_in_hex>\n", argv[0]);
        return 1;
    }

    const char *resource0_path = argv[1];
    off_t offset = strtoul(argv[2], NULL, 0);

    // Extract directory to read BAR0 base
    char resource_dir[256];
    strncpy(resource_dir, resource0_path, sizeof(resource_dir));
    char *last_slash = strrchr(resource_dir, '/');
    if (last_slash) *last_slash = '\0';

    unsigned long bar0_base = get_bar0_base(resource_dir);
    unsigned long phys_addr = bar0_base + offset;

    int fd = open(resource0_path, O_RDONLY | O_SYNC);
    if (fd < 0) {
        perror("Failed to open BAR0 resource file");
        return 1;
    }

    void *map_base = mmap(0, MAP_SIZE, PROT_READ, MAP_SHARED, fd, offset & ~MAP_MASK);
    if (map_base == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return 1;
    }

    void *virt_addr = (char *)map_base + (offset & MAP_MASK);
    uint32_t value = *((volatile uint32_t *)virt_addr);

    printf("BAR0 Base Address       : 0x%08lX\n", bar0_base);
    printf("Effective Address       : 0x%08lX\n", phys_addr);
    printf("4-Byte Register Value   : 0x%08X\n", value);

    munmap(map_base, MAP_SIZE);
    close(fd);
    return 0;
}

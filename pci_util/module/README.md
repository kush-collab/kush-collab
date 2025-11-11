```markdown
# PCI BAR Dump Kernel Module

## Overview

`pci_bar_dump` is a Linux kernel module designed to **inspect and dump PCI Base Address Register (BAR) memory contents** directly from kernel space.  
It provides a **safe, user-configurable interface via `/proc/pci_bar_dump_control`** to dynamically specify:
- **Bus/Device/Function (BDF)** — e.g., `0000:01:00.0`
- **BAR number (0–5)**
- **Offset** (in bytes)
- **Dump size** (in bytes, default 64B)

The module outputs the **BAR memory region** in a format similar to `hexdump -C` via `/proc/pci_bar_dump`.

---

## Key Features

✅ Accesses PCI BAR memory safely using kernel APIs (`pci_iomap`, `ioread32`)  
✅ Dynamically configurable via `/proc/pci_bar_dump_control`  
✅ Produces human-readable **hex + ASCII** dump (like `hexdump -C`)  
✅ Includes **driver versioning** and metadata  
✅ Supports **runtime reload** without recompilation

---

## Driver Version

**Version:** 1.1.0  
**Author:** Kushal Bandi  
**License:** GPL v2  
**Purpose:** Debug, analyze, or inspect PCIe device memory-mapped registers directly from kernel space.

---

## Directory Structure

```

pci_bar_dump/
├── Makefile
├── pci_bar_dump.c
└── README.md

````

---

## Build Instructions

Ensure you have kernel headers installed for your current kernel version:

```bash
sudo apt install linux-headers-$(uname -r)
````

Then build the module:

```bash
make
```

You should see an output file:

```
pci_bar_dump.ko
```

---

## Load and Verify the Module

### Insert the module:

```bash
sudo insmod pci_bar_dump.ko
```

### Verify module is loaded:

```bash
lsmod | grep pci_bar_dump
```

### Check kernel logs:

```bash
dmesg | tail
```

You should see:

```
[pci_bar_dump] Module loaded (v1.1.0)
```

---

## Usage Instructions

### 1. Check the control interface

When loaded, the module creates two `/proc` entries:

```
/proc/pci_bar_dump_control
/proc/pci_bar_dump
```

You can view current configuration:

```bash
cat /proc/pci_bar_dump_control
```

Example output:

```
BDF: 0000:01:00.0
BAR: 0
Offset: 0x0
Dump Size: 64 bytes
```

---

### 2. Update Configuration Dynamically

To modify BDF, BAR, offset, or dump size:

```bash
echo "0000:01:00.0 0 0x100 128" | sudo tee /proc/pci_bar_dump_control
```

**Format:**

```
<BDF> <BAR> <OFFSET_HEX> <DUMP_SIZE_BYTES>
```

If `dump_size` is not provided, default = **64 bytes**.

---

### 3. Dump BAR Memory Contents

Once configured:

```bash
sudo cat /proc/pci_bar_dump
```

Example output:

```
00000000  01 23 45 67 89 ab cd ef  10 32 54 76 98 ba dc fe  |.3Eg.....2Tv.....|
00000010  11 22 33 44 55 66 77 88  99 aa bb cc dd ee ff 00  |."3DUfw.........|
00000020  00 11 22 33 44 55 66 77  88 99 aa bb cc dd ee ff  |.."3DUfw.........|
00000030  ff ee dd cc bb aa 99 88  77 66 55 44 33 22 11 00  |........wfUD3"..|
```

---

### 4. Save Dump to File

You can redirect the dump to a file:

```bash
sudo cat /proc/pci_bar_dump > dump.dat
```

This file will be in the **hexdump -C style** for easy offline analysis.

---

### 5. Unload the Module

```bash
sudo rmmod pci_bar_dump
```

Check kernel logs:

```bash
dmesg | tail
```

Output:

```
[pci_bar_dump] Module unloaded.
```

---

## Example Session

```bash
# Build and insert the module
make
sudo insmod pci_bar_dump.ko

# Configure for device 0000:01:00.0, BAR0, offset 0x0, 128 bytes
echo "0000:01:00.0 0 0x0 128" | sudo tee /proc/pci_bar_dump_control

# Dump to screen
sudo cat /proc/pci_bar_dump

# Save to file
sudo cat /proc/pci_bar_dump > bar0_dump.dat

# Unload module
sudo rmmod pci_bar_dump
```

---

## Error Handling

| Error Message                          | Possible Cause                       | Solution                                     |
| -------------------------------------- | ------------------------------------ | -------------------------------------------- |
| `mmap failed: Operation not permitted` | Attempting user-space mapping of BAR | Use kernel-space access only via this driver |
| `Invalid BDF or BAR`                   | Provided BDF not found               | Verify with `lspci`                          |
| `ioremap failed`                       | Invalid BAR mapping or access denied | Check permissions or use smaller dump size   |
| `read failed`                          | Offset beyond BAR size               | Reduce dump size or adjust offset            |

---

## Dynamic Reload Support (via /proc)

The `/proc/pci_bar_dump_control` interface allows **live reconfiguration** without rebuilding or reloading the module:

* Update parameters at runtime.
* Immediately re-read `/proc/pci_bar_dump` for new dump.
* No `rmmod` / `insmod` required between dumps.

Example:

```bash
echo "0000:02:00.0 2 0x200 256" | sudo tee /proc/pci_bar_dump_control
sudo cat /proc/pci_bar_dump
```

---

## Notes

* For **64KB or larger** dumps, ensure BAR memory is accessible and valid.
* Reading from unmapped or restricted BAR regions may produce kernel logs or truncated output.
* Only **read operations** are performed — the driver never writes to BAR memory.

---

## License

```
GPL v2
Copyright (C) 2025
Author: Kushal Bandi
```

---

## Summary

This tool provides a **robust and kernel-safe way to inspect PCI device BAR memory** for diagnostics, hardware bring-up, or firmware validation — similar in purpose to `devmem` or `lspci -xxxx`, but with **direct kernel access and full control over BDF, BAR, offset, and dump size.**

---

*End of README*

```

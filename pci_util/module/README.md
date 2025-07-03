# pci_bar_dump

`pci_bar_dump` is a Linux kernel module that dumps PCI BAR0 (Base Address Register) memory-mapped register values of a given PCI device to a `/proc` entry. It formats the output similar to `hexdump -C`, showing both hexadecimal and ASCII representations of the register values.

---

## 🔧 Features

- Accepts runtime parameters:
  - PCI BDF (Bus:Device.Function) address
  - Offset within BAR0
  - Number of bytes to dump
- Maps BAR0 using `ioremap`
- Formats output in `hex + ASCII` (hexdump -C style)
- Outputs to `/proc/pci_bar_dump`
- Prevents over-read with bounds checks
- Versioned (`v1.0`)

---

## 📦 Requirements

- Linux kernel and headers installed
- Root privileges to insert/remove modules
- Target PCI device must expose a valid memory BAR (BAR0)

---

## 🛠️ Build Instructions

### Clone and build the module

```bash
make
```

## 🚀 Usage

### Insert the Module

```bash
sudo insmod pci_bar_dump.ko bdf=0000:11:00.0 offset=0x09a0000 dump_size=128
```

bdf: PCI Bus-Device-Function (e.g. 0000:01:00.0)
offset: Byte offset inside BAR0 from where to start the dump
dump_size: Total number of bytes to read (default is 64 bytes)

⚠️ Make sure offset + dump_size does not exceed the BAR0 size

### Read the Output

The dump is made available in:

```bash
cat /proc/pci_bar_dump
```

Or redirect to a file:

```bash
cat /proc/pci_bar_dump > pci_dump.dat
```

### Sample Output Format

```r
00000100  48 65 6c 6c 6f 20 57 6f  72 6c 64 21 00 00 00 00  |Hello World!....|
00000110  01 23 45 67 89 ab cd ef  ff ee dd cc bb aa 99 88  |.#Eg............|
```

### Unload the Module

```bash
sudo rmmod pci_bar_dump
```

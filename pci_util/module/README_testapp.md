---

## 🧰 Build Instructions

Save as `pci_bar_dump_test.c` in the same folder as your kernel module, then compile it:

```bash
gcc -O2 -o pci_bar_dump_test pci_bar_dump_test.c
```

---

## ▶️ Example Usage

### Dump 128 bytes from BAR0, offset 0x0:

```bash
sudo ./pci_bar_dump_test 0000:01:00.0 0 0x0 128
```

### Dump 256 bytes from BAR2, offset 0x200 and save to file:

```bash
sudo ./pci_bar_dump_test 0000:02:00.0 2 0x200 256 bar2_dump.dat
```

---

## 💡 Expected Output (stdout example)

```
[INFO] Configuration written: 0000:01:00.0 0 0x0 128
[INFO] Reading dump from /proc/pci_bar_dump...

00000000  01 23 45 67 89 ab cd ef  10 32 54 76 98 ba dc fe  |.#Eg.....2Tv.....|
00000010  11 22 33 44 55 66 77 88  99 aa bb cc dd ee ff 00  |."3DUfw.........|
...
[INFO] Dump completed successfully.
```

---

## 🧩 Summary

This tool automates:

* Writing config values into `/proc/pci_bar_dump_control`
* Reading the resulting hexdump from `/proc/pci_bar_dump`
* Saving it optionally to a file for offline inspection

It acts as a **lightweight front-end utility** for your kernel driver, similar to how tools like `ethtool` or `devmem2` interact with kernel interfaces.

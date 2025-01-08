# Dmesg Parser

## Overview
`Dmesg Parser` is a C++ program designed to parse logs obtained from the `dmesg` command or a specified log file. It matches the log lines against a set of user-defined regex patterns provided in a file and outputs any matching lines.

## Features
- Load regex patterns from a file to match against log lines.
- Parse logs from a specified log file or directly from the output of the `dmesg` command.
- Pass arguments to the `dmesg` command for more refined log retrieval.
- Command-line options for flexibility and usability.

## Compilation
To compile the program, ensure you have a C++ compiler installed (e.g., `g++`) and run the following command:

```bash
g++ -o dmesg_parser dmesg_parser.cpp
```

## Usage
Run the program with the following command-line options:

```bash
./dmesg_parser [options]
```

### Options
| Option             | Description                                                                 |
|--------------------|-----------------------------------------------------------------------------|
| `-p <patterns_file>` | Specify the file containing regex patterns. Default: `patterns.txt`.       |
| `-l <log_file>`      | Specify a log file to parse instead of running the `dmesg` command.       |
| `-a <dmesg_args>`    | Pass additional arguments to the `dmesg` command.                        |
| `-h`                | Display this help message describing the program options.                 |

### Examples
1. **Parse using default patterns file and `dmesg` command:**
   ```bash
   ./dmesg_parser
   ```

2. **Specify a custom patterns file:**
   ```bash
   ./dmesg_parser -p my_patterns.txt
   ```

3. **Parse a specific log file:**
   ```bash
   ./dmesg_parser -p patterns.txt -l /var/log/dmesg
   ```

4. **Pass arguments to the `dmesg` command:**
   ```bash
   ./dmesg_parser -p patterns.txt -a "-T"
   ```

5. **Display help message:**
   ```bash
   ./dmesg_parser -h
   ```

## Requirements
- A C++17-compatible compiler.
- The `dmesg` command must be available in your system's PATH if using live logs.

## Patterns File Format
The patterns file should contain one regex pattern per line. For example:

```
.*error.*
.*warning.*
.*usb.*
.*memory.*
```

## License
This program is released under the MIT License. See `LICENSE` for more details.

## Author
Kushal Bandi


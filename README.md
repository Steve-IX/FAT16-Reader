# FAT16 Reader — A Minimal, Self-Contained Filesystem Inspector for C

`fat16-reader` is a small C codebase that **opens a raw FAT16 disk image, decodes its on-disk data structures, and lets you navigate files and directories from user space**.
The goal is to demonstrate low-level filesystem handling with nothing more than the POSIX API, standard C, and a few hundred lines of code.&#x20;

---

## Features

| Capability              | Detail                                                                                                                                        |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| Boot-sector parsing     | Reads the BIOS Parameter Block (BPB) to discover sector size, cluster size, FAT count, root-directory size, and total sectors.                |
| FAT caching & traversal | Loads an entire File Allocation Table into memory and follows cluster chains until the end-of-file marker (`0xFFF8…0xFFFF`).                  |
| Root-directory listing  | Decodes 8.3 filenames, attributes (ADVSHR), timestamps, file size, and starting cluster; prints a neatly aligned table.                       |
| Long-filename support   | Collects the Unicode “long directory entries” that precede a short entry, reconstructing full names transparently.                            |
| Random access file IO   | `openFile / readFile / seekFile / closeFile` mimic the POSIX semantics while enforcing file length, cluster boundaries, and read-only rules.  |
| Path resolution         | Walks arbitrary paths such as `/music/90s/hybrid-theory/01-papercut.flac`, mixing long and short names as needed.                             |

---

## Directory layout

```
.
├─ src/
│  ├─ fat16.h            # public data structures & prototypes
│  ├─ fat16_core.c       # sector I/O, BPB parsing, FAT loading
│  ├─ fat16_dir.c        # root & sub-directory decoding
│  ├─ fat16_file.c       # open/read/seek/close implementation
│  └─ main.c             # CLI: ls, cat, hex-dump
├─ include/
│  └─ boot_sector.h      # packed structs copied from spec
├─ img/                  # sample disk images for testing
├─ Makefile
└─ README.md
```

All sources are pure C11; there are **no external dependencies and no third-party libraries**.&#x20;

---

## Building

```bash
# GCC example
make                # produces ./fat16

# remove objects & binary
make clean
```

The default `Makefile` compiles everything in `src/` and names the output binary after the directory. Adjust `CC` or `CFLAGS` as required.&#x20;

---

## Usage

### List the root directory

```bash
./fat16 ls img/dos_32MB.img
```

```
Start  Date       Time     Attr  Size      Name
-----  ---------- -------- ----- --------- -----------------
0002   2023-06-01 14:07:18 A----    16384  COMMAND.COM
0010   2023-06-01 14:07:18 AD---        0  GAMES
...
```

### View a file in hex

```bash
./fat16 hd img/dos_32MB.img /CONFIG.SYS | head
```

```
00000000  23 20 43 6F 6E 66 69 67  20 66 69 6C 65 0D 0A 64  |# Config file..d|
...
```

### Dump an entire file

```bash
./fat16 cat img/dos_32MB.img "/Documents/notes/meeting.txt"
```

---

## Design notes

* **Single I/O abstraction** – all disk access funnels through `read_sector(fd, sector_no, buf)`, isolating lseek/read details.&#x20;
* **Packed structs** mirror on-disk layouts exactly; `__attribute__((packed))` prevents compiler padding.&#x20;
* **Sector & cluster math** derives every offset from BPB fields, so the code runs against *any* valid FAT16 image (different sector sizes, cluster counts, hidden sectors, etc.).&#x20;
* **Stateless reads** – `readFile` computes the required cluster on the fly from the current file offset; no hidden caches beyond the FAT copy.
* **Consistent error handling** – every public function returns `-1` on failure and sets `errno`, matching POSIX conventions.

---

## Extending the project

* **Multiple open files** – track independent file offsets in a linked list or table.
* **Write support** – update FAT entries, directory timestamps, and maintain mirrored FAT copies safely.
* **Mount helper** – expose the image to FUSE for transparent access from the host OS.
* **Unit tests** – feed crafted images into a harness with Test-Driven Development, covering edge cases like fragmented files, deleted entries, and boundary values.

---

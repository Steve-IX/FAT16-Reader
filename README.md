# FAT16-Explorer – Minimal Filesystem Inspector in ISO C

FAT16-Explorer is a self-contained study of the FAT16 disk format implemented in portable C 11.
It opens an uncompressed disk image, decodes every on-disk structure, and lets you walk directories, follow cluster chains, and read files without relying on any external libraries or FUSE bindings.

---

## Why this exists

* **Hands-on learning.** All code paths map 1-to-1 to the fields described in the FAT specification.
* **Small surface.** Fewer than 1 000 effective lines; easy to step through in a debugger.
* **Progressive layers.** Each source file adds exactly one capability, so you can follow the evolution from a single‐sector hex dump to a fully navigable filesystem.

---

## Project layout

```
Operating-Systemss/
├── fat.c          # step 1: read an arbitrary sector
├── fat2.c         # step 2: parse the BIOS Parameter Block (boot sector)
├── fat3.c         # step 3: load a complete FAT and follow cluster chains
├── fat4.c         # step 4: list the root directory with timestamps & attributes
├── fat5.c         # step 5: read-only POSIX-like API (open/read/seek/close)
├── fat6.c         # step 6: reconstruct long file names (VFAT entries)
├── fat7.c         # step 7: resolve nested paths
├── final.c        # consolidated demo combining every feature above
├── fat16.img      # sample 32 MB image (MS-DOS formatted)
├── Makefile       # convenience targets for GCC or Clang
└── docs/          # packed structs and offset cheat-sheets
```

---

## Building

```bash
# quickest route – compile the full demo
gcc -std=c11 -Wall -Wextra -pedantic -o fat16 final.c
# or build individual stages
gcc -o fat4 fat4.c
```

The code is strictly POSIX; it builds on Linux, macOS, and the \*BSDs with the system compiler.

---

## Running the demo

`final.c` is wired to the bundled `fat16.img` by default:

```bash
./fat16
```

It prints

* Boot-sector/BPB fields (bytes-per-sector, sectors-per-cluster, number-of-FATs, etc.).
* A neatly aligned root-directory listing that mixes 8 .3 and long file names.
* The full cluster chain for every regular file.
* A hexdump of the first data cluster of each file.

To inspect a different image, change the `const char *filePath` at the top of `final.c` or pass your own path when compiling:

```bash
gcc -DBOOT_IMAGE=\"~/images/msdos.img\" -o fat16 final.c
```

---

## Inside the code

| Layer           | Key idea                                                                                                                                   | Where to look                            |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------- |
| Sector I/O      | *Single abstraction* `read_sector(fd, n, buf)` hides `lseek`-plus-`read`.                                                                  | `fat.c`, `fat2.c`                        |
| Metadata        | Packed C structs mirror the BPB, directory entries, and long-name records byte-for-byte.                                                   | `fat2.c`, `fat4.c`, `docs/boot_sector.h` |
| FAT traversal   | The cluster number read from a directory entry is chased through the in-memory FAT until an end-of-file marker (0xFFF8–0xFFFF) is reached. | `fat3.c`                                 |
| Read-only API   | `openFile`, `readFile`, `seekFile`, `closeFile` enforce cluster boundaries and file length while presenting familiar POSIX semantics.      | `fat5.c`                                 |
| Long file names | Consecutive VFAT entries are re-assembled into UTF-16, converted to wchar-t, then matched against user paths.                              | `fat6.c`, `fat7.c`                       |

Every public function returns `-1` on error and sets `errno`, so callers can integrate the code into larger projects without surprises.

---

## Extending the tool

* **Multiple concurrent files** – store per-file state in a linked list instead of a single global pointer.
* **Write support** – allocate free clusters, update both FAT copies, and patch directory timestamps.
* **FUSE front-end** – expose the reader through a FUSE mountpoint for transparent browsing from the host OS.
* **Automated tests** – craft edge-case images (fragmented files, deleted entries, non-standard BPB values) and validate behaviour under CI.

---

# SFS-FAT12 — Simple File System Utility

> A suite of C utilities for low-level manipulation of FAT12 disk images, implementing the full read/write lifecycle of an MS-DOS file system from scratch.

---

## Overview

This project provides a complete CLI toolset to interface with FAT12 `.IMA` disk images from a Linux environment. It involves direct binary parsing of disk structures — the BIOS Parameter Block, File Allocation Tables, and Root/Sub-directory entries — without any library abstractions.

The implementation emphasizes:

- **Correctness at the byte level** — manually unpacking 12-bit FAT entries that span byte boundaries, handling Little-Endian multi-byte integers, and reconstructing fragmented files via cluster chains
- **Recursive filesystem traversal** — walking arbitrarily deep subdirectory trees by following cluster pointers through the Data Region
- **Writeback integrity** — allocating clusters, updating FAT copies, writing directory entries, and syncing timestamps on `diskput` without corrupting existing data

---

## Utilities

### `diskinfo`
Parses the Boot Sector (BPB) to surface filesystem metadata.

```
OS Name:            MS-DOS
Label of the disk:  MYDISK
Total size:         1,474,560 bytes
Free size:          614,400 bytes
==============
Number of files:    42
=============
FAT copies:         2
Sectors per FAT:    9
```

### `disklist`
Recursively walks all directories, printing each entry with type, size, name, and decoded creation timestamp.

```
/
==================
F       4096  HELLO   .TXT  2024-03-15 14:22:00
D          0  DOCS             2024-03-10 09:00:00

/DOCS
==================
F      12884  REPORT  .PDF  2024-03-12 11:45:30
```

### `diskget`
Reconstructs a file from the disk image to the host filesystem by following its FAT cluster chain.

```bash
./diskget disk.IMA REPORT.PDF
# Writes REPORT.PDF to current directory
```

### `diskput`
Copies a Linux file into a specified path within the disk image — allocating clusters, updating both FAT copies, inserting a directory entry, and syncing the Linux `mtime` as the FAT12 creation/write timestamp.

```bash
./diskput disk.IMA /DOCS/NOTES/readme.txt
```

---

## Key Implementation Challenges

**12-bit FAT entry parsing** — FAT12 packs two 12-bit entries into every three bytes. Extracting even- vs. odd-indexed entries requires different nibble-shift logic and careful boundary handling.

**Endianness** — All multi-byte fields (sector counts, cluster numbers, file sizes) are stored Little-Endian. Every read and write goes through explicit byte-order handling rather than casting raw structs.

**Cluster chain reconstruction** — Files are read block-by-block by following the FAT linked list from the starting cluster until a terminal marker (`0xFF8–0xFFF`) is reached.

**Writeback consistency** — `diskput` must atomically update two FAT copies and insert a directory entry. Insufficient free space, missing subdirectory paths, and duplicate filenames are caught before any writes occur.

**Timestamp encoding** — FAT12 stores date and time in packed 16-bit fields (year offset from 1980, 2-second time resolution). `diskput` extracts `mtime` from `stat()` and re-encodes it into this format.

---

## Build & Usage

**Prerequisites:** GCC, Linux/Unix (or WSL), `make`

```bash
# Compile all utilities
make

# Inspect disk metadata
./diskinfo disk.IMA

# List all files recursively
./disklist disk.IMA

# Extract a file from the image
./diskget disk.IMA FILENAME.TXT

# Insert a file into a subdirectory
./diskput disk.IMA /SUBDIR/FILENAME.TXT
```

---

## File System Layout

```
┌─────────────────────────────┐
│  Boot Sector (Sector 0)     │  ← BPB: geometry, cluster size, FAT layout
├─────────────────────────────┤
│  FAT Region (×2 copies)     │  ← 12-bit linked cluster map
├─────────────────────────────┤
│  Root Directory Region      │  ← Fixed-size 32-byte entries
├─────────────────────────────┤
│  Data Region                │  ← File contents + subdirectory tables
└─────────────────────────────┘
```

---

## Technical Stack

| | |
|---|---|
| **Language** | C (C99) |
| **Memory access** | `mmap()` for efficient random I/O on the disk image |
| **Build system** | GNU Make |
| **Platform** | Linux / Unix |
| **Filesystem spec** | FAT12 (MS-DOS) |

---

## Academic Context

Developed for **CSc 360: Operating Systems** at the University of Victoria. The assignment targets manual memory management, system call error handling, and the kind of low-level data structure work that library abstractions normally hide.

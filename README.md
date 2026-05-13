SFS-FAT12: Simple File System Utility
A suite of C-based utilities designed to interface with and manipulate FAT12 file system images. This project implements low-level disk operations, including metadata extraction, directory traversal, and file I/O within a simulated MS-DOS environment.

🚀 Overview
This project provides a bridge between a Linux environment and a FAT12 disk image (.IMA). It involves direct binary manipulation of the disk's structures, including the BIOS Parameter Block (BPB), File Allocation Tables (FAT), and Root Directory entries.

Key Technical Challenges
Bit Manipulation: Implementing logic to parse 12-bit FAT entries (packed across byte boundaries).

Endianness: Handling Little-Endian data storage prevalent in Intel-based architectures.

Recursion: Developing deep-walk algorithms to traverse multi-layered subdirectories within the Data Area.

Memory Mapping: Utilizing mmap() for efficient random access to the disk image.

🛠️ Utilities
The suite consists of four primary tools:

1. diskinfo
Displays high-level metadata about the file system.

Metrics: OS Name, Label, Total/Free Disk Size.

Statistics: Total file count (recursive), number of FAT copies, and sectors per FAT.

2. disklist
A recursive directory listing tool.

Formats output to show file types (F for files, D for directories).

Displays file sizes, names, and creation timestamps formatted from raw FAT12 date/time bytes.

3. diskget
Retrieves a file from the disk image and copies it to the host Linux system.

Verifies file existence in the root directory.

Performs block-by-block reconstruction using the File Allocation Table.

4. diskput
Copies a file from the host Linux system into a specified directory in the disk image.

Dynamic Allocation: Updates the FAT and directory entries.

Validation: Checks for existing filenames, directory paths, and sufficient disk space.

Timestamp Sync: Synchronizes Linux "Last Modified" time with FAT12 creation metadata.

📂 File System Structure
The implementation relies on an intimate understanding of the FAT12 layout:

Reserved Region: Boot Sector (Sector 0).

FAT Region: Two copies of the File Allocation Table.

Root Directory Region: Fixed-size entries following the FATs.

Data Region: Clusters containing file data and subdirectory tables.

⚙️ Build & Usage
Prerequisites
GCC Compiler

Linux/Unix environment (or WSL)

xxd (for raw binary debugging)

Compilation
Build all utilities using the provided Makefile:

Bash
make
Execution
Bash
# Get disk statistics
./diskinfo disk.IMA

# List all files and subdirectories
./disklist disk.IMA

# Copy a file from the image to Linux
./diskget disk.IMA FILE.PDF

# Copy a file from Linux into a specific subdirectory in the image
./diskput disk.IMA /SUBDIR1/SUBDIR2/NEWFILE.TXT

🎓 Academic Context
Developed as part of CSc 360: Operating Systems at the University of Victoria. The project emphasizes the importance of system calls, error handling, and manual memory management in C.

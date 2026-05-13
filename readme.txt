========================================================================
CSC 360 - Operating Systems (Spring 2026)
Assignment 3: A Simple File System (SFS)
========================================================================

Author: Rahil Wijeyesekera
Student ID: v01041863

------------------------------------------------------------------------
1. PROGRAMMING PROJECT OVERVIEW
------------------------------------------------------------------------
This project implements four command-line utilities written 
in C to manipulate and extract information from a FAT12 file system 
image (MS-DOS). 

------------------------------------------------------------------------
2. INCLUDED FILES
------------------------------------------------------------------------
- diskinfo.c : Reads the boot sector and FAT tables to display disk 
               metadata (Total size, free size, file count, FAT info).
               
- disklist.c : Recursively traverses the root and subdirectories to 
               display all files/folders with their types, sizes, and 
               creation timestamps.
               
- diskget.c  : Extracts a specified file from the FAT12 image and 
               copies it to the local Linux directory.
               
- diskput.c  : Injects a local Linux file into the FAT12 image at a 
               specified path, updating the FAT tables, allocating 
               clusters, and syncing the Linux modification timestamp.
               
- Makefile   : Compiles all executables and handles cleanup.

- readme.txt : This documentation file.

------------------------------------------------------------------------
3. INSTRUCTIONS
------------------------------------------------------------------------
To compile all utilities, run the following command in the terminal:
    $ make

To remove all compiled executables, run:
    $ make clean

------------------------------------------------------------------------
4. USAGE INSTRUCTIONS
------------------------------------------------------------------------
All executables require the disk image (e.g., disk.IMA) as the first 
argument.

Part I - diskinfo:
    $ ./diskinfo disk.IMA

Part II - disklist:
    $ ./disklist disk.IMA

Part III - diskget:
    $ ./diskget disk.IMA <filename>
    (e.g., ./diskget disk.IMA ANS1.PDF)

Part IV - diskput:
    $ ./diskput disk.IMA <file_to_insert>
    (e.g., ./diskput disk.IMA foo.txt)
    
    Or, to insert into a specific subdirectory:
    $ ./diskput disk.IMA /SUBDIR1/SUBDIR2/foo.txt


- Code was successfully tested and verified on linux.csc.uvic.ca.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 512
#define DIR_SIZE 32

// Global variables to store file system information extracted from the boot sector
int bytes_per_sector;
int sectors_per_cluster;
int reserved_sectors;
int num_fats;
int sectors_per_fat;
int max_root_entries;
int total_sectors;
int root_start;
int data_start;
int file_count = 0;

// Recursive function to scan directories and count files
// It reads directory entries and checks if they are files or subdirectories
// If it's a subdirectory, it calls itself to scan that subdirectory, otherwise it increments the file count

void scan_directory(FILE *fp, int offset)
{
    unsigned char entry[DIR_SIZE];

    // Loop through directory entries until we reach an empty entry (0x00) or a deleted entry (0xE5)
    while(1)
    {
        fread(entry, DIR_SIZE, 1, fp);

        if(entry[0] == 0x00) // Empty entry, end of directory~!
            break;

        if(entry[0] == 0xE5) // Deleted entry, skip it
            continue;

        int attr = entry[11]; // Attribute byte to determine if it's a file or directory

        // Get the starting cluster of the file/directory
        int cluster = entry[26] + (entry[27] << 8);

        // If the cluster number is 0 or 1, it's not valid, so we skip it
        if(cluster <= 1)
            continue;

        // If it's a directory, we need to scan it recursively
        if(attr == 0x10)
        {
            int subdir_offset = data_start + (cluster - 2) * sectors_per_cluster * bytes_per_sector; // Calculate the byte offset of the subdirectory

            fseek(fp, subdir_offset, SEEK_SET); // Move the file pointer to the start of the subdirectory

            scan_directory(fp, subdir_offset); // Recursively scan the subdirectory
        }
        
        else // If it's a file, we just increment the file count
        {
            file_count++;
        }
    }
}

// The main function takes the disk image file as a command-line argument,
// reads the boot sector to extract necessary information about the file system,
// and then calls the scan_directory function to count the number of files in the root directory and any subdirectories.
// It also calc and prints the total size of the disk and the free size based on the FAT entries.

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("Usage: ./diskinfo disk.IMA\n");
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");

    // Read the boot sector to extract file system information
    unsigned char boot[SECTOR_SIZE];
    fread(boot, SECTOR_SIZE, 1, fp);

    // Extract the OS name from the boot sector
    char os_name[9];
    memcpy(os_name, &boot[3], 8);
    os_name[8] = '\0';

    // Extract necessary information from the boot sector
    bytes_per_sector = boot[11] + (boot[12] << 8);
    sectors_per_cluster = boot[13];
    reserved_sectors = boot[14] + (boot[15] << 8);
    num_fats = boot[16];
    max_root_entries = boot[17] + (boot[18] << 8);
    sectors_per_fat = boot[22] + (boot[23] << 8);
    total_sectors = boot[19] + (boot[20] << 8);

    // Extract the label of the disk from the boot sector
    char label[12];
    memcpy(label, &boot[43], 11);
    label[11] = '\0';

    // Calculate the total size of the disk based on the total number of sectors and bytes per sector
    int total_size = total_sectors * bytes_per_sector;

    // Calculate the starting byte offset of the root directory and the data area
    root_start = (reserved_sectors + num_fats * sectors_per_fat) * bytes_per_sector;

    // Calculate the size of the root directory based on the maximum number of entries and the size of each entry
    int root_size = max_root_entries * DIR_SIZE;

    // Calculate the starting byte offset of the data area, which comes after the root directory
    data_start = root_start + root_size;

    printf("OS Name: %s\n", os_name);
    printf("Label of the disk: %s\n", label);
    printf("Total size of the disk: %d bytes\n", total_size);


    // Calculate the number of clusters and the FAT type (FAT12 or FAT16) based on the total number of sectors and the size of the root directory
    int root_sectors = (max_root_entries * DIR_SIZE + bytes_per_sector - 1) / bytes_per_sector;
    int data_sectors = total_sectors - reserved_sectors - num_fats * sectors_per_fat - root_sectors;
    int num_clusters = data_sectors / sectors_per_cluster;
    int fat_type = (total_sectors <= 4084) ? 12 : 16;
    int fat_start = reserved_sectors * bytes_per_sector;

    // Read the FAT into memory to calculate the free size of the disk based on the number of free clusters
    fseek(fp, fat_start, SEEK_SET);
    unsigned char *fat = malloc(sectors_per_fat * bytes_per_sector);
    
    fread(fat, sectors_per_fat * bytes_per_sector, 1, fp);
    
    // Loop through the FAT entries to count the number of free clusters (entries with value 0) and calculate the free size of the disk
    int free_count = 0;
    for(int i = 2; i < num_clusters + 2; i++) {
        int offset;
        int entry;
        // For FAT12, each entry is 12 bits, so we need to calculate the offset and extract the entry value accordingly
        // For FAT16, each entry is 16 bits, so we can directly calculate the offset and extract the entry value
        if (fat_type == 12) {
            offset = i * 3 / 2;
            if (i % 2 == 0) {
                entry = fat[offset] + ((fat[offset+1] & 0x0F) << 8);
            } else {
                entry = ((fat[offset] & 0xF0) >> 4) + (fat[offset+1] << 4);
            }
        } else { // FAT16
            offset = i * 2;
            entry = fat[offset] + (fat[offset+1] << 8);
        }
        if (entry == 0) free_count++;
    }

    // Calculate the free size of the disk based on the number of free clusters, sectors per cluster, and bytes per sector
    int free_size = free_count * sectors_per_cluster * bytes_per_sector;
    printf("Free size of the disk: %d bytes\n", free_size);
    free(fat);

    // Move the file pointer to the start of the root directory to begin scanning for files and subdirectories
    fseek(fp, root_start, SEEK_SET);

    scan_directory(fp, root_start);

    printf("==============\n");
    printf("Number of files: %d\n", file_count);
    printf("==============\n");
    printf("Number of FAT copies: %d\n", num_fats);
    printf("Sectors per FAT: %d\n", sectors_per_fat);

    fclose(fp);

    return 0;
}
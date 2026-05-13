#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 512
#define DIR_SIZE 32

int bytes_per_sector;
int sectors_per_cluster;
int reserved_sectors;
int num_fats;
int sectors_per_fat;
int max_root_entries;

int root_start;
int data_start;


// Convert FAT filename to normal string
void format_filename(unsigned char *entry, char *name)
{
    char fname[9];
    char ext[4];

    memcpy(fname, entry, 8);
    fname[8] = '\0';

    memcpy(ext, entry+8, 3);
    ext[3] = '\0';

    // Remove trailing spaces
    for(int i=7;i>=0;i--)
        if(fname[i]==' ') fname[i]='\0';
        else break;

    // Remove trailing spaces from extension
    for(int i=2;i>=0;i--)
        if(ext[i]==' ') ext[i]='\0';
        else break;

    if(strlen(ext)>0)
        sprintf(name,"%s.%s",fname,ext);
    else
        sprintf(name,"%s",fname);
}


/*  get_fat_entry: read a FAT entry */
int get_fat_entry(unsigned char *fat, int cluster)
{
    int offset = cluster * 3 / 2;

    int next;

    if(cluster % 2 == 0) // even cluster
        next = fat[offset] + ((fat[offset+1] & 0x0F) << 8);

    else // odd cluster
        next = ((fat[offset] & 0xF0) >> 4) + (fat[offset+1] << 4);

    return next;
}



int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        printf("Usage: ./diskget disk.IMA filename\n");
        return 1;
    }

    FILE *fp = fopen(argv[1],"rb");

    if(fp == NULL)
    {
        printf("Cannot open disk image\n");
        return 1;
    }

    // The target filename is the second command-line argument
    char *target = argv[2];

    // Read the boot sector to extract file system information
    unsigned char boot[SECTOR_SIZE];
    fread(boot,SECTOR_SIZE,1,fp);

    bytes_per_sector = boot[11] + (boot[12]<<8);
    sectors_per_cluster = boot[13];
    reserved_sectors = boot[14] + (boot[15]<<8);
    num_fats = boot[16];
    max_root_entries = boot[17] + (boot[18]<<8);
    sectors_per_fat = boot[22] + (boot[23]<<8);

    // Calculate the starting byte offset of the root directory and the data area
    root_start =
        (reserved_sectors +
        num_fats * sectors_per_fat)
        * bytes_per_sector;

    int root_size = max_root_entries * DIR_SIZE;

    data_start = root_start + root_size;


    unsigned char entry[DIR_SIZE];

    int found = 0;
    int start_cluster;
    int file_size;

    fseek(fp, root_start, SEEK_SET);

    // Loop through the root directory entries to find the target file
    for(int i=0;i<max_root_entries;i++)
    {
        fread(entry,DIR_SIZE,1,fp);

        if(entry[0]==0x00)
            break;

        if(entry[0]==0xE5)
            continue;

        char name[30];
        format_filename(entry,name);

        if(strcmp(name,target)==0)
        {
            found = 1;

            start_cluster = entry[26] + (entry[27]<<8);

            // File size is stored in the last 4 bytes of the directory entry
            file_size =
                entry[28] +
                (entry[29]<<8) +
                (entry[30]<<16) +
                (entry[31]<<24);

            break;
        }
    }

    if(!found)
    {
        printf("File not found.\n");
        fclose(fp);
        return 0;
    }


    // Calculate the starting byte offset of the FAT and read it into memory
    int fat_start = reserved_sectors * bytes_per_sector;

    // Read the FAT into memory to follow the cluster chain of the file
    unsigned char *fat = malloc(sectors_per_fat * bytes_per_sector);

    fseek(fp,fat_start,SEEK_SET);
    fread(fat,sectors_per_fat * bytes_per_sector,1,fp);


    FILE *out = fopen(target,"wb");

    int cluster = start_cluster;

    // Calculate the size of a cluster in bytes
    int cluster_size =
        sectors_per_cluster *
        bytes_per_sector;

    unsigned char *buffer = malloc(cluster_size);

    int remaining = file_size;

    // Follow the cluster chain of the file and write its contents to the output file
    while(cluster < 0xFF8)
    {
        int offset =
            data_start +
            (cluster-2) * cluster_size;

        fseek(fp,offset,SEEK_SET);

        fread(buffer,cluster_size,1,fp);

        int to_write;
        if (remaining < cluster_size)
            to_write = remaining;
        else
            to_write = cluster_size;

        fwrite(buffer,to_write,1,out);

        remaining -= to_write;

        if(remaining <= 0)
            break;

        cluster = get_fat_entry(fat,cluster);
    }

    free(buffer);
    free(fat);

    fclose(out);
    fclose(fp);

    return 0;
}
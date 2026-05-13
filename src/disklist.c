#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 512
#define DIR_SIZE 32

// Boot sector values
int bytes_per_sector;
int sectors_per_cluster;
int reserved_sectors;
int num_fats;
int sectors_per_fat;
int max_root_entries;
int total_sectors;

int root_start;
int data_start;


// Function to format filename + extension
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
    {
        if(fname[i]==' ') fname[i]='\0';
        else break;
    }

    for(int i=2;i>=0;i--)
    {
        if(ext[i]==' ') ext[i]='\0';
        else break;
    }

    if(strlen(ext) > 0)
        sprintf(name,"%s.%s",fname,ext);
    else
        sprintf(name,"%s",fname);
}


// Function to print date/time
void print_datetime(unsigned char *entry)
{
    int date = entry[16] + (entry[17] << 8);
    int time = entry[14] + (entry[15] << 8);

    int day = date & 0x1F;
    int month = (date >> 5) & 0x0F;
    int year = ((date >> 9) & 0x7F) + 1980;

    int minute = (time >> 5) & 0x3F;
    int hour = (time >> 11) & 0x1F;

    printf("%04d-%02d-%02d %02d:%02d",year,month,day,hour,minute);
}


// Recursive directory listing
void list_directory(FILE *fp, int offset, char *dirname)
{
    printf("\n%s\n", dirname);
    printf("==================\n");

    unsigned char entry[DIR_SIZE];

    fseek(fp, offset, SEEK_SET);

    while(1)
    {
        fread(entry, DIR_SIZE, 1, fp);

        if(entry[0] == 0x00)
            break;

        if(entry[0] == 0xE5)
            continue;

        int attr = entry[11];

        int cluster = entry[26] + (entry[27] << 8);

        // skip invalid clusters
        if(cluster <= 1)
            continue;

        char name[30];
        format_filename(entry,name);

        int size =
            entry[28] +
            (entry[29]<<8) +
            (entry[30]<<16) +
            (entry[31]<<24);

        char type;

        if(attr == 0x10)
            type = 'D';
        else
            type = 'F';

        printf("%c %10d %-20s ",type,size,name);
        print_datetime(entry);
        printf("\n");

        // If directory → recursively list
        if(attr == 0x10)
        {
            int subdir_offset =
                data_start +
                (cluster-2) *
                sectors_per_cluster *
                bytes_per_sector;

            list_directory(fp, subdir_offset, name);
        }
    }
}



int main(int argc, char *argv[])
{

    if(argc != 2)
    {
        printf("Usage: ./disklist disk.IMA\n");
        return 1;
    }

    FILE *fp = fopen(argv[1],"rb");

    if(fp == NULL)
    {
        printf("Cannot open disk image\n");
        return 1;
    }


    unsigned char boot[SECTOR_SIZE];
    fread(boot,SECTOR_SIZE,1,fp);


    bytes_per_sector = boot[11] + (boot[12]<<8);
    sectors_per_cluster = boot[13];
    reserved_sectors = boot[14] + (boot[15]<<8);
    num_fats = boot[16];
    max_root_entries = boot[17] + (boot[18]<<8);
    sectors_per_fat = boot[22] + (boot[23]<<8);
    total_sectors = boot[19] + (boot[20]<<8);


    root_start =
        (reserved_sectors +
        num_fats * sectors_per_fat)
        * bytes_per_sector;


    int root_size = max_root_entries * DIR_SIZE;

    data_start = root_start + root_size;


    // Start listing from ROOT
    list_directory(fp, root_start, "ROOT");


    fclose(fp);

    return 0;
}
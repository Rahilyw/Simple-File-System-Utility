#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>

/* FAT12 Boot Sector */
typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_sectors;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} BootSector;

/*  FAT12 Directory Entry */
typedef struct __attribute__((packed)) {
    uint8_t  name[8];               // 0
    uint8_t  ext[3];                // 8
    uint8_t  attr;                  // 11
    uint8_t  reserved;              // 12
    uint8_t  creation_time_tenths;  // 13
    uint16_t creation_time;         // 14
    uint16_t creation_date;         // 16
    uint16_t last_access_date;      // 18
    uint16_t first_cluster_high;    // 20
    uint16_t write_time;            // 22
    uint16_t write_date;            // 24
    uint16_t first_cluster;         // 26
    uint32_t file_size;             // 28
} DirEntry;

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20

/* Helpers*/
static BootSector bs;
static uint8_t   *disk = NULL;
static long       disk_size = 0;

/* Offsets computed from boot sector */
static uint32_t fat_start;       /* byte offset of FAT1 */
static uint32_t root_start;      /* byte offset of root directory */
static uint32_t data_start;      /* byte offset of data region */
static uint32_t total_clusters;  /* total number of clusters in the data region */

/* Function prototypes */
static uint32_t fat_offset(void)  { 
    return bs.reserved_sectors * bs.bytes_per_sector; 
}

static uint32_t root_offset(void) {
     return fat_offset() + bs.num_fats * bs.fat_size_sectors * bs.bytes_per_sector; 
}

static uint32_t data_offset(void) { 
        return root_offset() + bs.root_entry_count * 32; 
}

/* cluster → byte offset in disk image (clusters start at 2) */
static uint32_t cluster_to_offset(uint16_t cluster) {
    return data_offset() + (cluster - 2) * bs.sectors_per_cluster * bs.bytes_per_sector;
}

/* Read a 12-bit FAT entry */
static uint16_t fat_get(uint16_t cluster) {
    uint32_t offset = fat_start + (cluster * 3) / 2;
    uint16_t val = disk[offset] | (disk[offset + 1] << 8);
    return (cluster & 1) ? (val >> 4) : (val & 0x0FFF);
}

/* Write a 12-bit FAT entry to both FAT copies */
static void fat_set(uint16_t cluster, uint16_t value) {
    // Update both FAT copies
    for (int f = 0; f < bs.num_fats; f++) {
        
        uint32_t base   = (bs.reserved_sectors + f * bs.fat_size_sectors) * bs.bytes_per_sector;
        uint32_t offset = base + (cluster * 3) / 2;

        // For FAT12, each entry is 12 bits. We need to read the existing 16 bits at the offset, modify the relevant 12 bits, and write it back.
        if (cluster & 1) {
            disk[offset] = (disk[offset] & 0x0F) | ((value & 0x0F) << 4);
            disk[offset + 1] = (value >> 4) & 0xFF;

        // For even clusters, the 12-bit entry starts at the offset. For odd clusters, the 12-bit entry starts 4 bits into the offset.
        } else { 
            disk[offset] = value & 0xFF;
            disk[offset + 1] = (disk[offset + 1] & 0xF0) | ((value >> 8) & 0x0F);
        }
    }
}

/* Find a free cluster (returns 0 if none) */
static uint16_t find_free_cluster(uint16_t start_search) {

    // Start searching from the given cluster number to find a free cluster (FAT entry == 0x000)
    for (uint16_t c = start_search; c < (uint16_t)(total_clusters + 2); c++) {
        if (fat_get(c) == 0x000) return c; // free cluster
    }

    return 0; // no free cluster found
}

/* Count free clusters */
static uint32_t count_free_clusters(void) {
    // Loop through all FAT entries to count how many are free (value == 0x000)
    uint32_t count = 0;
    for (uint16_t c = 2; c < (uint16_t)(total_clusters + 2); c++) {
        if (fat_get(c) == 0x000){
            count++;
        }
    }
    return count;
}

/* Convert Unix time → FAT date/time pair */
static void unix_to_fat(time_t t, uint16_t *fat_date, uint16_t *fat_time) {
    struct tm *tm = localtime(&t);
    
    /* FAT date format: bits 15-9 = year (since 1980), bits 8-5 = month, bits 4-0 = day */
    uint16_t year = (uint16_t)(tm->tm_year - 80);
    uint16_t month = (uint16_t)(tm->tm_mon + 1);
    uint16_t day = (uint16_t)tm->tm_mday;
    *fat_date = (year << 9) | (month << 5) | day;
    
    /* FAT time format: bits 15-11 = hour, bits 10-5 = minute, bits 4-0 = second/2 */
    uint16_t hour = (uint16_t)tm->tm_hour;
    uint16_t minute = (uint16_t)tm->tm_min;
    uint16_t second = (uint16_t)(tm->tm_sec / 2);
    *fat_time = (hour << 11) | (minute << 5) | second;
}

/* Convert a path component to 8.3 format (space-padded, upper-case) */
static void to_83(const char *name, uint8_t out_name[8], uint8_t out_ext[3]) {
    int i;
    
    /* Initialize name and extension with spaces */
    for (i = 0; i < 8; i++)
        out_name[i] = ' ';
    for (i = 0; i < 3; i++)
        out_ext[i] = ' ';

    /* Find the dot that separates name from extension */
    const char *dot = strrchr(name, '.');
    
    /* Calculate length of filename part (before the dot) */
    int base_len = strlen(name);
    if (dot != NULL) {
        base_len = dot - name;
    }
    
    /* Limit to 8 characters for filename */
    if (base_len > 8) 
        base_len = 8;

    /* Copy filename, converting to uppercase */
    for (i = 0; i < base_len; i++) {
        out_name[i] = toupper((unsigned char)name[i]);
    }

    /* Copy extension if it exists */
    if (dot != NULL) {
        const char *ext_start = dot + 1;
        for (i = 0; i < 3 && ext_start[i] != '\0'; i++) {
            out_ext[i] = toupper((unsigned char)ext_start[i]);
        }
    }
}

/* Compare a dir entry name/ext against an 8.3 name */
static int entry_matches(DirEntry *e, uint8_t name[8], uint8_t ext[3]) {
    return memcmp(e->name, name, 8) == 0 && memcmp(e->ext, ext, 3) == 0;
}

/* ─── Directory helpers ─────────────────────────────────────────────────── */

/*
 * Iterate over directory entries of a directory.
 * For the root directory, cluster == 0 (special).
 * Returns pointer to the entry array start and sets *count to number of entries.
 * The returned pointer is directly into 'disk'.
 */
static DirEntry *dir_entries(uint16_t cluster, uint32_t *count) {
    if (cluster == 0) {
        /* root directory */
        *count = bs.root_entry_count;
        return (DirEntry *)(disk + root_start);
    } else {
        uint32_t bytes_per_cluster = bs.sectors_per_cluster * bs.bytes_per_sector;
        /* Count cluster chain length */
        uint32_t chain = 0;
        uint16_t c = cluster;

        // Follow the FAT chain until we reach end-of-chain (FAT entry >= 0xFF8)
        while (c < 0xFF8) { 
            chain++; c = fat_get(c); 
        }
        *count = chain * bytes_per_cluster / 32; // Each directory entry is 32 bytes
        return (DirEntry *)(disk + cluster_to_offset(cluster));
    }
}

/*
 * Find a directory entry by 8.3 name within a directory cluster.
 * Returns pointer to entry or NULL.
 */
static DirEntry *find_entry(uint16_t dir_cluster, uint8_t name[8], uint8_t ext[3]) {
    uint32_t count;
    DirEntry *entries = dir_entries(dir_cluster, &count);

    // Loop through directory entries to find a match for the given name and extension
    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].name[0] == 0x00) break;          /* no more entries */
        if ((uint8_t)entries[i].name[0] == 0xE5) continue; /* deleted */
        if (entry_matches(&entries[i], name, ext)) return &entries[i];
    }
    return NULL;
}

/*
 * Find a free directory slot in a directory cluster.
 * Returns pointer to slot or NULL.
 */
static DirEntry *find_free_slot(uint16_t dir_cluster) {
    uint32_t count;
    DirEntry *entries = dir_entries(dir_cluster, &count);

    // Loop through directory entries to find a free slot (entry with name[0] == 0x00 or 0xE5)
    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5)
            return &entries[i];
    }
    return NULL;
}

/* ─── Path parser ───────────────────────────────────────────────────────── */

/*
 * Walk a path like /subdir1/subdir2/foo.txt.
 * Sets *dir_cluster to the cluster of the containing directory.
 * Sets filename[] to the filename component.
 * Returns 1 on success, 0 if a directory component is not found.
 */
static int resolve_path(const char *path, uint16_t *dir_cluster, char filename[13]) {

    // Make a  copy of the path for tokenization!
    char tmp[256];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    /* Strip leading slash */
    char *p = tmp;
    if (*p == '/') p++;

    *dir_cluster = 0; /* start at root */

    /* Tokenise on '/' */
    char *token = strtok(p, "/");

    if (!token) {
        /* empty path → root, no filename */
        filename[0] = '\0';
        return 1;
    }

    /* Collect tokens */
    char tokens[64][256];
    int  ntok = 0;
    while (token) {
        strncpy(tokens[ntok++], token, 255);
        token = strtok(NULL, "/");
    }

    /* All tokens except the last are directory components */
    for (int i = 0; i < ntok - 1; i++) {
        uint8_t n[8], e[3];
        to_83(tokens[i], n, e);
        DirEntry *ent = find_entry(*dir_cluster, n, e);
        if (!ent || !(ent->attr & ATTR_DIRECTORY)) return 0;
        *dir_cluster = ent->first_cluster;
    }

    /* Last token is the filename */
    strncpy(filename, tokens[ntok - 1], 12);
    filename[12] = '\0';
    return 1;
}

/* ─── Main ──────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk.IMA> <path/to/file>\n", argv[0]);
        return 1;
    }

    const char *disk_path = argv[1];
    const char *dest_path = argv[2]; /* e.g.  /subdir1/foo.txt  or  foo.txt */

    /* ── 1. Open the source file from the current Linux directory ── */

    /* The filename to look for locally is the last component of dest_path */
    const char *slash = strrchr(dest_path, '/');
    const char *local_name = slash ? slash + 1 : dest_path;

    FILE *src = fopen(local_name, "rb");
    if (!src) {
        
        printf("File not found.\n");
        return 1;
    }

    /* Get source file size and last-write time */
    struct stat st;
    if (stat(local_name, &st) != 0) {
        printf("File not found.\n");
        fclose(src);
        return 1;
    }
    uint32_t file_size = (uint32_t)st.st_size;
    time_t   mtime     = st.st_mtime;

    /* Read source file into buffer */
    uint8_t *file_data = NULL;
    if (file_size > 0) {
        file_data = (uint8_t *)malloc(file_size);
        if (!file_data) { fprintf(stderr, "Out of memory.\n"); fclose(src); return 1; }
        if (fread(file_data, 1, file_size, src) != file_size) {
            fprintf(stderr, "Read error.\n"); fclose(src); free(file_data); return 1;
        }
    }
    fclose(src);

    /* 2. Load the disk image */
    FILE *disk_fp = fopen(disk_path, "r+b");
    if (!disk_fp) {
        fprintf(stderr, "Error: Cannot open disk image: %s\n", disk_path);
        free(file_data);
        return 1;
    }

    fseek(disk_fp, 0, SEEK_END);
    disk_size = ftell(disk_fp);
    rewind(disk_fp);

    disk = (uint8_t *)malloc(disk_size);
    if (!disk) { fprintf(stderr, "Out of memory.\n"); fclose(disk_fp); free(file_data); return 1; }
    if (fread(disk, 1, disk_size, disk_fp) != (size_t)disk_size) {
        fprintf(stderr, "Disk read error.\n"); fclose(disk_fp); free(disk); free(file_data); return 1;
    }

    /* Parse boot sector */
    memcpy(&bs, disk, sizeof(BootSector));

    fat_start    = fat_offset();
    root_start   = root_offset();
    data_start   = data_offset();

    // Calculate total clusters in the data region based on total sectors and data start offset
    uint32_t total_sectors;
    if (bs.total_sectors_16 != 0) {
        total_sectors = bs.total_sectors_16;
    } else {
        total_sectors = bs.total_sectors_32;
    }
    uint32_t data_start_sector = data_start / bs.bytes_per_sector;
    uint32_t data_sectors = total_sectors - data_start_sector;
    total_clusters = data_sectors / bs.sectors_per_cluster;

    /* 3. Resolve destination directory */
    uint16_t dir_cluster;
    char     filename[13];

    if (!resolve_path(dest_path, &dir_cluster, filename)) {
        printf("The directory not found.\n");
        free(disk); free(file_data); fclose(disk_fp);
        return 1;
    }

    /* 4. Check free space */
    uint32_t bytes_per_cluster = bs.sectors_per_cluster * bs.bytes_per_sector;
    uint32_t clusters_needed   = (file_size + bytes_per_cluster - 1) / bytes_per_cluster;
    if (file_size == 0) clusters_needed = 0;

    if (count_free_clusters() < clusters_needed) {
        printf("No enough free space in the disk image.\n");
        free(disk); free(file_data); fclose(disk_fp);
        return 1;
    }

    /* 5. Allocate clusters and write data */
    uint16_t first_cluster = 0;
    uint16_t prev_cluster  = 0;

    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint16_t c = find_free_cluster(2);
        if (c == 0) {
            printf("No enough free space in the disk image.\n");
            free(disk); free(file_data); fclose(disk_fp);
            return 1;
        }
        fat_set(c, 0xFFF); /* temporarily mark end-of-chain */

        if (prev_cluster != 0)
            fat_set(prev_cluster, c); /* link previous cluster */
        else
            first_cluster = c;

        prev_cluster = c;

        /* Copy data into cluster */
        uint32_t offset  = cluster_to_offset(c);
        uint32_t written = i * bytes_per_cluster;
        uint32_t chunk   = file_size - written;
        if (chunk > bytes_per_cluster) chunk = bytes_per_cluster;

        memset(disk + offset, 0, bytes_per_cluster);
        if (chunk > 0)
            memcpy(disk + offset, file_data + written, chunk);
    }

    /* 6. Create directory entry*/
    DirEntry *slot = find_free_slot(dir_cluster);
    if (!slot) {
        printf("No enough free space in the disk image.\n");
        free(disk); free(file_data); fclose(disk_fp);
        return 1;
    }

    uint8_t n83[8], e83[3];
    to_83(filename, n83, e83);

    uint16_t fat_date, fat_time;
    unix_to_fat(mtime, &fat_date, &fat_time);

    memset(slot, 0, sizeof(DirEntry));
    memcpy(slot->name, n83, 8);
    memcpy(slot->ext,  e83, 3);
    slot->attr               = ATTR_ARCHIVE;
    slot->creation_time      = fat_time;
    slot->creation_date      = fat_date;
    slot->write_time         = fat_time;
    slot->write_date         = fat_date;
    slot->first_cluster      = (clusters_needed > 0) ? first_cluster : 0;
    slot->file_size          = file_size;

    /* ── 7. Write disk image back ── */
    rewind(disk_fp);
    fwrite(disk, 1, disk_size, disk_fp);
    fclose(disk_fp);

    free(disk);
    free(file_data);
    return 0;
}
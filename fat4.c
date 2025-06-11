#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <time.h>
#include <string.h>


typedef struct __attribute__((__packed__)) {
    uint8_t BS_jmpBoot[3];
    uint8_t BS_OEMName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t BPB_NumFATs;
    uint16_t BPB_RootEntCnt;
    uint16_t BPB_TotSec16;
    uint8_t BPB_Media;
    uint16_t BPB_FATSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint8_t BS_DrvNum;
    uint8_t BS_Reserved1;
    uint8_t BS_BootSig;
    uint32_t BS_VolID;
    uint8_t BS_VolLab[11];
    uint8_t BS_FilSysType[8];
} Fat16BootSector;


typedef struct {
    uint8_t DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} DirectoryEntry;

void formatTimeDate(uint16_t time, uint16_t date, char *timeStr, char *dateStr) {
    struct tm tm = {0};

    tm.tm_sec = (time & 0x1F) * 2;
    tm.tm_min = (time >> 5) & 0x3F;
    tm.tm_hour = (time >> 11);

    tm.tm_mday = (date & 0x1F);
    tm.tm_mon = ((date >> 5) & 0x0F) - 1;
    tm.tm_year = ((date >> 9) + 80);

    strftime(timeStr, 9, "%H:%M:%S", &tm);
    strftime(dateStr, 11, "%Y-%m-%d", &tm);
}

void formatFileName(const uint8_t *DIR_Name, char *formattedName) {
    char name[9], ext[4];
    strncpy(name, DIR_Name, 8);
    strncpy(ext, DIR_Name + 8, 3);
    name[8] = '\0';
    ext[3] = '\0';

    // Remove padding spaces
    for (int i = 7; i >= 0 && name[i] == ' '; --i) name[i] = '\0';
    for (int i = 2; i >= 0 && ext[i] == ' '; --i) ext[i] = '\0';

    // Concatenate name and extension
    if (strlen(ext) > 0)
        sprintf(formattedName, "%s.%s", name, ext);
    else
        sprintf(formattedName, "%s", name);
}


int main() {
    const char *filePath = "fat16 (1).img"; // change for requited cluster 
    int fileDescriptor;
    Fat16BootSector bootSector;

    // Open the file
    fileDescriptor = open(filePath, O_RDONLY);
    if (fileDescriptor < 0) {
        perror("Error opening file");
        return 1;
    }

    // Read the boot sector
    if (read(fileDescriptor, &bootSector, sizeof(bootSector)) < sizeof(bootSector)) {
        perror("Error reading boot sector");
        close(fileDescriptor);
        return 1;
    }

    // allocate memory
    size_t fatSize = bootSector.BPB_FATSz16 * bootSector.BPB_BytsPerSec;
    uint16_t *fat = (uint16_t *)malloc(fatSize);
    if (!fat) {
        perror("Error allocating memory for FAT");
        close(fileDescriptor);
        return 1;
    }


    off_t fatOffset = bootSector.BPB_BytsPerSec * bootSector.BPB_RsvdSecCnt;
    lseek(fileDescriptor, fatOffset, SEEK_SET);
    if (read(fileDescriptor, fat, fatSize) < fatSize) {
        perror("Error reading FAT");
        close(fileDescriptor);
        free(fat);
        return 1;
    }

    // Calculate the location and size of the root directory
    off_t rootDirOffset = (bootSector.BPB_RsvdSecCnt + bootSector.BPB_NumFATs * bootSector.BPB_FATSz16) * bootSector.BPB_BytsPerSec;
    size_t rootDirSize = bootSector.BPB_RootEntCnt * sizeof(DirectoryEntry);
    DirectoryEntry *rootDir = (DirectoryEntry *)malloc(rootDirSize);

    lseek(fileDescriptor, rootDirOffset, SEEK_SET);
    read(fileDescriptor, rootDir, rootDirSize);

    // Process the root directory entries
    for (int i = 0; i < bootSector.BPB_RootEntCnt; ++i) {
        DirectoryEntry entry = rootDir[i];

        // Ignore empty or deleted entries
        if (entry.DIR_Name[0] == 0x00 || entry.DIR_Name[0] == 0xE5) continue;

        // Decode starting cluster
        uint32_t startingCluster = ((uint32_t)entry.DIR_FstClusHI << 16) | entry.DIR_FstClusLO;

        // Format time and date
        char timeStr[9], dateStr[11];
        formatTimeDate(entry.DIR_WrtTime, entry.DIR_WrtDate, timeStr, dateStr);

        // Format file name
        char formattedName[13];
        formatFileName(entry.DIR_Name, formattedName);

        // Decode attributes
        char attrStr[7] = "------";
        if (entry.DIR_Attr & 0x01) attrStr[5] = 'R'; // Read-Only
        if (entry.DIR_Attr & 0x02) attrStr[4] = 'H'; // Hidden
        if (entry.DIR_Attr & 0x04) attrStr[3] = 'S'; // System
        if (entry.DIR_Attr & 0x08) attrStr[2] = 'V'; // Volume ID
        if (entry.DIR_Attr & 0x10) attrStr[1] = 'D'; // Directory
        if (entry.DIR_Attr & 0x20) attrStr[0] = 'A'; // Archive

        // Print file info
        printf("%10u %10s %10s %s %10u %11s\n", 
               startingCluster, timeStr, dateStr, attrStr, entry.DIR_FileSize, formattedName);
    }


    //starting cluster
    uint16_t currentCluster = 2; 

    // Output the cluster chain
    printf("Cluster chain starting from cluster %u:\n", currentCluster);
    while (currentCluster < 0xFFF8) {
        printf("%u ", currentCluster);
        currentCluster = fat[currentCluster];
    }
    printf("\n");

    // Close 
    free(rootDir);
    close(fileDescriptor);
    free(fat);
    return 0;
}

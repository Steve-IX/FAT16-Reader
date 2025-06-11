#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

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

int main() {
    const char *filePath = "fat16 (1).img"; // <--- path
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

    // FAT  memory
    off_t fatOffset = bootSector.BPB_BytsPerSec * bootSector.BPB_RsvdSecCnt;
    lseek(fileDescriptor, fatOffset, SEEK_SET);
    if (read(fileDescriptor, fat, fatSize) < fatSize) {
        perror("Error reading FAT");
        close(fileDescriptor);
        free(fat);
        return 1;
    }

    //starting cluster
    uint16_t currentCluster = 2; 

    // cluster chain
    printf("Cluster chain starting from cluster %u:\n", currentCluster);
    while (currentCluster < 0xFFF8) {
        printf("%u ", currentCluster);
        currentCluster = fat[currentCluster];
    }
    printf("\n");


    close(fileDescriptor);
    free(fat);
    return 0;
}

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
    const char *filePath = "fat16 (1).img"; // file path
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

    // Output values
    printf("Bytes per Sector: %u\n", bootSector.BPB_BytsPerSec);
    printf("Sectors per Cluster: %u\n", bootSector.BPB_SecPerClus);
    printf("Reserved Sector Count: %u\n", bootSector.BPB_RsvdSecCnt);
    printf("Number of FATs: %u\n", bootSector.BPB_NumFATs);

    // Close the file
    close(fileDescriptor);

    return 0;
}

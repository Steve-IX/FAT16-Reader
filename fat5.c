#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

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

//file structure 
typedef struct {
    uint32_t currentCluster;   
    uint32_t nextCluster;      
    uint16_t BPB_RsvdSecCnt;    
    uint8_t BPB_NumFATs;        
    uint16_t BPB_FATSz16;     
    uint32_t fileSize;          
    uint32_t currentPosition;   
    uint16_t *fat;              
    int fd;                     
    off_t clusterByteOffset;    
    uint16_t BPB_BytsPerSec;    
    uint8_t BPB_SecPerClus;    
} File;

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


File *openFile(int fd, uint16_t *fat, DirectoryEntry *entry, uint16_t BPB_BytsPerSec, uint8_t BPB_SecPerClus);
off_t seekFile(File *file, off_t offset, int whence);
size_t readFile(File *file, void *buffer, size_t bufferSize, size_t length);
void closeFile(File *file);


//open file
File *openFile(int fd, uint16_t *fat, DirectoryEntry *entry, uint16_t BPB_BytsPerSec, uint8_t BPB_SecPerClus) {
    File *file = malloc(sizeof(File));
    if (!file) return NULL;

    file->currentCluster = ((uint32_t)entry->DIR_FstClusHI << 16) | entry->DIR_FstClusLO;
    file->fileSize = entry->DIR_FileSize;
    file->currentPosition = 0;
    file->fat = fat;
    file->fd = fd;
    file->BPB_BytsPerSec = BPB_BytsPerSec;
    file->BPB_SecPerClus = BPB_SecPerClus;
    file->clusterByteOffset = 0;

    // Check entry
    if (entry->DIR_Attr & 0x10 || entry->DIR_Attr & 0x08) {
        free(file);
        return NULL; 
    }

    if (file->currentCluster < 0xFFF8) {
        file->nextCluster = fat[file->currentCluster];
        file->clusterByteOffset = (file->currentCluster - 2) * BPB_SecPerClus * BPB_BytsPerSec;
    } else {
        file->nextCluster = 0;
    }

    return file;
}

//da seek file
off_t seekFile(File *file, off_t offset, int whence) {
    off_t newpos;

    switch (whence) {
        case SEEK_SET:
            newpos = offset;
            break;
        case SEEK_CUR:
            newpos = file->currentPosition + offset;
            break;
        case SEEK_END:
            newpos = file->fileSize + offset;
            break;
        default:
            return -1; 
        return file->currentPosition;
    }

    if (newpos < 0 || newpos > file->fileSize) {
        return -1; // New position is out of bounds
    }

    // Check position
    uint32_t newCluster = file->currentCluster;
    off_t clusterSize = file->BPB_BytsPerSec * file->BPB_SecPerClus;
    off_t currentClusterOffset = file->currentPosition % clusterSize;
    off_t newClusterOffset = newpos % clusterSize;

    
    if (newpos / clusterSize == file->currentPosition / clusterSize) {
        file->currentPosition = newpos;
        return file->currentPosition;
    }

    // If new position is in a different cluster
    newCluster = file->currentCluster;
    while (newpos / clusterSize > 0) {
        newCluster = file->fat[newCluster];
        newpos -= clusterSize;
        if (newCluster >= 0xFFF8) {
            // the enddd
            return -1;
        }
    }

    // Update file structure with new cluster and position
    file->currentCluster = newCluster;
    file->currentPosition = newClusterOffset;
    file->clusterByteOffset = ((file->currentCluster - 2) * file->BPB_SecPerClus + file->BPB_RsvdSecCnt + file->BPB_NumFATs * file->BPB_FATSz16) * file->BPB_BytsPerSec;

    return file->currentPosition;
}

//readfile
size_t readFile(File *file, void *buffer, size_t bufferSize, size_t length) {
    if (file->currentPosition >= file->fileSize) {
        return 0; // Already at or beyond the end of the file
    }

    // max length 
    size_t readableLength = (file->currentPosition + length > file->fileSize) ? 
                            file->fileSize - file->currentPosition : length;


    
    readableLength = (readableLength > bufferSize) ? bufferSize : readableLength;

    size_t bytesRead = 0;
    size_t bytesToRead;
    char *buf = (char *)buffer;

    while (bytesRead < readableLength) {
        
        size_t clusterOffset = file->currentPosition % (file->BPB_BytsPerSec * file->BPB_SecPerClus);

        
        bytesToRead = file->BPB_BytsPerSec * file->BPB_SecPerClus - clusterOffset;
        if (bytesRead + bytesToRead > readableLength) {
            bytesToRead = readableLength - bytesRead;
        }

        // Read data from the current cluster
        lseek(file->fd, file->clusterByteOffset + clusterOffset, SEEK_SET);
        read(file->fd, buf + bytesRead, bytesToRead);

        // Update bytesRead and file->currentPosition
        bytesRead += bytesToRead;
        file->currentPosition += bytesToRead;

        
        if (file->currentPosition % (file->BPB_BytsPerSec * file->BPB_SecPerClus) == 0) {
            file->currentCluster = file->fat[file->currentCluster];

            // Check for end of cluster chain
            if (file->currentCluster >= 0xFFF8) {
                break;
            }

            
            file->clusterByteOffset = ((file->currentCluster - 2) * file->BPB_SecPerClus + file->BPB_RsvdSecCnt + file->BPB_NumFATs * file->BPB_FATSz16) * file->BPB_BytsPerSec;
        }
    }

    return bytesRead;
}

//close file 
void closeFile(File *file) {
    if (file) {
        free(file);
    }
}

//adds voids below heeree from fat4//

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

    
    if (strlen(ext) > 0)
        sprintf(formattedName, "%s.%s", name, ext);
    else
        sprintf(formattedName, "%s", name);
}

void formatTimeDate(uint16_t time, uint16_t date, char *timeStr, char *dateStr);
void formatFileName(const uint8_t *DIR_Name, char *formattedName);

//file operation
File *openFile(int fd, uint16_t *fat, DirectoryEntry *entry, uint16_t BPB_BytsPerSec, uint8_t BPB_SecPerClus);
off_t seekFile(File *file, off_t offset, int whence);
size_t readFile(File *file, void *buffer, size_t bufferSize, size_t length);
void closeFile(File *file);

int main() {
    const char *filePath = "fat16 (1).img"; // file path
    int fileDescriptor = open(filePath, O_RDONLY);
    if (fileDescriptor < 0) {
        perror("Error opening file");
        return 1;
    }

    Fat16BootSector bootSector;
    if (read(fileDescriptor, &bootSector, sizeof(bootSector)) < sizeof(bootSector)) {
        perror("Error reading boot sector");
        close(fileDescriptor);
        return 1;
    }

    // Read FAT
    size_t fatSize = bootSector.BPB_FATSz16 * bootSector.BPB_BytsPerSec;
    uint16_t *fat = malloc(fatSize);
    if (!fat) {
        perror("Error allocating memory for FAT");
        close(fileDescriptor);
        return 1;
    }
    lseek(fileDescriptor, bootSector.BPB_RsvdSecCnt * bootSector.BPB_BytsPerSec, SEEK_SET);
    if (read(fileDescriptor, fat, fatSize) < fatSize) {
        perror("Error reading FAT");
        close(fileDescriptor);
        free(fat);
        return 1;
    }

    // Read Root Directory
    off_t rootDirOffset = (bootSector.BPB_RsvdSecCnt + bootSector.BPB_NumFATs * bootSector.BPB_FATSz16) * bootSector.BPB_BytsPerSec;
    size_t rootDirSize = bootSector.BPB_RootEntCnt * sizeof(DirectoryEntry);
    DirectoryEntry *rootDir = malloc(rootDirSize);
    if (!rootDir) {
        perror("Error allocating memory for root directory");
        close(fileDescriptor);
        free(fat);
        return 1;
    }
    lseek(fileDescriptor, rootDirOffset, SEEK_SET);
    if (read(fileDescriptor, rootDir, rootDirSize) < rootDirSize) {
        perror("Error reading root directory");
        close(fileDescriptor);
        free(fat);
        free(rootDir);
        return 1;
    }

    // Process files in Root Directory
    for (int i = 0; i < bootSector.BPB_RootEntCnt; ++i) {
        DirectoryEntry entry = rootDir[i];
        if (entry.DIR_Name[0] == 0x00 || entry.DIR_Name[0] == 0xE5) continue; // Skip empty or deleted entries

        char formattedName[13];
        formatFileName(entry.DIR_Name, formattedName);

        // Skip directories and system files
        if (entry.DIR_Attr & 0x10 || entry.DIR_Attr & 0x04) continue;

        // Open and read file
        File *file = openFile(fileDescriptor, fat, &entry, bootSector.BPB_BytsPerSec, bootSector.BPB_SecPerClus);
        if (!file) continue;

        char buffer[1024]; // Adjust the buffer size as needed
        size_t bytesRead = readFile(file, buffer, sizeof(buffer), file->fileSize);

        printf("Contents of %s:\n", formattedName);
        for (size_t j = 0; j < bytesRead; ++j) {
            if (isprint(buffer[j]) || buffer[j] == '\n') {
                printf("%c", buffer[j]);
            } else {
                printf(".");
            }
        }
        printf("\n\n");

        closeFile(file);
    }

    // Close
    free(rootDir);
    free(fat);
    close(fileDescriptor);

    return 0;
}

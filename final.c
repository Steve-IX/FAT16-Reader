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
#include <wchar.h>
#include <stdbool.h>

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

//long directory entry
typedef struct {
    uint8_t LDIR_Ord;
    uint8_t LDIR_Name1[10];
    uint8_t LDIR_Attr;
    uint8_t LDIR_Type;
    uint8_t LDIR_Chksum;
    uint8_t LDIR_Name2[12];
    uint16_t LDIR_FstClusLO;
    uint8_t LDIR_Name3[4];
} __attribute__((__packed__)) LongDirectoryEntry;

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

    // Check if the entry is a regular file
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
        return -1; 
    }

    
    uint32_t newCluster = file->currentCluster;
    off_t clusterSize = file->BPB_BytsPerSec * file->BPB_SecPerClus;
    off_t currentClusterOffset = file->currentPosition % clusterSize;
    off_t newClusterOffset = newpos % clusterSize;

    
    if (newpos / clusterSize == file->currentPosition / clusterSize) {
        file->currentPosition = newpos;
        return file->currentPosition;
    }

    
    newCluster = file->currentCluster;
    while (newpos / clusterSize > 0) {
        newCluster = file->fat[newCluster];
        newpos -= clusterSize;
        if (newCluster >= 0xFFF8) {
            
            return -1;
        }
    }

    
    file->currentCluster = newCluster;
    file->currentPosition = newClusterOffset;
    file->clusterByteOffset = ((file->currentCluster - 2) * file->BPB_SecPerClus + file->BPB_RsvdSecCnt + file->BPB_NumFATs * file->BPB_FATSz16) * file->BPB_BytsPerSec;

    return file->currentPosition;
}

//readfile
size_t readFile(File *file, void *buffer, size_t bufferSize, size_t length) {
    if (file->currentPosition >= file->fileSize) {
        return 0; 
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

        // Calculate how much data 
        bytesToRead = file->BPB_BytsPerSec * file->BPB_SecPerClus - clusterOffset;
        if (bytesRead + bytesToRead > readableLength) {
            bytesToRead = readableLength - bytesRead;
        }

        // Read data from the current cluster
        lseek(file->fd, file->clusterByteOffset + clusterOffset, SEEK_SET);
        read(file->fd, buf + bytesRead, bytesToRead);

        
        bytesRead += bytesToRead;
        file->currentPosition += bytesToRead;

        
        if (file->currentPosition % (file->BPB_BytsPerSec * file->BPB_SecPerClus) == 0) {
            file->currentCluster = file->fat[file->currentCluster];

        
            if (file->currentCluster >= 0xFFF8) {
                break;
            }

            
            file->clusterByteOffset = ((file->currentCluster - 2) * file->BPB_SecPerClus + file->BPB_RsvdSecCnt + file->BPB_NumFATs * file->BPB_FATSz16) * file->BPB_BytsPerSec;
        }
    }

    return bytesRead;
}


void closeFile(File *file) {
    if (file) {
        free(file);
    }
}

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

int isLongDirectoryEntry(const DirectoryEntry *entry) {
    return (entry->DIR_Attr == 0x0F);
}

void decodeLongFileName(const LongDirectoryEntry *entries, int count, wchar_t *output) {
    wchar_t longName[260] = {0}; 
    int pos = 0;

    for (int i = count - 1; i >= 0; --i) { // Iterate in reverse
        const LongDirectoryEntry *entry = &entries[i];

        
        for (int j = 0; j < 5 && entry->LDIR_Name1[j*2] != 0xFF && entry->LDIR_Name1[j*2+1] != 0xFF; j++) {
            longName[pos++] = entry->LDIR_Name1[j*2] + (entry->LDIR_Name1[j*2+1] << 8);
        }
        for (int j = 0; j < 6 && entry->LDIR_Name2[j*2] != 0xFF && entry->LDIR_Name2[j*2+1] != 0xFF; j++) {
            longName[pos++] = entry->LDIR_Name2[j*2] + (entry->LDIR_Name2[j*2+1] << 8);
        }
        for (int j = 0; j < 2 && entry->LDIR_Name3[j*2] != 0xFF && entry->LDIR_Name3[j*2+1] != 0xFF; j++) {
            longName[pos++] = entry->LDIR_Name3[j*2] + (entry->LDIR_Name3[j*2+1] << 8);
        }
    }
    longName[pos] = '\0'; 
    wcscpy(output, longName);
}

// Function to split the path into components
void splitPath(const char *path, char components[][13], int *numComponents)
{
    char *token;
    char tempPath[256];
    strcpy(tempPath, path);

    token = strtok(tempPath, "/");
    *numComponents = 0;
    while (token != NULL)
    {
        strcpy(components[(*numComponents)++], token);
        token = strtok(NULL, "/");
    }
}

// compare names 
bool compareDirName(const char *name, const DirectoryEntry *entry)
{
    char formattedName[13];
    formatFileName(entry->DIR_Name, formattedName); 
    return strcmp(name, formattedName) == 0;
}

int findDirEntry(int fd, uint16_t *fat, uint32_t currentCluster, const char *name, DirectoryEntry *foundEntry, uint16_t BPB_BytsPerSec, uint8_t BPB_SecPerClus, uint16_t BPB_RsvdSecCnt, uint8_t BPB_NumFATs, uint16_t BPB_FATSz16)
{

    // function body
    DirectoryEntry entry;
    size_t bytesRead;
    off_t clusterSize = BPB_BytsPerSec * BPB_SecPerClus;
    char buffer[clusterSize]; 

    while (currentCluster < 0xFFF8)
    {
        
        off_t clusterOffset = ((currentCluster - 2) * BPB_SecPerClus + BPB_RsvdSecCnt + BPB_NumFATs * BPB_FATSz16) * BPB_BytsPerSec;

        
        lseek(fd, clusterOffset, SEEK_SET);
        bytesRead = read(fd, buffer, clusterSize);

        
        for (int i = 0; i < bytesRead; i += sizeof(DirectoryEntry))
        {
            memcpy(&entry, buffer + i, sizeof(DirectoryEntry));

            //check entries
            if (entry.DIR_Name[0] == 0x00)
            {
                return 0; 
            }

            
            if (entry.DIR_Name[0] == 0xE5 || (entry.DIR_Attr & 0x08))
            {
                continue;
            }

            
            if (compareDirName(name, &entry))
            {
                memcpy(foundEntry, &entry, sizeof(DirectoryEntry));
                return 1; 
            }
        }

        
        currentCluster = fat[currentCluster];
    }

    return 0; 
}


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

    
    off_t fatOffset = bootSector.BPB_RsvdSecCnt * bootSector.BPB_BytsPerSec;
    

    
    // root
    off_t rootDirOffset = (bootSector.BPB_RsvdSecCnt + bootSector.BPB_NumFATs * bootSector.BPB_FATSz16) * bootSector.BPB_BytsPerSec;
    size_t rootDirSize = bootSector.BPB_RootEntCnt * sizeof(DirectoryEntry);
    
    // Allocate memory 
    DirectoryEntry *rootDir = malloc(rootDirSize);
    if (!rootDir) {
        perror("Error allocating memory for root directory");
        close(fileDescriptor);
        return 1;
    }

    lseek(fileDescriptor, rootDirOffset, SEEK_SET);
    if (read(fileDescriptor, rootDir, rootDirSize) < rootDirSize) {
        perror("Error reading root directory");
        free(rootDir);
        close(fileDescriptor);
        return 1;
    }

    // Iterates
    for (int i = 0; i < bootSector.BPB_RootEntCnt; i++) {
        DirectoryEntry entry = rootDir[i];

        if (entry.DIR_Name[0] == 0x00) {
            break; 
        }

        if (entry.DIR_Name[0] == 0xE5) {
            continue; 
        }

        char formattedName[13];
        formatFileName(entry.DIR_Name, formattedName);

        if (entry.DIR_Attr & 0x10) {
            printf("Directory: %s\n", formattedName); 
        } else {
            printf("File: %s\n", formattedName);
        }

    
    }

    
    
    // Allocote memory for the diroctory
    
    if (!rootDir) {
        perror("Error allocating memory for root directory");
        close(fileDescriptor);
        return 1;
    }

    lseek(fileDescriptor, rootDirOffset, SEEK_SET);
    if (read(fileDescriptor, rootDir, rootDirSize) < rootDirSize) {
        perror("Error reading root directory");
        free(rootDir);
        close(fileDescriptor);
        return 1;
    }

    // Iterate through the root 
    for (int i = 0; i < bootSector.BPB_RootEntCnt; i++) {
        DirectoryEntry entry = rootDir[i];

        if (entry.DIR_Name[0] == 0x00) {
            break; 
        }

        if (entry.DIR_Name[0] == 0xE5) {
            continue; 
        }

        char formattedName[13];
        formatFileName(entry.DIR_Name, formattedName);

        if (entry.DIR_Attr & 0x10) {
            printf("Directory: %s\n", formattedName); 
        } else {
            printf("File: %s\n", formattedName); 
        }

    }

    size_t fatSize = bootSector.BPB_FATSz16 * bootSector.BPB_BytsPerSec;
    uint16_t *fat = malloc(fatSize);
    if (!fat) {
        perror("Error allocating memory for FAT");
        close(fileDescriptor);
        return 1;
    }

    // Read FAT
    
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

    // strarts long directory entry 
    LongDirectoryEntry longDirEntries[20]; 
    int longDirCount = 0;
    wchar_t longFileName[256];
    
    
    for (int i = 0; i < bootSector.BPB_RootEntCnt; ++i) {
        DirectoryEntry entry = rootDir[i];

        if (isLongDirectoryEntry(&entry)) {
            memcpy(&longDirEntries[longDirCount++], &entry, sizeof(LongDirectoryEntry));
        } else {
            
            if (longDirCount > 0) {
                decodeLongFileName(longDirEntries, longDirCount, longFileName);
                wprintf(L"Long File Name: %ls\n", longFileName); 
                longDirCount = 0; 
            }

            
        }
    }

    // files in the root 
    for (int i = 0; i < bootSector.BPB_RootEntCnt; ++i) {
        DirectoryEntry entry = rootDir[i];
        if (entry.DIR_Name[0] == 0x00 || entry.DIR_Name[0] == 0xE5) continue; 

        char formattedName[13];
        formatFileName(entry.DIR_Name, formattedName);

        
        if (entry.DIR_Attr & 0x10 || entry.DIR_Attr & 0x04) continue;

        // Open/ read
        File *file = openFile(fileDescriptor, fat, &entry, bootSector.BPB_BytsPerSec, bootSector.BPB_SecPerClus);
        if (!file) continue;

        char buffer[1024];
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

    fileDescriptor = open(filePath, O_RDONLY);
    if (fileDescriptor < 0) {
        perror("Error opening file");
        return 1;
    }
    
    if (read(fileDescriptor, &bootSector, sizeof(bootSector)) < sizeof(bootSector)) {
        perror("Error reading boot sector");
        close(fileDescriptor);
        return 1;
    }

    // Calculate the size of the fat
    
    if (!fat) {
        perror("Error allocating memory for FAT");
        close(fileDescriptor);
        return 1;
    }
    

    lseek(fileDescriptor, rootDirOffset, SEEK_SET);
    read(fileDescriptor, rootDir, rootDirSize);

    
    for (int i = 0; i < bootSector.BPB_RootEntCnt; ++i) {
        DirectoryEntry entry = rootDir[i];
        
        if (entry.DIR_Name[0] == 0x00 || entry.DIR_Name[0] == 0xE5) continue;
        
        uint32_t startingCluster = ((uint32_t)entry.DIR_FstClusHI << 16) | entry.DIR_FstClusLO;

        char timeStr[9], dateStr[11];
        formatTimeDate(entry.DIR_WrtTime, entry.DIR_WrtDate, timeStr, dateStr);

        char formattedName[13];
        formatFileName(entry.DIR_Name, formattedName);

        // Decodes
        char attrStr[7] = "------";
        if (entry.DIR_Attr & 0x01) attrStr[5] = 'R'; // Read-Only
        if (entry.DIR_Attr & 0x02) attrStr[4] = 'H'; // Hidden
        if (entry.DIR_Attr & 0x04) attrStr[3] = 'S'; // System
        if (entry.DIR_Attr & 0x08) attrStr[2] = 'V'; // Volume ID
        if (entry.DIR_Attr & 0x10) attrStr[1] = 'D'; // Directory
        if (entry.DIR_Attr & 0x20) attrStr[0] = 'A'; // Archive

        // file info
        printf("%10u %10s %10s %s %10u %11s\n", 
               startingCluster, timeStr, dateStr, attrStr, entry.DIR_FileSize, formattedName);
    }

    uint16_t currentCluster = 2; // 

    printf("Cluster chain starting from cluster %u:\n", currentCluster);
    while (currentCluster < 0xFFF8) {
        printf("%u ", currentCluster);
        currentCluster = fat[currentCluster];
    }
    printf("\n");

    printf("Bytes per Sector: %u\n", bootSector.BPB_BytsPerSec);
    printf("Sectors per Cluster: %u\n", bootSector.BPB_SecPerClus);
    printf("Reserved Sector Count: %u\n", bootSector.BPB_RsvdSecCnt);
    printf("Number of FATs: %u\n", bootSector.BPB_NumFATs);


    // Close 
    close(fileDescriptor);
    free(fat);
    return 0;
}    
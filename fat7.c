#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <wchar.h>
#include <stdbool.h>

typedef struct __attribute__((__packed__))
{
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

// file structure
typedef struct
{
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

typedef struct
{
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

// long directory entry
typedef struct
{
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

// open file
File *openFile(int fd, uint16_t *fat, DirectoryEntry *entry, uint16_t BPB_BytsPerSec, uint8_t BPB_SecPerClus)
{
    File *file = malloc(sizeof(File));
    if (!file)
        return NULL;

    file->currentCluster = ((uint32_t)entry->DIR_FstClusHI << 16) | entry->DIR_FstClusLO;
    file->fileSize = entry->DIR_FileSize;
    file->currentPosition = 0;
    file->fat = fat;
    file->fd = fd;
    file->BPB_BytsPerSec = BPB_BytsPerSec;
    file->BPB_SecPerClus = BPB_SecPerClus;
    file->clusterByteOffset = 0;

    
    if (entry->DIR_Attr & 0x10 || entry->DIR_Attr & 0x08)
    {
        free(file);
        return NULL; 
    }

    if (file->currentCluster < 0xFFF8)
    {
        file->nextCluster = fat[file->currentCluster];
        file->clusterByteOffset = (file->currentCluster - 2) * BPB_SecPerClus * BPB_BytsPerSec;
    }
    else
    {
        file->nextCluster = 0;
    }

    return file;
}

// seek file
off_t seekFile(File *file, off_t offset, int whence)
{
    off_t newpos;

    switch (whence)
    {
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

    if (newpos < 0 || newpos > file->fileSize)
    {
        return -1; 
    }

    //different cluster
    uint32_t newCluster = file->currentCluster;
    off_t clusterSize = file->BPB_BytsPerSec * file->BPB_SecPerClus;
    off_t currentClusterOffset = file->currentPosition % clusterSize;
    off_t newClusterOffset = newpos % clusterSize;

    
    if (newpos / clusterSize == file->currentPosition / clusterSize)
    {
        file->currentPosition = newpos;
        return file->currentPosition;
    }

    // new position is in a different cluster
    newCluster = file->currentCluster;
    while (newpos / clusterSize > 0)
    {
        newCluster = file->fat[newCluster];
        newpos -= clusterSize;
        if (newCluster >= 0xFFF8)
        {
            
            return -1;
        }
    }

    // Update file structure 
    file->currentCluster = newCluster;
    file->currentPosition = newClusterOffset;
    file->clusterByteOffset = ((file->currentCluster - 2) * file->BPB_SecPerClus + file->BPB_RsvdSecCnt + file->BPB_NumFATs * file->BPB_FATSz16) * file->BPB_BytsPerSec;

    return file->currentPosition;
}

// readfile
size_t readFile(File *file, void *buffer, size_t bufferSize, size_t length)
{
    if (file->currentPosition >= file->fileSize)
    {
        return 0; // Already at or beyond the end of the file
    }

    
    size_t readableLength = (file->currentPosition + length > file->fileSize) ? file->fileSize - file->currentPosition : length;

    //limit buffer 
    readableLength = (readableLength > bufferSize) ? bufferSize : readableLength;

    size_t bytesRead = 0;
    size_t bytesToRead;
    char *buf = (char *)buffer;

    while (bytesRead < readableLength)
    {
        
        size_t clusterOffset = file->currentPosition % (file->BPB_BytsPerSec * file->BPB_SecPerClus);

        // Calculate how much data to read
        bytesToRead = file->BPB_BytsPerSec * file->BPB_SecPerClus - clusterOffset;
        if (bytesRead + bytesToRead > readableLength)
        {
            bytesToRead = readableLength - bytesRead;
        }

        // Reads current cluster
        lseek(file->fd, file->clusterByteOffset + clusterOffset, SEEK_SET);
        read(file->fd, buf + bytesRead, bytesToRead);

    
        bytesRead += bytesToRead;
        file->currentPosition += bytesToRead;

    
        if (file->currentPosition % (file->BPB_BytsPerSec * file->BPB_SecPerClus) == 0)
        {
            file->currentCluster = file->fat[file->currentCluster];

            
            if (file->currentCluster >= 0xFFF8)
            {
                break;
            }

            
            file->clusterByteOffset = ((file->currentCluster - 2) * file->BPB_SecPerClus + file->BPB_RsvdSecCnt + file->BPB_NumFATs * file->BPB_FATSz16) * file->BPB_BytsPerSec;
        }
    }

    return bytesRead;
}


void closeFile(File *file)
{
    if (file)
    {
        free(file);
    }
}

// add voids //

void formatTimeDate(uint16_t time, uint16_t date, char *timeStr, char *dateStr)
{
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

void formatFileName(const uint8_t *DIR_Name, char *formattedName)
{
    char name[9], ext[4];
    strncpy(name, DIR_Name, 8);
    strncpy(ext, DIR_Name + 8, 3);
    name[8] = '\0';
    ext[3] = '\0';


    for (int i = 7; i >= 0 && name[i] == ' '; --i)
        name[i] = '\0';
    for (int i = 2; i >= 0 && ext[i] == ' '; --i)
        ext[i] = '\0';


    if (strlen(ext) > 0)
        sprintf(formattedName, "%s.%s", name, ext);
    else
        sprintf(formattedName, "%s", name);
}

void formatTimeDate(uint16_t time, uint16_t date, char *timeStr, char *dateStr);
void formatFileName(const uint8_t *DIR_Name, char *formattedName);

// file operation
File *openFile(int fd, uint16_t *fat, DirectoryEntry *entry, uint16_t BPB_BytsPerSec, uint8_t BPB_SecPerClus);
off_t seekFile(File *file, off_t offset, int whence);
size_t readFile(File *file, void *buffer, size_t bufferSize, size_t length);
void closeFile(File *file);

int isLongDirectoryEntry(const DirectoryEntry *entry)
{
    return (entry->DIR_Attr == 0x0F);
}

void decodeLongFileName(const LongDirectoryEntry *entries, int count, wchar_t *output)
{
    wchar_t longName[260] = {0}; // max lenght
    int pos = 0;

    for (int i = count - 1; i >= 0; --i)
    { // Iterate in reverse
        const LongDirectoryEntry *entry = &entries[i];

        
        for (int j = 0; j < 5 && entry->LDIR_Name1[j * 2] != 0xFF && entry->LDIR_Name1[j * 2 + 1] != 0xFF; j++)
        {
            longName[pos++] = entry->LDIR_Name1[j * 2] + (entry->LDIR_Name1[j * 2 + 1] << 8);
        }
        for (int j = 0; j < 6 && entry->LDIR_Name2[j * 2] != 0xFF && entry->LDIR_Name2[j * 2 + 1] != 0xFF; j++)
        {
            longName[pos++] = entry->LDIR_Name2[j * 2] + (entry->LDIR_Name2[j * 2 + 1] << 8);
        }
        for (int j = 0; j < 2 && entry->LDIR_Name3[j * 2] != 0xFF && entry->LDIR_Name3[j * 2 + 1] != 0xFF; j++)
        {
            longName[pos++] = entry->LDIR_Name3[j * 2] + (entry->LDIR_Name3[j * 2 + 1] << 8);
        }
    }
    longName[pos] = '\0';
    wcscpy(output, longName);
}


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

// compares names 
bool compareDirName(const char *name, const DirectoryEntry *entry)
{
    char formattedName[13];
    formatFileName(entry->DIR_Name, formattedName); 
    return strcmp(name, formattedName) == 0;
}

int findDirEntry(int fd, uint16_t *fat, uint32_t currentCluster, const char *name, DirectoryEntry *foundEntry, uint16_t BPB_BytsPerSec, uint8_t BPB_SecPerClus, uint16_t BPB_RsvdSecCnt, uint8_t BPB_NumFATs, uint16_t BPB_FATSz16)
{


    DirectoryEntry entry;
    size_t bytesRead;
    off_t clusterSize = BPB_BytsPerSec * BPB_SecPerClus;
    char buffer[clusterSize]; 

    while (currentCluster < 0xFFF8)
    {
        // Calculate the byte offset for the current cluster
        off_t clusterOffset = ((currentCluster - 2) * BPB_SecPerClus + BPB_RsvdSecCnt + BPB_NumFATs * BPB_FATSz16) * BPB_BytsPerSec;

        
        lseek(fd, clusterOffset, SEEK_SET);
        bytesRead = read(fd, buffer, clusterSize);

        
        for (int i = 0; i < bytesRead; i += sizeof(DirectoryEntry))
        {
            memcpy(&entry, buffer + i, sizeof(DirectoryEntry));

            // Check for end of entries
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
    const char *filePath = "fat16 (1).img"; // file Path 

    int fileDescriptor = open(filePath, O_RDONLY);
    if (fileDescriptor < 0) {
        perror("Error opening file");
        return 1;
    }

    // Read the boot sector
    Fat16BootSector bootSector;
    if (read(fileDescriptor, &bootSector, sizeof(bootSector)) < sizeof(bootSector)) {
        perror("Error reading boot sector");
        close(fileDescriptor);
        return 1;
    }

    
    off_t rootDirOffset = (bootSector.BPB_RsvdSecCnt + bootSector.BPB_NumFATs * bootSector.BPB_FATSz16) * bootSector.BPB_BytsPerSec;
    size_t rootDirSize = bootSector.BPB_RootEntCnt * sizeof(DirectoryEntry);
    
    // Allocate memory f
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

    // Iterate through the root directory 
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

    free(rootDir);
    close(fileDescriptor);

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int main() {
    const char *filePath = "fat16 (1).img"; // the  file path
    int fileDescriptor;
    off_t offset = 40960; 
    size_t bytesToRead = 512; //byte count, 
    ssize_t bytesRead;
    char *buffer;

    // memoru for the buffer
    buffer = (char *)malloc(bytesToRead);
    if (buffer == NULL) {
        perror("Error allocating memory");
        return 1;
    }

    // Open the file
    fileDescriptor = open(filePath, O_RDONLY);
    if (fileDescriptor < 0) {
        perror("Error opening file");
        free(buffer);
        return 1;
    }

    // Move to the specified location
    if (lseek(fileDescriptor, offset, SEEK_SET) < 0) {
        perror("Error seeking in file");
        close(fileDescriptor);
        free(buffer);
        return 1;
    }

    // Read bytes
    bytesRead = read(fileDescriptor, buffer, bytesToRead);
    if (bytesRead < 0) {
        perror("Error reading file");
        close(fileDescriptor);
        free(buffer);
        return 1;
    }

    // prints
    printf("Read %ld bytes from the file:\n", bytesRead);
    for (ssize_t i = 0; i < bytesRead; ++i) {
        printf("%c", buffer[i]);
    }
    printf("\n");

    // Close tbe file
    close(fileDescriptor);
    free(buffer);

    return 0;
}

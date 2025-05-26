#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(void)
{
    printf("Testing file access logging...\n");
    
    // Create a test file
    int fd = open("testfile.txt", O_CREATE | O_WRONLY);
    sleep(10);
    if(fd >= 0) {
        printf("Created testfile.txt\n");
        
        // Write some data
        char data[] = "Hello, xv6 logging system!";
        write(fd, data, strlen(data));
        sleep(10);
        printf("Wrote %d bytes to testfile.txt\n", strlen(data));
        
        close(fd);
        sleep(10);
        printf("Closed testfile.txt\n");
    }
    
    // Read the file back
    fd = open("testfile.txt", O_RDONLY);
    sleep(10);

    if(fd >= 0) {
        char buffer[100];
        int n = read(fd, buffer, sizeof(buffer));
        printf("Read %d bytes from testfile.txt\n", n);
        close(fd);
    }
    
    // Try to access non-existent file
    fd = open("nonexistent.txt", O_RDONLY);
    sleep(10);
    
    if(fd < 0) {
        printf("Failed to open nonexistent.txt (this will be logged)\n");
    }
    
    printf("Test complete. Run 'showlogs' to see logged activities.\n");
    exit(0);
}
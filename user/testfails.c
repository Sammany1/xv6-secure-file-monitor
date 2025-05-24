#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
    printf("Testing different types of failed access attempts...\n\n");
    
    // Test 1: File not found errors
    printf("1. Testing FILE_NOT_FOUND errors:\n");
    int fd1 = open("nonexistent.txt", 0);
    if(fd1 < 0) {
        printf("   ✓ Failed to open nonexistent.txt (expected: FILE_NOT_FOUND)\n");
    }
    
    int fd2 = open("/invalid/deep/path/file.txt", 0);
    if(fd2 < 0) {
        printf("   ✓ Failed to open invalid deep path (expected: FILE_NOT_FOUND)\n");
    }
    
    // Test 2: Permission denied errors
    printf("\n2. Testing PERMISSION_DENIED errors:\n");
    
    // Try to write to a directory
    int fd3 = open(".", 1); // Try to open current directory for writing
    if(fd3 < 0) {
        printf("   ✓ Failed to open directory for writing (expected: PERMISSION_DENIED)\n");
    }
    
    // Try to remove current directory
    if(unlink(".") < 0) {
        printf("   ✓ Failed to unlink current directory (expected: PERMISSION_DENIED)\n");
    }
    
    // Test 3: Access errors
    printf("\n3. Testing ACCESS_ERROR scenarios:\n");
    
    // Try to change to a file as if it were a directory
    // First create a file, then try to chdir to it
    int fd4 = open("testfile", 0x200); // O_CREATE
    if(fd4 >= 0) {
        close(fd4);
        if(chdir("testfile") < 0) {
            printf("   ✓ Failed to chdir to a file (expected: ACCESS_ERROR)\n");
        }
        unlink("testfile"); // cleanup
    }
    
    // Test 4: Try operations on non-existent directories
    printf("\n4. Testing more FILE_NOT_FOUND scenarios:\n");
    
    if(chdir("nonexistent_directory") < 0) {
        printf("   ✓ Failed to chdir to non-existent directory (expected: FILE_NOT_FOUND)\n");
    }
    
    if(unlink("another_nonexistent_file.txt") < 0) {
        printf("   ✓ Failed to unlink non-existent file (expected: FILE_NOT_FOUND)\n");
    }
    
    if(mkdir("/invalid/path/newdir") < 0) {
        printf("   ✓ Failed to mkdir with invalid path (expected: PERMISSION_DENIED)\n");
    }
    
    printf("\nTest complete! Run the following commands to see results:\n");
    printf("  showfails     - Basic view\n");
    printf("  showfails -v  - Detailed view with descriptions\n");
    printf("  showfails -p %d  - Show only failures from this test process\n", getpid());
    
    exit(0);
}
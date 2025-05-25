#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if(argc > 1 && strcmp(argv[1], "-c") == 0) {
        // Clear logs
        clear_logs();
        printf("File access logs cleared.\n");
        exit(0);
    }
    
    if(argc > 1 && strcmp(argv[1], "-s") == 0) {
        // Show stats for specific file
        if(argc < 3) {
            printf("Usage: showlogs -s <filename>\n");
            exit(1);
        }
        
        struct file_stats stats;
        if(get_file_stats(argv[2], &stats) < 0) {
            printf("Error getting stats for %s\n", argv[2]);
            exit(1);
        }
        
        printf("Statistics for file: %s\n", argv[2]);
        printf("Total accesses: %d\n", stats.total_accesses);
        printf("Read operations: %d (%d bytes)\n", stats.read_count, stats.total_bytes_read);
        printf("Write operations: %d (%d bytes)\n", stats.write_count, stats.total_bytes_written);
        exit(0);
    }
    
    // Show recent file access logs - reduced buffer size
    struct file_access_log logs[20];  // Reduced from 100 to 20
    int count = get_file_logs(logs, 20);
    
    if(count < 0) {
        printf("Error retrieving file access logs\n");
        exit(1);
    }
    
    printf("Recent File Access Log (%d entries):\n", count);
    printf("PID\tProcess\t\tOperation\tFile\t\tBytes\tStatus\tTime\n");
    printf("---\t-------\t\t---------\t----\t\t-----\t------\t----\n");
    
    for(int i = 0; i < count; i++) {
        printf("%d\t%s\t\t%s\t\t%s\t%d\t%s\t%d\n",
               logs[i].pid,
               logs[i].proc_name,
               logs[i].operation,
               logs[i].filename,
               logs[i].bytes_transferred,
               logs[i].success ? "OK" : "FAIL",
               logs[i].timestamp);
    }
    
    exit(0);
}
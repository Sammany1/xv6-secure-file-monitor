#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
    printf("File Access Monitor - Press Ctrl+C to stop\n");
    printf("Monitoring file system activity...\n\n");
    
    int last_count = 0;
    
    while(1) {
        struct file_access_log logs[5];  // Reduced from 10 to 5
        int count = get_file_logs(logs, 5);
        
        if(count > last_count) {
            // New activity detected
            for(int i = last_count; i < count; i++) {
                printf("[%d] %s(%d): %s %s (%d bytes) - %s\n",
                       logs[i].timestamp,
                       logs[i].proc_name,
                       logs[i].pid,
                       logs[i].operation,
                       logs[i].filename,
                       logs[i].bytes_transferred,
                       logs[i].success ? "SUCCESS" : "FAILED");
            }
            last_count = count;
        }
        
        sleep(100);  // Wait 1 second
    }
    
    exit(0);
}
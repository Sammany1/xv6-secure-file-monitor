#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void pad(const char *s, int width) {
    int len = strlen(s);
    if(len > width) {
        char tmp[width+1];
        for(int i=0; i<width; i++) {
            tmp[i] = s[i];
        }
        tmp[width] = '\0';
        printf("%s", tmp);
    }
    else{
        printf("%s", s);
    }
    for(int i = len; i < width; i++)
        printf(" ");
}

void pad_num(int num, int width) {
    char buf[16];
    int len = 0, n = num, i = 0;

    // Convert number to string (handle 0 specially)
    if(n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        len = 1;
    } else {
        // Convert to string in reverse
        while(n > 0 && i < sizeof(buf)-1) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
        buf[i] = '\0';
        // Reverse the string
        for(int j = 0; j < i/2; j++) {
            char tmp = buf[j];
            buf[j] = buf[i-1-j];
            buf[i-1-j] = tmp;
        }
        len = i;
    }

    printf("%s", buf);
    for(int j = len; j < width; j++)
        printf(" ");
}

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
    struct file_access_log logs[20];  
    int count = get_file_logs(logs, 20);
    
    if(count < 0) {
        printf("Error retrieving file access logs\n");
        exit(1);
    }
    
    printf("Recent File Access Log (%d entries):\n", count);
    printf("PID    Process    Operation    File             Bytes    Status    Date\'Time                    \n");
    printf("---    -------    ---------    --------------   -----    ------    ------------------------\n");
    
    for(int i = 0; i < count; i++) {
        pad_num(logs[i].pid, 3);        printf("    ");
        pad(logs[i].proc_name, 7);     printf("    ");
        pad(logs[i].operation, 9);      printf("    ");
        pad(logs[i].filename, 14);      printf("    ");
        pad_num(logs[i].bytes_transferred, 5); printf("   ");
        pad(logs[i].status ? "OK" : "FAIL", 6); printf("    ");
        pad(logs[i].timestamp, 24); printf("\n");
    }
    
    exit(0);
}
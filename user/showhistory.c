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

// Modify the main function to use dynamic allocation

int
main(int argc, char *argv[])
{
    if(argc > 1 && strcmp(argv[1], "-c") == 0) {
        // Clear history logs
        clear_history_logs();
        printf("History storage cleared.\n");
        exit(0);
    }
    
    if(argc > 1 && strcmp(argv[1], "-s") == 0) {
        // Show history storage statistics
        int total_logs, total_chunks;
        get_history_stats(&total_logs, &total_chunks);
        
        printf("History Storage Statistics:\n");
        printf("Total logs stored: %d\n", total_logs);
        printf("Total chunks: %d\n", total_chunks);
        printf("Average logs per chunk: %d\n", total_chunks > 0 ? total_logs / total_chunks : 0);
        exit(0);
    }
    
    // Show logs from history storage
    int max_display = 100;  // Maximum logs to display at once
    int offset = 0;         // Start from beginning
    
    if(argc > 1) {
        // Allow user to specify how many logs to show
        max_display = atoi(argv[1]);
        if(max_display <= 0 || max_display > 200) {
            max_display = 100;  // Default fallback
        }
    }
    
    // Dynamically allocate memory instead of using stack
    struct file_access_log *logs = malloc(max_display * sizeof(struct file_access_log));
    if(!logs) {
        printf("Failed to allocate memory for logs\n");
        exit(1);
    }
    
    int count = get_history_logs(logs, max_display, offset);
    
    if(count < 0) {
        printf("Error retrieving history logs\n");
        free(logs); // Don't forget to free memory
        exit(1);
    }
    
    if(count == 0) {
        printf("No logs found in history storage.\n");
        printf("(Logs are transferred here when the 20-entry buffer fills up)\n");
        free(logs); // Don't forget to free memory
        exit(0);
    }
    
    // Rest of the function remains the same except we need to free memory at the end
    printf("History File Access Log (%d entries shown):\n", count);
    printf("PID    Process    Operation    File             Bytes    Status    Time\n");
    printf("---    -------    ---------    --------------   -----    ------    ----\n");
    
    for(int i = 0; i < count; i++) {
        pad_num(logs[i].pid, 3);        printf("    ");
        pad(logs[i].proc_name, 7);     printf("    ");
        pad(logs[i].operation, 9);      printf("    ");
        pad(logs[i].filename, 14);      printf("    ");
        pad_num(logs[i].bytes_transferred, 5); printf("    ");
        pad(logs[i].status ? "OK" : "FAIL", 6); printf("    ");
        pad_num(logs[i].timestamp, 4); printf("\n");
    }
    
    printf("\nNote: Use 'showhistory -s' for storage statistics\n");
    printf("      Use 'showhistory -c' to clear history storage\n");
    printf("      Use 'showhistory <number>' to show specific number of logs\n");
    
    free(logs);
    
    exit(0);
}
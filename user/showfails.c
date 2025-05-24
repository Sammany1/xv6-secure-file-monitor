#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char *failure_types[] = {"UNKNOWN", "PERMISSION_DENIED", "FILE_NOT_FOUND", "ACCESS_ERROR"};

void print_help(void)
{
    printf("showfails - Failed Access Log Viewer\n\n");
    printf("Usage:\n");
    printf("  showfails       - Basic view (show all failures)\n");
    printf("  showfails -v    - Detailed view with descriptions\n");
    printf("  showfails -p N  - Show only failures from process N\n");
    printf("  showfails -h    - Show this help\n\n");
    printf("Examples:\n");
    printf("  showfails       - Basic view\n");
    printf("  showfails -v    - Detailed view with descriptions\n");
    printf("  showfails -p 3  - Show only failures from process 3\n");
}

void print_detailed(struct failed_access_log *logs, int count)
{
    printf("Failed File Access Attempts (%d entries)\n\n", count);
    
    if(count == 0) {
        printf("No failures recorded. Run 'testfails' to generate test data.\n");
        return;
    }
    
    for(int i = 0; i < count; i++) {
        printf("Entry %d:\n", i + 1);
        printf("  Process: %s (PID: %d)\n", logs[i].proc_name, logs[i].pid);
        printf("  File: %s\n", logs[i].filename);
        printf("  Type: %s\n", failure_types[logs[i].failure_type]);
        printf("  Time: %d ticks\n", logs[i].timestamp);
        printf("\n");
    }
}

void print_summary(struct failed_access_log *logs, int count)
{
    int perm = 0, notfound = 0, access = 0;
    
    for(int i = 0; i < count; i++) {
        if(logs[i].failure_type == 1) perm++;
        else if(logs[i].failure_type == 2) notfound++;
        else if(logs[i].failure_type == 3) access++;
    }
    
    printf("Summary:\n");
    printf("  Permission denied: %d\n", perm);
    printf("  File not found: %d\n", notfound);
    printf("  Access errors: %d\n", access);
}

void print_table(struct failed_access_log *logs, int count)
{
    printf("Failed File Access Attempts (%d entries)\n\n", count);
    
    if(count == 0) {
        printf("No failures recorded. Run 'testfails' to generate test data.\n");
        return;
    }
    
    printf("PID  Process      File                      Type               Time\n");
    printf("---- ------------ ------------------------- ------------------ --------\n");
    
    for(int i = 0; i < count; i++) {
        // Simple filename truncation
        char filename[26];
        int len = strlen(logs[i].filename);
        if(len > 24) {
            strcpy(filename, logs[i].filename);
            filename[21] = '.';
            filename[22] = '.';
            filename[23] = '.';
            filename[24] = '\0';
        } else {
            strcpy(filename, logs[i].filename);
        }
        
        // Simple fixed-width printing with proper spacing
        printf("%d", logs[i].pid);
        if(logs[i].pid < 10) printf("    ");
        else if(logs[i].pid < 100) printf("   ");
        else printf("  ");
        
        printf("%s", logs[i].proc_name);
        int proc_len = strlen(logs[i].proc_name);
        for(int j = proc_len; j < 12; j++) printf(" ");
        printf(" ");
        
        printf("%s", filename);
        int file_len = strlen(filename);
        for(int j = file_len; j < 25; j++) printf(" ");
        printf(" ");
        
        printf("%s", failure_types[logs[i].failure_type]);
        int type_len = strlen(failure_types[logs[i].failure_type]);
        for(int j = type_len; j < 18; j++) printf(" ");
        printf(" ");
        
        printf("%d", logs[i].timestamp);
        printf("\n");
    }
    printf("\n");
    print_summary(logs, count);
}

int main(int argc, char *argv[])
{
    struct failed_access_log logs[50];
    int count;
    
    // Handle command line options
    if(argc > 1) {
        if(strcmp(argv[1], "-h") == 0) {
            print_help();
            exit(0);
        }
        
        if(strcmp(argv[1], "-p") == 0) {
            if(argc < 3) {
                printf("Usage: showfails -p <pid>\n");
                exit(1);
            }
            int pid = atoi(argv[2]);
            count = get_process_failures(pid);
            printf("Process %d has %d failed access attempts\n", pid, count);
            exit(0);
        }
        
        if(strcmp(argv[1], "-v") == 0) {
            count = get_failed_logs(logs, 50);
            if(count < 0) {
                printf("Error getting logs\n");
                exit(1);
            }
            print_detailed(logs, count);
            exit(0);
        }
    }
    
    // Default: show table view
    count = get_failed_logs(logs, 50);
    if(count < 0) {
        printf("Error getting logs\n");
        exit(1);
    }
    
    print_table(logs, count);
    
    // Show usage options
    printf("Usage options:\n");
    printf("  showfails     - Basic view\n");
    printf("  showfails -v  - Detailed view with descriptions\n");
    printf("  showfails -p 3  - Show only failures from this test process\n");
    printf("  showfails -h  - Show help information\n");
    
    exit(0);
}
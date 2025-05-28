#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// kernel/filelog.h is included via user.h, providing struct file_access_log
// and constants like FILENAME_MAX, OPERATION_MAX.

// Helper function to safely copy strings
static char*
safestrcpy_user(char *s, const char *t, int n)
{
  char *os;

  os = s;
  if(n <= 0)
    return os;
  while(--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}

// Local implementation of strncmp
static int
strncmp(const char *p, const char *q, uint n)
{
  while(n > 0 && *p && *p == *q)
    n--, p++, q++;
  if(n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

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

struct filters {
    int pid;
    char proc_name[16];
    char file_name[FILENAME_MAX];
    int status; // 0 = FAIL, 1 = OK, -1 = no filter
    int filter_by_pid;
    int filter_by_proc_name;
    int filter_by_file_name;
    int filter_by_status;
};

void print_help() {
    printf("\nUsage: showhistory [options] [number_of_logs_to_fetch]\n");
    printf("Options:\n");
    printf("  -c                Clear history logs\n");
    printf("  -s                Show history storage statistics\n");
    printf("  --pid <pid>       Filter by process ID\n");
    printf("  -p <proc_name>    Filter by process name\n");
    printf("  -f <file_name>    Filter by file name\n");
    printf("  --status <OK|FAIL> Filter by operation status\n");
    printf("  --help            Show this help message\n");
    printf("If [number_of_logs_to_fetch] is not specified, defaults to 100 (max 200).\n");
}

int
main(int argc, char *argv[])
{
    struct filters f;
    memset(&f, 0, sizeof(f));
    f.status = -1; // No status filter by default

    int max_display = -1; // Sentinel: -1 means not set by user yet
    int offset = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            clear_history_logs();
            printf("History storage cleared.\n");
            exit(0);
        } else if (strcmp(argv[i], "-s") == 0) {
            int total_logs, total_chunks;
            get_history_stats(&total_logs, &total_chunks);
            printf("History Storage Statistics:\n");
            printf("Total logs stored: %d\n", total_logs);
            printf("Total chunks: %d\n", total_chunks);
            printf("Average logs per chunk: %d\n", total_chunks > 0 ? total_logs / total_chunks : 0);
            exit(0);
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            exit(0);
        } else if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            f.pid = atoi(argv[++i]);
            f.filter_by_pid = 1;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            safestrcpy_user(f.proc_name, argv[++i], sizeof(f.proc_name));
            f.filter_by_proc_name = 1;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            safestrcpy_user(f.file_name, argv[++i], sizeof(f.file_name));
            f.filter_by_file_name = 1;
        } else if (strcmp(argv[i], "--status") == 0 && i + 1 < argc) {
            char* status_str = argv[++i];
            if (strcmp(status_str, "OK") == 0) {
                f.status = 1;
                f.filter_by_status = 1;
            } else if (strcmp(status_str, "FAIL") == 0) {
                f.status = 0;
                f.filter_by_status = 1;
            } else {
                printf("Error: Invalid status '%s'. Use 'OK' or 'FAIL'.\n", status_str);
                print_help();
                exit(1);
            }
        } else if (argv[i][0] != '-' && max_display == -1) { 
            max_display = atoi(argv[i]);
        } else {
            printf("Unknown or misplaced argument: %s\n", argv[i]);
            print_help();
            exit(1);
        }
    }

    if (max_display == -1) { 
        max_display = 100; // Default if not specified
    } else if (max_display <= 0 || max_display > 200) {
        printf("Requested log count %d out of range (1-200). Using 100.\n", max_display);
        max_display = 100;
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
        free(logs); 
        exit(1);
    }
    
    if(count == 0) {
        printf("No logs found in history storage.\n");
        printf("(Logs are transferred here when the 20-entry buffer fills up)\n");
        free(logs); 
        exit(0);
    }
    
    printf("History File Access Log (fetched %d entries, applying filters):\n", count);
    printf("PID    Process    Operation    File             Bytes    Status    Time\n");
    printf("---    -------    ---------    --------------   -----    ------    ----\n");
    
    int displayed_count = 0;
    for(int i = 0; i < count; i++) {
        struct file_access_log *current_log = &logs[i];
        int match = 1;

        if (f.filter_by_pid && current_log->pid != f.pid) {
            match = 0;
        }
        if (match && f.filter_by_proc_name && strncmp(current_log->proc_name, f.proc_name, sizeof(current_log->proc_name)) != 0) {
            match = 0;
        }
        if (match && f.filter_by_file_name && strncmp(current_log->filename, f.file_name, sizeof(current_log->filename)) != 0) {
            match = 0;
        }
        if (match && f.filter_by_status && current_log->status != f.status) {
            match = 0;
        }

        if (match) {
            pad_num(logs[i].pid, 3);        printf("    ");
            pad(logs[i].proc_name, 7);     printf("    ");
            pad(logs[i].operation, 9);      printf("    ");
            pad(logs[i].filename, 14);      printf("    ");
            pad_num(logs[i].bytes_transferred, 5); printf("    ");
            pad(logs[i].status ? "OK" : "FAIL", 6); printf("    ");
            pad_num(logs[i].timestamp, 4); printf("\n");
            displayed_count++;
        }
    }
    
    if (displayed_count == 0 && count > 0) {
        printf("No logs matched the specified filters from the %d fetched entries.\n", count);
    }
    printf("\nDisplayed %d of %d fetched entries.\n", displayed_count, count);
    printf("Use 'showhistory --help' for all options.\n");
    
    free(logs);
    
    exit(0);
}

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


#define MAX_BIG 1024     


/*  Pretty‑printing helpers                      */

void pad(const char *s, int width) {
    int len = strlen(s);
    if(len > width) {
        char tmp[width+1];
        for(int i=0; i<width; i++)
            tmp[i] = s[i];
        tmp[width] = '\0';
        printf("%s", tmp);
    } else {
        printf("%s", s);
    }
    for(int i=len; i<width; i++)
        printf(" ");
}

void pad_num(int num, int width) {
    char buf[16];
    int len = 0, n = num, i = 0;

    if(n == 0) {
        buf[0] = '0'; buf[1] = '\0'; len = 1;
    } else {
        while(n > 0 && i < sizeof(buf)-1) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
        buf[i] = '\0';
        for(int j=0; j<i/2; j++) {
            char t = buf[j];
            buf[j] = buf[i-1-j];
            buf[i-1-j] = t;
        }
        len = i;
    }
    printf("%s", buf);
    for(int j=len; j<width; j++)
        printf(" ");
}

/*  main                                                              */
int
main(int argc, char *argv[])
{
    /* clear big logs*/
    if(argc > 1 && strcmp(argv[1], "-c") == 0) {
        if(clear_big_logs() < 0) {
            printf("Failed to clear big log\n");
            exit(1);
        }
        printf("Big file‑access log cleared.\n");
        exit(0);
    }

    /* ---- per‑file stats ----------------------------------------- */
    if(argc > 1 && strcmp(argv[1], "-s") == 0) {
        if(argc < 3) {
            printf("Usage: showbiglogs -s <filename>\n");
            exit(1);
        }
        struct file_stats st;
        if(get_file_stats(argv[2], &st) < 0) {
            printf("Error getting stats for %s\n", argv[2]);
            exit(1);
        }
        printf("Statistics for file: %s\n", argv[2]);
        printf("Total accesses: %d\n", st.total_accesses);
        printf("Read operations : %d (%d bytes)\n", st.read_count,  st.total_bytes_read);
        printf("Write operations: %d (%d bytes)\n", st.write_count, st.total_bytes_written);
        exit(0);
    }

    /* ---- how many entries to show? ------------------------------ */
    int limit = MAX_BIG;
    if(argc > 1 && strcmp(argv[1], "-n") == 0) {
        if(argc < 3) {
            printf("Usage: showbiglogs -n <num>\n");
            exit(1);
        }
        limit = atoi(argv[2]);
        if(limit <= 0 || limit > MAX_BIG)
            limit = MAX_BIG;
    }

    struct file_access_log logs[MAX_BIG];
    int count = get_big_file_logs(logs, MAX_BIG);
    if(count < 0) {
        printf("Error retrieving big file log\n");
        exit(1);
    }

    if(limit < count)
        count = limit;

    printf("Big File‑Access Log (most‑recent %d of %d stored):\n", count, MAX_BIG);
    printf("PID    Process    Operation    File             Bytes    Status    Time\n");
    printf("---    -------    ---------    --------------   -----    ------    ----\n");

    /* Entries are returned oldest‑to‑newest; pick the last <count> */
    int start = (count == MAX_BIG) ? 0 : (MAX_BIG + count - count) % MAX_BIG;
    for(int i = MAX_BIG - count; i < MAX_BIG; i++) {
        int idx = i % MAX_BIG;
        if(!logs[idx].valid) continue;
        pad_num(logs[idx].pid, 3);        printf("    ");
        pad(logs[idx].proc_name, 7);      printf("    ");
        pad(logs[idx].operation, 9);      printf("    ");
        pad(logs[idx].filename, 14);      printf("    ");
        pad_num(logs[idx].bytes_transferred, 5); printf("    ");
        pad(logs[idx].status ? "OK" : "FAIL", 6); printf("    ");
        pad_num(logs[idx].timestamp, 4);  printf("\n");
    }

    exit(0);
}


/*  Prototypes for the new syscalls (mirrors user.h updates)          */
extern int  get_big_file_logs(struct file_access_log *, int);
extern int  clear_big_logs(void);

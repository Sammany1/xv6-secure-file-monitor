#ifndef FILELOG_H
#define FILELOG_H

#define MAX_LOG_ENTRIES 20
#define FILENAME_MAX 64
#define OPERATION_MAX 16

// Operation types
#define OP_OPEN   1
#define OP_READ   2
#define OP_WRITE  3
#define OP_CLOSE  4
#define OP_CREATE 5
#define OP_DELETE 6

struct file_access_log {
    int pid;
    char proc_name[16];
    char filename[FILENAME_MAX];
    char operation[OPERATION_MAX];
    int bytes_transferred;
    int status;  // 1 for success, 0 for failure
    char timestamp[25];
    int valid;
};

struct file_stats {
    int total_accesses;
    int read_count;
    int write_count;
    int total_bytes_read;
    int total_bytes_written;
};

#endif
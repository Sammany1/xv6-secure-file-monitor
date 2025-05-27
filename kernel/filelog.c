#include "types.h"
#include "riscv.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

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

extern struct spinlock tickslock;

struct file_access_log {
    int pid;
    char proc_name[16];
    char filename[FILENAME_MAX];
    char operation[OPERATION_MAX];
    int bytes_transferred;
    int status;  // 1 for status, 0 for failure
    uint timestamp;
    int valid;
};

struct {
    struct spinlock lock;
    struct file_access_log entries[MAX_LOG_ENTRIES];
    int next_index;
    int total_accesses;
} access_log_buffer;


// Initialize the file access logging system
void
filelog_init(void)
{
    initlock(&access_log_buffer.lock, "filelog");
    access_log_buffer.next_index = 0;
    access_log_buffer.total_accesses = 0;

    for(int i=0; i<MAX_LOG_ENTRIES; i++){
        access_log_buffer.entries[i].valid = 0;
    }
    detector_init();
}


// Helper function to determine if we should log this process
static int
should_log_process(char *proc_name)
{
    // Only skip logging for showlogs utility and init
    if(strncmp(proc_name, "showlogs", 8) == 0) return 0;
    if(strncmp(proc_name, "init", 4) == 0) return 0;
    if(strncmp(proc_name, "ls", 2) == 0) return 0;
    return 1; // log all other processes, including sh, echo, ls, etc.
}

// Helper function to determine if operation should be logged
static int
should_log_operation(char *proc_name, char *operation, int bytes, int file_type)
{
    // Always log these important operations
    if(strncmp(operation, "CREATE", 6) == 0) return 1;
    if(strncmp(operation, "DELETE", 6) == 0) return 1;
    if(strncmp(operation, "OPEN", 4) == 0) return 1;

    // For READ/WRITE operations, apply filters
    if(strncmp(operation, "READ", 4) == 0) {
        // Only log reads of significant size from files (not devices)
        return (bytes > 10 && file_type == 1); // 1 = file, 0 = device
    }
    if(strncmp(operation, "WRITE", 5) == 0) {
        // Log all writes to files (not stdout), even small ones
        return (file_type == 1);
    }
    if(strncmp(operation, "CLOSE", 5) == 0) {
        // Only log file closes (not device closes)
        return (file_type == 1);
    }
    return 1; // other operations by default
}

// Main logging function with built-in filtering
void
log_file_access(int pid, char *proc_name, char *operation, char *filename, int bytes, int status)
{
    // First filter: check if we should log this process
    if(!should_log_process(proc_name)) {
        return;
    }

    // Determine file type: 1 for real files, 0 for devices/stdout
    int file_type = 1; // Default to file
    if(strncmp(filename, "console", 7) == 0) file_type = 0;
    if(strncmp(filename, "device", 6) == 0) file_type = 0;
    if(strncmp(filename, "stdout", 6) == 0) file_type = 0;

    // Second filter: check if we should log this operation
    if(!should_log_operation(proc_name, operation, bytes, file_type)) {
        return;
    }

    // detect suspicious activity
    check_suspicious(pid, proc_name, operation, filename, status);

    // Passed all filters, now log it
    acquire(&access_log_buffer.lock);
    struct file_access_log *entry = &access_log_buffer.entries[access_log_buffer.next_index];
    entry->pid = pid;
    safestrcpy(entry->proc_name, proc_name, sizeof(entry->proc_name));
    safestrcpy(entry->filename, filename, sizeof(entry->filename));
    safestrcpy(entry->operation, operation, sizeof(entry->operation));
    entry->bytes_transferred = bytes;
    entry->timestamp = ticks;
    entry->status = status;
    entry->valid = 1;
    access_log_buffer.next_index = (access_log_buffer.next_index + 1) % MAX_LOG_ENTRIES;
    access_log_buffer.total_accesses++;
    release(&access_log_buffer.lock);
}

// Get recent file access logs
int
get_file_logs(uint64 user_buf, int max_entries)
{
    int count = 0;
    
    // Limit max_entries to prevent buffer overflow
    if(max_entries > MAX_LOG_ENTRIES) {
        max_entries = MAX_LOG_ENTRIES;
    }
    
    acquire(&access_log_buffer.lock);
    
    // Copy most recent entries one by one directly to user space
    for(int i = 0; i < MAX_LOG_ENTRIES && count < max_entries; i++) {
        int index = (access_log_buffer.next_index - 1 - i + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
        if(access_log_buffer.entries[index].valid) {
            if(copyout(myproc()->pagetable, 
                      user_buf + (count * sizeof(struct file_access_log)), 
                      (char*)&access_log_buffer.entries[index], 
                      sizeof(struct file_access_log)) < 0) {
                release(&access_log_buffer.lock);
                return -1;
            }
            count++;
        }
    }
    
    release(&access_log_buffer.lock);
    return count;
}

// Get access statistics for a file
int
get_file_stats(char *filename, uint64 user_stats)
{
    struct {
        int total_accesses;
        int read_count;
        int write_count;
        int total_bytes_read;
        int total_bytes_written;
    } stats;
    
    stats.total_accesses = 0;
    stats.read_count = 0;
    stats.write_count = 0;
    stats.total_bytes_read = 0;
    stats.total_bytes_written = 0;
    
    acquire(&access_log_buffer.lock);
    
    for(int i = 0; i < MAX_LOG_ENTRIES; i++) {
        if(access_log_buffer.entries[i].valid && strncmp(access_log_buffer.entries[i].filename, filename, FILENAME_MAX) == 0) {
            stats.total_accesses++;
            
            if(strncmp(access_log_buffer.entries[i].operation, "READ", 4) == 0) {
                stats.read_count++;
                stats.total_bytes_read += access_log_buffer.entries[i].bytes_transferred;
            } else if(strncmp(access_log_buffer.entries[i].operation, "WRITE", 5) == 0) {
                stats.write_count++;
                stats.total_bytes_written += access_log_buffer.entries[i].bytes_transferred;
            }
        }
    }
    
    release(&access_log_buffer.lock);
    
    if(copyout(myproc()->pagetable, user_stats, (char*)&stats, sizeof(stats)) < 0) {
        return -1;
    }
    
    return 0;
}

// Clear all logs (for administrative purposes)
void
clear_file_logs(void)
{
    acquire(&access_log_buffer.lock);

    for(int i = 0; i < MAX_LOG_ENTRIES; i++) {
        access_log_buffer.entries[i].valid = 0;
    }
    
    access_log_buffer.next_index = 0;
    access_log_buffer.total_accesses = 0;
    
    release(&access_log_buffer.lock);
}
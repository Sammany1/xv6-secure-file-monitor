#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define MAX_FAIL_ENTRIES 50
// 50 * 60 bytes = 3KB
// Kernel stack is typically only 4KB in xv6
// if we want bigger buffer use dynamic allocation
#define FILENAME_MAX 32

// Failure types
#define FAIL_PERMISSION 1
#define FAIL_NOT_FOUND  2
#define FAIL_ACCESS     3

struct failed_access_log {
    int pid;
    char proc_name[16];
    char filename[FILENAME_MAX];
    int failure_type;
    uint timestamp;
    int valid;
};

struct {
    struct spinlock lock;
    struct failed_access_log entries[MAX_FAIL_ENTRIES];
    int next_index;
    int total_failures;
} fail_log_buffer;

// Initialize the failed access logging system
void
faillog_init(void)
{
    initlock(&fail_log_buffer.lock, "faillog");
    fail_log_buffer.next_index = 0;
    fail_log_buffer.total_failures = 0;
    
    for(int i = 0; i < MAX_FAIL_ENTRIES; i++) {
        fail_log_buffer.entries[i].valid = 0;
    }
}

// Log a failed file access attempt
void
log_failed_access(int pid, char *proc_name, char *filename, int failure_type)
{
    acquire(&fail_log_buffer.lock);
    
    struct failed_access_log *entry = &fail_log_buffer.entries[fail_log_buffer.next_index];
    
    entry->pid = pid;
    safestrcpy(entry->proc_name, proc_name, sizeof(entry->proc_name));
    safestrcpy(entry->filename, filename, sizeof(entry->filename));
    entry->failure_type = failure_type;
    entry->timestamp = ticks;
    entry->valid = 1;
    
    fail_log_buffer.next_index = (fail_log_buffer.next_index + 1) % MAX_FAIL_ENTRIES;
    fail_log_buffer.total_failures++;
    
    release(&fail_log_buffer.lock);
}

// Get failed access logs
int
get_failed_logs(uint64 user_buf, int max_entries)
{

    if(max_entries <= 0 || max_entries > 100) {
        return 0;  // Return 0 entries for invalid input
    }

    struct failed_access_log temp_entries[MAX_FAIL_ENTRIES];
    int count = 0;
    
    acquire(&fail_log_buffer.lock);
    
    for(int i = 0; i < MAX_FAIL_ENTRIES && count < max_entries; i++) {
        if(fail_log_buffer.entries[i].valid) {
            temp_entries[count] = fail_log_buffer.entries[i];
            count++;
        }
    }
    
    release(&fail_log_buffer.lock);
    
    if(copyout(myproc()->pagetable, user_buf, (char*)temp_entries, 
               count * sizeof(struct failed_access_log)) < 0) {
        return -1;
    }
    
    return count;
}

// Get failure statistics for a specific process
int
get_process_failures(int target_pid)
{
    int failure_count = 0;
    
    acquire(&fail_log_buffer.lock);
    
    for(int i = 0; i < MAX_FAIL_ENTRIES; i++) {
        if(fail_log_buffer.entries[i].valid && 
           fail_log_buffer.entries[i].pid == target_pid) {
            failure_count++;
        }
    }
    
    release(&fail_log_buffer.lock);
    
    return failure_count;
}

uint64
sys_get_failed_logs(void)
{
    uint64 user_buf;
    int max_entries;
    
    // Extract arguments from user space
    argaddr(0, &user_buf);
    argint(1, &max_entries);
    
    // Call the core function
    return get_failed_logs(user_buf, max_entries);
}

// System call wrapper for get_process_failures  
uint64
sys_get_process_failures(void)
{
    int target_pid;
    
    // Extract argument from user space
    argint(0, &target_pid);
    
    // Call the core function
    return get_process_failures(target_pid);
}
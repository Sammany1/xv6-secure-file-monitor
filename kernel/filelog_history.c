#include "types.h"
#include "riscv.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "filelog.h"

#define CHUNK_SIZE 20  // Same as short-term buffer size
#define MAX_CHUNKS 50  // Reduced to be more conservative with memory

struct file_access_log_chunk {
    struct file_access_log logs[CHUNK_SIZE];
    struct file_access_log_chunk *next;
    int count;  // number of logs in this chunk (0-CHUNK_SIZE)
    uint transfer_time; // when this chunk was created
};

struct history_storage {
    struct spinlock lock;
    struct file_access_log_chunk *head;
    struct file_access_log_chunk *tail;
    int total_logs;
    int total_chunks;
} history_log_storage;

// Initialize history storage
void
history_log_init(void)
{
    initlock(&history_log_storage.lock, "history_log");
    history_log_storage.head = 0;
    history_log_storage.tail = 0;
    history_log_storage.total_logs = 0;
    history_log_storage.total_chunks = 0;
}

// Transfer logs from short-term buffer to history storage
int
transfer_to_history(struct file_access_log *buffer, int count)
{
    if(count <= 0 || count > CHUNK_SIZE) {
        return -1;
    }

    // Check if we've reached maximum chunks (memory management)
    acquire(&history_log_storage.lock);
    if(history_log_storage.total_chunks >= MAX_CHUNKS) {
        // Remove oldest chunk to make space
        if(history_log_storage.head) {
            struct file_access_log_chunk *old_head = history_log_storage.head;
            history_log_storage.head = old_head->next;
            if(history_log_storage.head == 0) {
                history_log_storage.tail = 0;
            }
            history_log_storage.total_logs -= old_head->count;
            history_log_storage.total_chunks--;
            kfree(old_head);
        }
    }
    release(&history_log_storage.lock);

    // Allocate new chunk
    struct file_access_log_chunk *new_chunk = (struct file_access_log_chunk*)kalloc();
    if(!new_chunk) {
        printf("Error: Failed to allocate memory for history log chunk\n");
        return -1;
    }

    // Initialize the chunk properly
    memset(new_chunk, 0, sizeof(struct file_access_log_chunk));
    
    // Copy logs from buffer to chunk
    for(int i = 0; i < count; i++) {
        new_chunk->logs[i] = buffer[i];
    }
    new_chunk->count = count;
    new_chunk->next = 0;
    new_chunk->transfer_time = ticks;

    // Add chunk to linked list
    acquire(&history_log_storage.lock);
    if(!history_log_storage.head) {
        history_log_storage.head = history_log_storage.tail = new_chunk;
    } else {
        history_log_storage.tail->next = new_chunk;
        history_log_storage.tail = new_chunk;
    }
    
    history_log_storage.total_logs += count;
    history_log_storage.total_chunks++;
    release(&history_log_storage.lock);

    return 0;
}

// Get logs from history storage (for system calls)
int
get_history_logs(uint64 user_buf, int max_entries, int offset)
{
    int copied = 0;
    int current_offset = 0;

    if(max_entries <= 0) {
        return 0;
    }

    acquire(&history_log_storage.lock);
    
    struct file_access_log_chunk *chunk = history_log_storage.head;
    while(chunk && copied < max_entries) {
        // Skip chunks until we reach the offset
        if(current_offset + chunk->count <= offset) {
            current_offset += chunk->count;
            chunk = chunk->next;
            continue;
        }

        // Copy logs from this chunk
        int start_index = (offset > current_offset) ? offset - current_offset : 0;
        for(int i = start_index; i < chunk->count && copied < max_entries; i++) {
            if(copyout(myproc()->pagetable, 
                      user_buf + (copied * sizeof(struct file_access_log)), 
                      (char*)&chunk->logs[i], 
                      sizeof(struct file_access_log)) < 0) {
                release(&history_log_storage.lock);
                return -1;
            }
            copied++;
        }
        
        current_offset += chunk->count;
        chunk = chunk->next;
    }
    
    release(&history_log_storage.lock);
    return copied;
}

// Get history storage statistics
void
get_history_stats(int *total_logs, int *total_chunks)
{
    acquire(&history_log_storage.lock);
    *total_logs = history_log_storage.total_logs;
    *total_chunks = history_log_storage.total_chunks;
    release(&history_log_storage.lock);
}

// Clear history storage (free all chunks)
void
clear_history_logs(void)
{
    acquire(&history_log_storage.lock);
    
    struct file_access_log_chunk *chunk = history_log_storage.head;
    while(chunk) {
        struct file_access_log_chunk *next = chunk->next;
        kfree(chunk);
        chunk = next;
    }
    
    history_log_storage.head = 0;
    history_log_storage.tail = 0;
    history_log_storage.total_logs = 0;
    history_log_storage.total_chunks = 0;
    
    release(&history_log_storage.lock);
}
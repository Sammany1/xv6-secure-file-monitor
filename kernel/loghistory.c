#include "types.h"
#include "riscv.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#ifndef BIG_LOG_ENTRIES
#define BIG_LOG_ENTRIES 1024   // adjust size if needed
#endif

#ifndef FILENAME_MAX
#define FILENAME_MAX 64
#endif

#ifndef OPERATION_MAX
#define OPERATION_MAX 16
#endif

// Definition of the structure that both logs share.
// If it has already been defined elsewhere, this will be
// an identical re‑declaration (allowed by the C standard).
struct file_access_log {
  char filename[FILENAME_MAX];
  char operation[OPERATION_MAX];
  int  bytes_transferred;
  int  timestamp;
  int  status;
  int  valid;
};

static struct {
  struct spinlock lock;
  struct file_access_log entries[BIG_LOG_ENTRIES];
  int next_idx;
  int total;
} biglog;

void
loghistory_init(void)
{
  initlock(&biglog.lock, "filelog_history");
  biglog.next_idx = 0;
  biglog.total = 0;
  for(int i = 0; i < BIG_LOG_ENTRIES; i++){
    biglog.entries[i].valid = 0;
  }
}

void
push_loghistory(struct file_access_log *src)
{
  if(src == 0) return;
  acquire(&biglog.lock);
  biglog.entries[biglog.next_idx] = *src;   // shallow copy
  biglog.entries[biglog.next_idx].valid = 1;
  biglog.next_idx = (biglog.next_idx + 1) % BIG_LOG_ENTRIES;
  biglog.total++;
  release(&biglog.lock);
}

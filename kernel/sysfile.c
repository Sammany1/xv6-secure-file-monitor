//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Assume 'ticks' is a global volatile uint updated by timer interrupts.
// If not, you need to ensure it's accessible here. For example:
// extern volatile uint ticks;


// Helper function prototype for suspicious activity (conceptual, might be inlined or elsewhere)
 void check_and_log_suspicious_file_access(struct proc *p, const char *operation_type);


// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  argint(n, &fd);
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

// --- Helper for "Many Files Quickly" Detection ---
// This function updates and checks the count of file operations within a time window.
// Note: This simplified version counts total operations, not necessarily *different* files.
static void
track_file_operation(struct proc *p, const char *op_name)
{
  if (!p) return;

  // Check if the time window has reset
  if (ticks - p->last_file_access_tick > FILE_ACCESS_TIME_WINDOW) {
    p->files_accessed_in_window = 1; // Start new window with current operation
  } else {
    p->files_accessed_in_window++;
  }
  p->last_file_access_tick = ticks; // Update last access time

  // Check threshold for too many operations quickly
  if (p->files_accessed_in_window > MAX_FILES_ACCESSED_QUICKLY_THRESHOLD) {
    printf("ALERT: PID %d (%s) performed %d file operations (%s) quickly.\n",
           p->pid, p->name, p->files_accessed_in_window, op_name);
    // Reset counter to avoid immediate re-triggering for the same burst.
    // A more sophisticated system might have a cooldown or log more details.
    p->files_accessed_in_window = 0;
  }
}


uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p_user_addr; // Renamed to avoid conflict with struct proc *p
  struct proc *current_proc = myproc();

  argaddr(1, &p_user_addr);
  argint(2, &n);
  if(argfd(0, 0, &f) < 0)
    return -1;
  if(!f->readable){
    log_file_access(current_proc->pid, current_proc->name, "READ", f->path, -1, 0);
    // Failed read due to permissions is a type of failed access.
    // Could increment a general failed access counter if desired,
    // For now, focusing 'consecutive_open_fails' on open primarily.
    return -1 ;
  }
  int result = fileread(f, p_user_addr, n);

  if(result >= 0) {
    log_file_access(current_proc->pid, current_proc->name, "READ", f->path, result, 1);
    track_file_operation(current_proc, "READ");
  }
  else {
    log_file_access(current_proc->pid, current_proc->name, "READ", f->path, result, 0);
  }
  return result;
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p_user_addr; // Renamed
  struct proc *current_proc = myproc();
  
  argaddr(1, &p_user_addr);
  argint(2, &n);
  if(argfd(0, 0, &f) < 0)
    return -1;
  if(!f->writable){
    log_file_access(current_proc->pid, current_proc->name, "WRITE", f->path, -1, 0);
    // Failed write due to permissions.
    return -1 ;
  }

  int result = filewrite(f, p_user_addr, n);
  
  if(result >= 0) {
    log_file_access(current_proc->pid, current_proc->name, "WRITE", f->path, result, 1);
    track_file_operation(current_proc, "WRITE");
  }
  else {
    log_file_access(current_proc->pid, current_proc->name, "WRITE", f->path, result, 0);
  }
  return result;
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;
  struct proc *current_proc = myproc();

  if(argfd(0, &fd, &f) < 0){
  log_file_access(current_proc->pid, current_proc->name, "CLOSE", "", -1, 0);
    return -1;
  }
  
  log_file_access(current_proc->pid, current_proc->name, "CLOSE", f->path, 0, 1);
  track_file_operation(current_proc, "CLOSE");

  current_proc->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  argaddr(1, &st);
  if(argfd(0, 0, &f) < 0)
    return -1;
  return filestat(f, st);
}

uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;
  // struct proc *current_proc = myproc(); // If tracking link operations

  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();
  // track_file_operation(current_proc, "LINK_SUCCESS"); // If desired
  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  // track_file_operation(current_proc, "LINK_FAIL"); // If desired
  return -1;
}

static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;
  struct proc *current_proc = myproc();

  if(argstr(0, path, MAXPATH) < 0){
    log_file_access(current_proc->pid, current_proc->name, "DELETE", path, -1, 0);
    // Could also increment a failed access counter here.
    // current_proc->consecutive_open_fails++; // Or a different counter for generic fails
    // if (current_proc->consecutive_open_fails >= MAX_CONSECUTIVE_OPEN_FAILS_THRESHOLD) {
    //   printf("ALERT: PID %d (%s) has too many failed access attempts (unlink path arg).\n", current_proc->pid, current_proc->name);
    //   current_proc->consecutive_open_fails = 0;
    // }
    return -1;
  }

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    log_file_access(current_proc->pid, current_proc->name, "DELETE", path, -1, 0);
    // current_proc->consecutive_open_fails++; // Example: count this as a type of failure
    // if (current_proc->consecutive_open_fails >= MAX_CONSECUTIVE_OPEN_FAILS_THRESHOLD) {
    //   printf("ALERT: PID %d (%s) has too many failed access attempts (unlink parent not found).\n", current_proc->pid, current_proc->name);
    //   current_proc->consecutive_open_fails = 0;
    // }
    return -1;
  }

  ilock(dp);

  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0){
    iunlockput(dp); // Unlock dp before goto
    end_op();     // End op before return
    log_file_access(current_proc->pid, current_proc->name, "DELETE", path, -1, 0); // Log this specific failure
    return -1; // Was goto bad_unlink, now directly returns after cleanup and log
  }

  if((ip = dirlookup(dp, name, &off)) == 0){
    iunlockput(dp); // Unlock dp before goto
    end_op();     // End op before return
    log_file_access(current_proc->pid, current_proc->name, "DELETE", path, -1, 0); // Log this specific failure
    return -1; // Was goto bad_unlink
  }
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    iunlockput(dp);
    end_op();
    log_file_access(current_proc->pid, current_proc->name, "DELETE", path, -1, 0);
    return -1; 
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  log_file_access(current_proc->pid, current_proc->name, "DELETE", path, 0, 1);
  track_file_operation(current_proc, "UNLINK");
  return 0;

// 'bad_unlink' label is no longer used due to direct returns after cleanup.
// If it were, it would need to handle logging and potentially suspicious activity checks.
// bad_unlink: 
//   if(dp) iunlockput(dp); // Ensure dp is unlocked if acquired
//   end_op();
//   log_file_access(current_proc->pid, current_proc->name, "DELETE", path, -1, 0);
//   return -1;
}


static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0){
    iunlockput(dp);
    return 0;
  }

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      goto fail;
  }

  if(dirlink(dp, name, ip->inum) < 0)
    goto fail;

  if(type == T_DIR){
    dp->nlink++; 
    iupdate(dp);
  }

  iunlockput(dp);

  return ip;

 fail:
  ip->nlink = 0;
  iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n_arg_ret; // To avoid confusion with 'n' in read/write
  struct proc *p = myproc();

  argint(1, &omode);
  if((n_arg_ret = argstr(0, path, MAXPATH)) < 0) {
    p->consecutive_open_fails++;
    if (p->consecutive_open_fails >= MAX_CONSECUTIVE_OPEN_FAILS_THRESHOLD) {
      printf("ALERT: PID %d (%s) has %d failed open attempts (bad path argument).\n", p->pid, p->name, p->consecutive_open_fails);
      p->consecutive_open_fails = 0; // Reset after alert
    }
    return -1;
  }

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      log_file_access(p->pid, p->name, "OPEN", path, -1, 0); 
      p->consecutive_open_fails++;
      if (p->consecutive_open_fails >= MAX_CONSECUTIVE_OPEN_FAILS_THRESHOLD) {
        printf("ALERT: PID %d (%s) has %d failed open/create attempts for '%s'.\n",
               p->pid, p->name, p->consecutive_open_fails, path);
        p->consecutive_open_fails = 0; 
      }
      return -1;
    }
    log_file_access(p->pid, p->name, "CREATE", path, 0, 1);
    p->consecutive_open_fails = 0; // Successful create means open is successful
    track_file_operation(p, "OPEN_CREATE");
  } else { 
    if((ip = namei(path)) == 0){ 
      end_op();
      log_file_access(p->pid, p->name, "OPEN", path, -1, 0);
      p->consecutive_open_fails++;
      if (p->consecutive_open_fails >= MAX_CONSECUTIVE_OPEN_FAILS_THRESHOLD) {
        printf("ALERT: PID %d (%s) has %d failed open attempts for '%s' (file not found).\n",
               p->pid, p->name, p->consecutive_open_fails, path);
        p->consecutive_open_fails = 0; 
      }
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){ 
      iunlockput(ip);
      end_op();
      log_file_access(p->pid, p->name, "OPEN", path, -1, 0);
      p->consecutive_open_fails++;
      if (p->consecutive_open_fails >= MAX_CONSECUTIVE_OPEN_FAILS_THRESHOLD) {
        printf("ALERT: PID %d (%s) has %d failed open attempts for directory '%s' (wrong mode).\n",
               p->pid, p->name, p->consecutive_open_fails, path);
        p->consecutive_open_fails = 0; 
      }
      return -1;
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    if(!(omode & O_CREATE)) iunlockput(ip); // ip already locked if not O_CREATE path
    else iunlockput(ip); // ip is locked from create() path too
    end_op();
    log_file_access(p->pid, p->name, "OPEN", path, -1, 0);
    p->consecutive_open_fails++;
    if (p->consecutive_open_fails >= MAX_CONSECUTIVE_OPEN_FAILS_THRESHOLD) {
      printf("ALERT: PID %d (%s) has %d failed open attempts for device '%s' (invalid major).\n",
             p->pid, p->name, p->consecutive_open_fails, path);
      p->consecutive_open_fails = 0; 
    }
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    if(!(omode & O_CREATE)) iunlockput(ip);
    else iunlockput(ip);
    end_op();
    log_file_access(p->pid, p->name, "OPEN", path, -1, 0);
    p->consecutive_open_fails++;
    if (p->consecutive_open_fails >= MAX_CONSECUTIVE_OPEN_FAILS_THRESHOLD) {
      printf("ALERT: PID %d (%s) has %d failed open attempts for '%s' (resource alloc fail).\n",
             p->pid, p->name, p->consecutive_open_fails, path);
      p->consecutive_open_fails = 0; 
    }
    return -1;
  }

  p->consecutive_open_fails = 0; // Open is now successful
  if (!(omode & O_CREATE)) { // O_CREATE path already called track_file_operation
    track_file_operation(p, "OPEN");
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  safestrcpy(f->path, path, sizeof(f->path));

  if((omode & O_TRUNC) && ip->type == T_FILE){
    itrunc(ip);
  }

  iunlock(ip); // Was locked either by create() or by namei()->ilock()
  end_op();

  if(!(omode & O_CREATE)) {
    log_file_access(p->pid, p->name, "OPEN", path, 0, 1);
  }

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();

  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    // Could add failed mkdir tracking if desired.
    // p->consecutive_generic_fails++; Check threshold, etc.
    return -1;
  }
  iunlockput(ip);
  end_op();
  track_file_operation(p, "MKDIR");
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;
  struct proc *p = myproc();

  begin_op();
  argint(1, &major);
  argint(2, &minor);
  if((argstr(0, path, MAXPATH)) < 0 ||
     (ip = create(path, T_DEVICE, major, minor)) == 0){
    end_op();
    // Could add failed mknod tracking.
    return -1;
  }
  iunlockput(ip);
  end_op();
  track_file_operation(p, "MKNOD");
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
    end_op();
    log_file_access(p->pid, p->name, "CHDIR", path, -1, 0);
    // Could add failed chdir tracking.
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    log_file_access(p->pid, p->name, "CHDIR", path, -1, 0);
    // Could add failed chdir tracking (not a dir).
    return -1;
  }
  iunlock(ip);
  iput(p->cwd);
  end_op();
  log_file_access(p->pid, p->name, "CHDIR", path, 0, 1);
  // chdir is not strictly a file "access" in the same vein for "many files quickly"
  // but could be tracked if desired.
  // track_file_operation(p, "CHDIR");
  p->cwd = ip;
  return 0;
}

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;
  // struct proc *p = myproc(); // Current process context is replaced on success.

  argaddr(1, &uargv);
  if(argstr(0, path, MAXPATH) < 0) {
    // If p was fetched, p->failed_exec_attempts++;
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);
  // If exec fails, ret is -1. The original process context (p) is still running.
  // At this point, you could use 'p' (if fetched earlier) to log a failed exec attempt.
  // e.g., if(ret == -1 && p) { p->failed_exec_attempts++; check_threshold... }

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  // if(p) { p->failed_exec_attempts++; }
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; 
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  argaddr(0, &fdarray);
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  // track_file_operation(p, "PIPE"); // Pipes are FDs but not typical file path accesses.
  return 0;
}
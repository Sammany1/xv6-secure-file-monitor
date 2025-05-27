#include "types.h"
#include "riscv.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern uint ticks;

// Simple thresholds
#define MAX_FAILED_ATTEMPTS 5
#define MAX_RAPID_ACCESS 8
#define TIME_WINDOW 3  // ticks

// Simple tracking structure
struct {
    int failed_count;
    int access_count;
    uint last_access_time;
    uint last_alert_time;
} simple_detector;

// Initialize detector
void
detector_init(void)
{
    simple_detector.failed_count = 0;
    simple_detector.access_count = 0;
    simple_detector.last_access_time = 0;
    simple_detector.last_alert_time = 0;
}

// Simple detection function
void
check_suspicious(int pid, char *proc_name, char *operation, char *filename, int status)
{
    // Skip system processes
    if(strncmp(proc_name, "init", 4) == 0 || strncmp(proc_name, "showlogs", 8) == 0) {
        return;
    }

    // Reset counters if time window passed
    if(ticks - simple_detector.last_access_time > TIME_WINDOW) {
        simple_detector.access_count = 0;
        simple_detector.failed_count = 0;
    }

    simple_detector.last_access_time = ticks;

    if(status == 0) {  // Failed operation
        simple_detector.failed_count++;
        if(simple_detector.failed_count >= MAX_FAILED_ATTEMPTS) {
            if(ticks - simple_detector.last_alert_time > TIME_WINDOW) {
                printf("ALERT: PID %d (%s) has %d failed attempts\n", 
                       pid, proc_name, simple_detector.failed_count);
                simple_detector.last_alert_time = ticks;
            }
        }
    } else {  // Successful operation
        simple_detector.access_count++;
        if(simple_detector.access_count >= MAX_RAPID_ACCESS) {
            if(ticks - simple_detector.last_alert_time > TIME_WINDOW) {
                printf("ALERT: PID %d (%s) accessing files rapidly (%d ops)\n", 
                       pid, proc_name, simple_detector.access_count);
                simple_detector.last_alert_time = ticks;
            }
        }
    }
}
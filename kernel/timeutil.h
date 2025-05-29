#ifndef TIMEUTIL_H
#define TIMEUTIL_H

#include "types.h"

// Global variables
extern struct rtcdate boot_time;

// Function prototypes
void timeutil_init(void);
void format_timestamp(uint ticks, char *buf, int bufsize);

#endif // TIMEUTIL_H
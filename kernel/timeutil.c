#include "types.h"
#include "riscv.h"
#include "param.h"
#include "spinlock.h"
#include "defs.h"
#include "boottime.h"


// System boot time (initialized with default values)
struct rtcdate boot_time;

// Month names for formatting
static char *month_names[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Day names for formatting
static char *day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// Days in each month (non-leap year)
static int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Check if a year is leap year
static int 
is_leap_year(int year) 
{
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Get days in given month of given year
static int 
get_days_in_month(int year, int month) 
{
  if (month == 2 && is_leap_year(year))
    return 29;
  return days_in_month[month];
}

// Calculate day of week (0=Sunday, 6=Saturday)
// Using Zeller's Congruence algorithm
static int 
day_of_week(int year, int month, int day) 
{
  if (month < 3) {
    month += 12;
    year--;
  }
  int h = (day + (13*(month+1))/5 + year + year/4 - year/100 + year/400) % 7;
  return (h + 6) % 7; // Adjust to make Sunday=0
}

// Initialize the timeutil module
void
timeutil_init(void)
{
  // Use time values from boottime.h (generated at build time)
  boot_time.second = BOOT_SECOND;
  boot_time.minute = BOOT_MINUTE;
  boot_time.hour = BOOT_HOUR;
  boot_time.day = BOOT_DAY;
  boot_time.month = BOOT_MONTH;
  boot_time.year = BOOT_YEAR;
}

// Format timestamp from ticks into provided buffer
// Buffer should be at least 25 characters
void 
format_timestamp(uint ticks, char *buf, int bufsize) 
{
  if (bufsize < 25) {
    if (bufsize > 0)
      buf[0] = '\0';
    return;
  }

  // Clear the buffer to ensure no stale data interferes
  memset(buf, 0, bufsize);

  // Convert ticks to time elapsed
  uint seconds_elapsed = ticks / 10;  // ticks are in 0.1 second increments
  
  // Copy boot time values to working variables
  uint seconds = boot_time.second;
  uint minutes = boot_time.minute;
  uint hours = boot_time.hour;
  uint day = boot_time.day;
  uint month = boot_time.month;
  uint year = boot_time.year;
  
  // Add elapsed time
  seconds += seconds_elapsed % 60;
  minutes += (seconds_elapsed / 60) % 60;
  hours += (seconds_elapsed / 3600);
  
  // Adjust for overflow
  if (seconds >= 60) {
    minutes++;
    seconds %= 60;
  }
  
  if (minutes >= 60) {
    hours++;
    minutes %= 60;
  }
  
  uint days_elapsed = hours / 24;
  hours %= 24;
  
  // Add days one by one
  for (uint i = 0; i < days_elapsed; i++) {
    day++;
    if (day > get_days_in_month(year, month)) {
      day = 1;
      month++;
      if (month > 12) {
        month = 1;
        year++;
      }
    }
  }
  
  // Get day of week
  int dow = day_of_week(year, month, day);
  
  // Copy day name (3 chars)
  for (int i = 0; i < 3 && day_names[dow][i]; i++) {
    buf[i] = day_names[dow][i];
  }
  
  // Add space
  buf[3] = ' ';
  
  // Copy month name (3 chars)
  for (int i = 0; i < 3 && month_names[month][i]; i++) {
    buf[4+i] = month_names[month][i];
  }
  
  // Complete the rest manually
  buf[7] = ' ';
  
  // Format day
  if (day < 10) {
    buf[8] = ' ';
    buf[9] = '0' + day;
  } else {
    buf[8] = '0' + (day / 10);
    buf[9] = '0' + (day % 10);
  }
  
  buf[10] = ' ';
  buf[11] = '0' + (hours / 10);
  buf[12] = '0' + (hours % 10);
  buf[13] = ':';
  buf[14] = '0' + (minutes / 10);
  buf[15] = '0' + (minutes % 10);
  buf[16] = ':';
  buf[17] = '0' + (seconds / 10);
  buf[18] = '0' + (seconds % 10);
  buf[19] = ' ';
  buf[20] = '0' + (year / 1000);
  buf[21] = '0' + ((year / 100) % 10);
  buf[22] = '0' + ((year / 10) % 10);
  buf[23] = '0' + (year % 10);
  
  // Ensure null-termination
  buf[24] = '\0';
}
// filepath: /Users/alsammany/Documents/Personal/SPRING 25/OS/OS Project/xv6-labs-2024/gentime.sh
#!/bin/bash

# Get current date/time components
YEAR_STR=$(date +"%Y")
MONTH_STR=$(date +"%m")
DAY_STR=$(date +"%d")
HOUR_STR=$(date +"%H")
MIN_STR=$(date +"%M")
SEC_STR=$(date +"%S")

# Validate and convert to integer, defaulting to 1 if problematic
DEFAULT_MONTH=1
DEFAULT_DAY=1
# Add similar defaults for other components if desired

if [[ -z "$YEAR_STR" || ! "$YEAR_STR" =~ ^[0-9]+$ ]]; then YEAR=2025; else YEAR=$((10#$YEAR_STR)); fi
if [[ -z "$MONTH_STR" || ! "$MONTH_STR" =~ ^[0-9]+$ ]]; then MONTH=$DEFAULT_MONTH; else MONTH=$((10#$MONTH_STR)); fi
if [[ -z "$DAY_STR" || ! "$DAY_STR" =~ ^[0-9]+$ ]]; then DAY=$DEFAULT_DAY; else DAY=$((10#$DAY_STR)); fi
if [[ -z "$HOUR_STR" || ! "$HOUR_STR" =~ ^[0-9]+$ ]]; then HOUR=0; else HOUR=$((10#$HOUR_STR)); fi
if [[ -z "$MIN_STR" || ! "$MIN_STR" =~ ^[0-9]+$ ]]; then MIN=0; else MIN=$((10#$MIN_STR)); fi
if [[ -z "$SEC_STR" || ! "$SEC_STR" =~ ^[0-9]+$ ]]; then SEC=0; else SEC=$((10#$SEC_STR)); fi

# # Ensure month is in valid range (1-12)
# if (( MONTH < 1 || MONTH > 12 )); then
#   echo "gentime.sh: Warning: Calculated month $MONTH is invalid. Defaulting to $DEFAULT_MONTH." >&2
#   MONTH=$DEFAULT_MONTH
# fi
# # Ensure day is in valid range (1-31) - more complex validation possible
# if (( DAY < 1 || DAY > 31 )); then
#   echo "gentime.sh: Warning: Calculated day $DAY is invalid. Defaulting to $DEFAULT_DAY." >&2
#   DAY=$DEFAULT_DAY
# fi

# # Debug output
# echo "gentime.sh: Using values: YEAR=${YEAR}, MONTH=${MONTH}, DAY=${DAY}, HOUR=${HOUR}, MIN=${MIN}, SEC=${SEC}" >&2

# Define the output file path
OUTPUT_FILE="kernel/boottime.h"

# Generate C header file with current time
cat > "$OUTPUT_FILE" << EOF
// Auto-generated file - do not edit
// Created: $(date)

#ifndef BOOTTIME_H
#define BOOTTIME_H

#define BOOT_YEAR   $YEAR
#define BOOT_MONTH  $MONTH
#define BOOT_DAY    $DAY
#define BOOT_HOUR   $HOUR
#define BOOT_MINUTE $MIN
#define BOOT_SECOND $SEC

#endif // BOOTTIME_H
EOF

# Optional: Print a message to indicate the file has been generated/updated
echo "Generated $OUTPUT_FILE with system time."
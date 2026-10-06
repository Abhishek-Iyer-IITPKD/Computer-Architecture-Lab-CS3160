// Logging.
//
// Two destinations, deliberately different:
//
//   sim.log  every message, at every level. This is the file you read when a
//            program does not do what you expected.
//   stdout   INFO and above only, so a normal run stays quiet.
//
// The OUT level is special: log_out() emits exactly one line per instruction
// that retires, in a format the test harness parses. Its shape is frozen --
// see processor.cpp for the format and do not change it.

#ifndef CS3160_LOG_H
#define CS3160_LOG_H

#include "common.h"

enum LogLevel {
    LOG_DEBUG = 10,
    LOG_INFO  = 20,
    LOG_OUT   = 25,  // per-instruction trace; between INFO and ERROR
    LOG_ERROR = 40,
};

// Open `path` for writing and record the level below which messages are
// dropped from the file. Must be called once, before any log_*() call.
void log_open(const char *path, LogLevel file_level);

// Flush and close the log file. Safe to call without log_open().
void log_close(void);

void log_debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log_out(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// True when DEBUG messages are being kept. Guard expensive message building
// with this; formatting a string that is then thrown away is pure cost.
bool log_debug_enabled(void);

// Parse "DEBUG" / "INFO" / "OUT" / "ERROR". Unknown names are fatal.
LogLevel log_level_from_name(const char *name);

#endif  // CS3160_LOG_H

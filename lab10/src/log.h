// Logging.
//
// Two destinations:
//
//   sim.log  every message at or above the configured level; the record of
//            what a program did.
//   stdout   INFO and above only, so that a normal run is quiet.
//
// log_out() emits exactly one OUT line per retired instruction, in a fixed
// format that the tests parse (see trace_retire() in processor.cpp). Do not
// change it.

#ifndef CS3160_LOG_H
#define CS3160_LOG_H

#include "common.h"

enum LogLevel {
    LOG_DEBUG = 10,
    LOG_INFO  = 20,
    LOG_OUT   = 25,  // per-instruction trace; between INFO and ERROR
    LOG_ERROR = 40,
};

// Open `path` for writing; messages below `file_level` are not written to it.
// Call once, before any other log_*() function.
void log_open(const char *path, LogLevel file_level);

// Flush and close the log file. Safe to call without log_open().
void log_close(void);

void log_debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log_out(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Whether DEBUG messages are kept. Guard costly message construction with it,
// since a formatted string that is then discarded is wasted work.
bool log_debug_enabled(void);

// Parse "DEBUG" / "INFO" / "OUT" / "ERROR". Unknown names are fatal.
LogLevel log_level_from_name(const char *name);

#endif  // CS3160_LOG_H

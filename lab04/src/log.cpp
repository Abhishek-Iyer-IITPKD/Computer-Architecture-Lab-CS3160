#include "log.h"

#include <cstdarg>
#include <cstring>

static FILE     *g_file       = NULL;
static LogLevel  g_file_level = LOG_DEBUG;

// stdout gets INFO and above. This is not configurable on purpose: the console
// is for "did it run", the log file is for "what did it do".
static const LogLevel CONSOLE_LEVEL = LOG_INFO;

static const char *level_name(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO";
        case LOG_OUT:   return "OUT";
        case LOG_ERROR: return "ERROR";
    }
    return "?";
}

void log_open(const char *path, LogLevel file_level) {
    g_file_level = file_level;
    g_file = fopen(path, "w");
    if (g_file == NULL) {
        fprintf(stderr, "[ERROR] cannot open log file '%s'\n", path);
        exit(1);
    }
}

void log_close(void) {
    if (g_file != NULL) {
        fclose(g_file);
        g_file = NULL;
    }
}

static void emit(LogLevel level, const char *fmt, va_list ap) {
    // vfprintf consumes the argument list, so each destination needs its own copy.
    if (g_file != NULL && level >= g_file_level) {
        va_list copy;
        va_copy(copy, ap);
        fprintf(g_file, "[%s] ", level_name(level));
        vfprintf(g_file, fmt, copy);
        fputc('\n', g_file);
        va_end(copy);
    }
    if (level >= CONSOLE_LEVEL) {
        va_list copy;
        va_copy(copy, ap);
        FILE *out = (level >= LOG_ERROR) ? stderr : stdout;
        fprintf(out, "[%s] ", level_name(level));
        vfprintf(out, fmt, copy);
        fputc('\n', out);
        va_end(copy);
    }
}

// The OUT level is the exception to the rule above: the per-instruction trace
// is far too long to put on the console, so it only ever goes to the log file.
static void emit_file_only(LogLevel level, const char *fmt, va_list ap) {
    if (g_file != NULL && level >= g_file_level) {
        fprintf(g_file, "[%s] ", level_name(level));
        vfprintf(g_file, fmt, ap);
        fputc('\n', g_file);
    }
}

#define LOG_BODY(level, emitter)      \
    va_list ap;                       \
    va_start(ap, fmt);                \
    emitter((level), fmt, ap);        \
    va_end(ap)

void log_debug(const char *fmt, ...) { LOG_BODY(LOG_DEBUG, emit_file_only); }
void log_out(const char *fmt, ...)   { LOG_BODY(LOG_OUT,   emit_file_only); }
void log_info(const char *fmt, ...)  { LOG_BODY(LOG_INFO,  emit); }
void log_error(const char *fmt, ...) { LOG_BODY(LOG_ERROR, emit); }

bool log_debug_enabled(void) {
    return g_file != NULL && g_file_level <= LOG_DEBUG;
}

LogLevel log_level_from_name(const char *name) {
    if (strcmp(name, "DEBUG") == 0) return LOG_DEBUG;
    if (strcmp(name, "INFO")  == 0) return LOG_INFO;
    if (strcmp(name, "OUT")   == 0) return LOG_OUT;
    if (strcmp(name, "ERROR") == 0) return LOG_ERROR;
    fatal("unknown log level '%s' (expected DEBUG, INFO, OUT or ERROR)", name);
}

// fatal() lives here because it has to reach the log file: a message that only
// appears on the console is lost the moment the harness captures stdout.
void fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    emit(LOG_ERROR, fmt, ap);
    va_end(ap);
    log_close();
    exit(1);
}

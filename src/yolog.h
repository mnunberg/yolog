/**
 *  Initial version August 2010
 *  Second revision June 2012
 *  Copyright 2010-2012 M. Nunberg
 *  See the included LICENSE file for distribution terms
 *
 *
 *  Yolog is a simple logging library.
 */

#ifndef YOLOG_H_
#define YOLOG_H_

#include <stdio.h>
#include <stdarg.h>

typedef enum {
#define YOLOG_XLVL(X) \
    X(DEFAULT, 0x0) \
    X(DEBUG, 0x1) \
    X(INFO, 0x2) \
    X(WARN, 0x4) \
    X(ERROR, 0x8) \
    X(CRIT, 0x10)

#define X(lvl, i) \
    YOLOG_##lvl = i,
    YOLOG_XLVL(X)
#undef X
    YOLOG_LEVEL_MAX
} yolog_level_t;



typedef enum {
    #define YOLOG_XFLAGS(X) \
    X(COLOR, 0x8)
    #define X(c,v) YOLOG_F_##c = v,
    YOLOG_XFLAGS(X)
    #undef X
    YOLOG_F_MAX = 0x200
} yolog_flags_t;

#define YOLOG_FLAGS_DEFAULT 0

typedef struct yolog_context {
    /**
     * The minimum allowable logging level
     */
    yolog_level_t level;

    /**
     * I'm not sure what uses this
     */
    yolog_flags_t flags;

    /**
     * This is a human-readable name for the context. This is the 'prefix'.
     */
    const char *prefix;

    /**
     * This is the logging format string.
     * Format strings can have specifiers as such: %(spec). This was taken
     * from Python's logging module which is probably one of the easiest logging
     * libraries to use aside from Yolog, of course!
     *
     * The following specifiers are available:
     *
     * %(epoch) - time(NULL) result
     *
     * %(pid) - The process ID
     *
     * %(tid) - The thread ID. On Linux this is gettid(), on other POSIX systems
     *  this does a byte-for-byte representation of the returned pthread_t from
     *  pthread_self(). On non-POSIX systems this does nothing.
     *
     * %(level) - A level string, e.g. 'DEBUG', 'ERROR'
     *
     * %(filename) - The source file
     *
     * %(line) - The line number at which the logging function was invoked
     *
     * %(func) - The function from which the function was invoked
     *
     * %(color) - This is a special specifier and indicates that normal
     *  severity color coding should begin here.
     */
    const char *logfmt;


    /**
     * The destination FILE* object to log to. Defaults to stderr.
     */
    FILE *dest;

} yolog_context;

/**
 * Default format string
 */
#define YOLOG_FORMAT_DEFAULT \
    "[%(prefix)] %(filename):%(line) %(color)(%(func)) "


/**
 * These two functions log an actual message.
 *
 * @param ctx - The context. Can be NULL to use the default/global context.
 * @param level - The severity of this message
 * @param file - The filename of this message (e.g. __FILE__)
 * @param line - The line of this message (e.g. __LINE__)
 * @param fn - The function name (e.g. __func__)
 * @param fmt - User-defined format (printf-style)
 * @param args .. extra arguments to format
 */
void
yolog_logger(yolog_context *ctx,
             yolog_level_t level,
             const char *file,
             int line,
             const char *fn,
             const char *fmt,
             ...);

/**
 * Same thing as yolog_logger except it takes a va_list instead.
 */
void
yolog_vlogger(yolog_context *ctx,
              yolog_level_t level,
              const char *file,
              int line,
              const char *fn,
              const char *fmt,
              va_list ap);

/**
 * Initialize the default logging settings. This function *must* be called
 * some time before any logging messages are invoked, or disaster may ensue.
 * (Or not).
 */
void
yolog_init_defaults(yolog_context *contexts,
                    int ncontext,
                    yolog_level_t default_level,
                    yolog_flags_t flags,
                    const char *color_env,
                    const char *level_env);

/**
 * Yolog maintains a global object for messages which have no context.
 * This function gets this object.
 */
yolog_context *
yolog_get_global(void);

/**
 * This is a hack for C89 compilers which don't support variadic macros.
 * In this case we maintain a global context which is initialized and locked.
 *
 * This function tries to lock the context (it checks to see if this level can
 * be logged, locks the global structure, and returns true. If the level
 * cannot be logged, false is retured).
 *
 * The functions implicit_logger() and implicit_end() should only be called
 * if implicit_begin() returns true.
 */
int
yolog_implicit_begin(yolog_context *ctx,
                     int level,
                     const char *file,
                     int line,
                     const char *fn);


/**
 * printf-compatible wrapper which operates on the implicit structure
 * set in implicit_begin()
 */
void
yolog_implicit_logger(const char *fmt, ...);

/**
 * Unlocks the implicit structure
 */
void
yolog_implicit_end(void);



/**
 * These are the convenience macros. They are disabled by default because I've
 * made some effort to make yolog more embed-friendly and not clobber a project
 * with generic functions.
 *
 * The default macros are C99 and employ __VA_ARGS__/variadic macros.
 *
 *
 *
 */
#ifdef YOLOG_ENABLE_MACROS

#ifndef YOLOG_C89_MACROS

/**
 * These macros are all invoked with double-parens, so
 * yolog_debug(("foo"));
 * This way the 'variation' of the arguments is dispatched to the actual C
 * function instead of the macro..
 */

#define yolog_debug(...) \
yolog_logger(\
    NULL\
    YOLOG_DEBUG, \
    __FILE__, \
    __LINE__, \
    __func__, \
    ## __VA_ARGS__)

#define yolog_info(...) \
yolog_logger(\
    NULL\
    YOLOG_INFO, \
    __FILE__, \
    __LINE__, \
    __func__, \
    ## __VA_ARGS__)

#define yolog_warn(...) \
yolog_logger(\
    NULL\
    YOLOG_WARN, \
    __FILE__, \
    __LINE__, \
    __func__, \
    ## __VA_ARGS__)

#define yolog_error(...) \
yolog_logger(\
    NULL\
    YOLOG_ERROR, \
    __FILE__, \
    __LINE__, \
    __func__, \
    ## __VA_ARGS__)

#define yolog_crit(...) \
yolog_logger(\
    NULL\
    YOLOG_CRIT, \
    __FILE__, \
    __LINE__, \
    __func__, \
    ## __VA_ARGS__)

#else /* ifdef YOLOG_C89_MACROS */


#define yolog_debug(args) \
if (yolog_implicit_begin( \
    NULL, \
    YOLOG_DEBUG, \
    __FILE__, \
    __LINE__, \
    __func__)) \
{ \
    yolog_implicit_logger args; \
    yolog_implicit_end(); \
}

#define yolog_info(args) \
if (yolog_implicit_begin( \
    NULL, \
    YOLOG_INFO, \
    __FILE__, \
    __LINE__, \
    __func__)) \
{ \
    yolog_implicit_logger args; \
    yolog_implicit_end(); \
}

#define yolog_warn(args) \
if (yolog_implicit_begin( \
    NULL, \
    YOLOG_WARN, \
    __FILE__, \
    __LINE__, \
    __func__)) \
{ \
    yolog_implicit_logger args; \
    yolog_implicit_end(); \
}

#define yolog_error(args) \
if (yolog_implicit_begin( \
    NULL, \
    YOLOG_ERROR, \
    __FILE__, \
    __LINE__, \
    __func__)) \
{ \
    yolog_implicit_logger args; \
    yolog_implicit_end(); \
}

#define yolog_crit(args) \
if (yolog_implicit_begin( \
    NULL, \
    YOLOG_CRIT, \
    __FILE__, \
    __LINE__, \
    __func__)) \
{ \
    yolog_implicit_logger args; \
    yolog_implicit_end(); \
}

#endif /* YOLOG_C89_MACROS */

#endif /* YOLOG_ENABLE_MACROS */

#endif /* YOLOG_H_ */

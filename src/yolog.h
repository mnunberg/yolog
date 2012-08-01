/**
 *  Initial version August 2010
 *  Second revision June 2012
 *  Copyright 2010-2012 M. Nunberg
 *  See the included LICENSE file for distribution terms
 *
 *
 *  Yolog is a simple logging library.
 *
 *  It has several goals
 *
 *  1) Make initial usage and learning curve as *easy* as possible. It should be
 *      simple enough to generate loging output for most cases
 *
 *  2) While simplicity is good, flexibility is good too. From my own
 *      experience, programs will tend to accumulate a great deal of logging
 *      info. In common situations they are just commented out or removed in
 *      production deployments. This is probably not the right way to go.
 *      Logging statements at times can function as comments, and should be
 *      enabled when required.
 *
 *      Thus output control and performance should be flexible, but not at the
 *      cost of reduced simplicity.
 *
 *  3) Reduction of boilerplate. Logging should be more about what's being
 *      logged, and less about which particular output control or context is
 *      being used. Through the use of preprocessor macros, this information
 *      should be implicit using a simple identifier which is the logging
 *      entry point.
 *
 *
 *  As such, the architecture is designed as follows:
 *
 *  Logging Context
 *      This is the main component. A logging context represents a class or
 *      subsystem within the application which emits messages. This logging
 *      context is created or initialized by the application developer
 *      appropriate to his or her application.
 *
 *      For example, an HTTP client may contain the following systems
 *          htparse: Subsystem which handles HTTP parsing
 *          sockpool: Subsystem maintaining connection pools
 *          srvconn:  Subsystem which intializes new connections to the server
 *          io: Reading/writing and I/O readiness notifications
 *
 *  Logging Levels:
 *      Each subsystem has various degrees of information it may wish to emit.
 *      Some information is relevant only while debugging an application, while
 *      other information may be relevant to its general maintenance.
 *
 *      For example, an HTTP parser system may wish to output the tokens it
 *      encounters during its processing of the TCP stream, but would also
 *      like to notify about bad messages, or such which may exceed the maximum
 *      acceptable header size (which may hint at an attack or security risk).
 *
 *      Logging messages of various degrees concern various aspects of the
 *      application's development and maintenance, and therefore need individual
 *      output control.
 *
 *  Configuration:
 *      Because of the varying degrees of flexibility required in a logging
 *      library, there are multiple configuration stages.
 *
 *      1) Project configuration.
 *          As the project author, you are aware of the subsystems which your
 *          application provides, and should be able to specify this as a
 *          compile/build time parameter. This includes the types of subsystems
 *          as well as the identifier/macro names which your application will
 *          use to emit logging messages. The burden should not be placed
 *          on the application developer to actually perform the boilerplate
 *          of writing logging wrappers, as this can be generally abstracted
 *
 *      2) Runtime/Initialization configuration.
 *          This is performed by users who are interested in different aspects
 *          of your application's logging systems. Thus, a method of providing
 *          configuration files and environment variables should be used in
 *          order to allow users to modify logging output from outside the code.
 *
 *          Additionally, you as the application developer may be required to
 *          trigger this bootstrap process (usually a few function calls from
 *          your program's main() function).
 *
 *      3) There is no logging configuration or setup!
 *          Logging messages themselves should be entirely about the message
 *          being emitted, with the metadata/context information being implicit
 *          and only the information itself being explicit.
 */

#ifndef YOLOG_H_
#define YOLOG_H_

#include <stdio.h>
#include <stdarg.h>

typedef enum {
#define YOLOG_XLVL(X) \
    X(DEFAULT,  0) \
    /* really transient messages */ \
    X(RANT,     1) \
    /* function enter/leave events */ \
    X(TRACE,    2) \
    /* state change events */ \
    X(STATE,    3) \
    /* generic application debugging events */ \
    X(DEBUG,    4) \
    /* informational messages */ \
    X(INFO,     5) \
    /* warning messages */ \
    X(WARN,     6) \
    /* error messages */ \
    X(ERROR,    7) \
    /* critical messages */ \
    X(CRIT,     8)

#define X(lvl, i) \
    YOLOG_##lvl = i,
    YOLOG_XLVL(X)
#undef X
    YOLOG_LEVEL_MAX
} yolog_level_t;

#define YOLOG_LEVEL_INVALID -1
#define YOLOG_LEVEL_UNSET 0

enum {
    /* screen output */
    YOLOG_OUTPUT_SCREEN = 0,

    /* global file */
    YOLOG_OUTPUT_GFILE,

    /* specific file */
    YOLOG_OUTPUT_PFILE,

    YOLOG_OUTPUT_COUNT
};

#define YOLOG_OUTPUT_ALL YOLOG_OUTPUT_COUNT

#ifndef YOLOG_API
#define YOLOG_API
#endif

struct yolog_context;
struct yolog_fmt_st;

/**
 * Callback to be invoked when a logging message arrives.
 * This is passed the logging context, the level, and message passed.
 */
typedef void
        (*yolog_callback)(
                struct yolog_context*,
                yolog_level_t,
                va_list ap);


enum {
    YOLOG_LINFO_DEFAULT_ONLY = 0,
    YOLOG_LINFO_DEFAULT_ALSO = 1
};

typedef enum {

    #define YOLOG_XFLAGS(X) \
    X(NOGLOG, 0x1) \
    X(NOFLOG, 0x2) \
    X(COLOR, 0x10)
    #define X(c,v) YOLOG_F_##c = v,
    YOLOG_XFLAGS(X)
    #undef X
    YOLOG_F_MAX = 0x200
} yolog_flags_t;

/* maximum size of heading/trailing user data between format specifiers */
#define YOLOG_FMT_USTR_MAX 16

/* Default format string */
#define YOLOG_FORMAT_DEFAULT \
    "[%(prefix)] %(filename):%(line) %(color)(%(func)) "


enum {
    YOLOG_FMT_LISTEND = 0,
    YOLOG_FMT_USTRING,
    YOLOG_FMT_EPOCH,
    YOLOG_FMT_PID,
    YOLOG_FMT_TID,
    YOLOG_FMT_LVL,
    YOLOG_FMT_TITLE,
    YOLOG_FMT_FILENAME,
    YOLOG_FMT_LINE,
    YOLOG_FMT_FUNC,
    YOLOG_FMT_COLOR
};


/* structure representing a single compiled format specifier */
struct yolog_fmt_st {
    /* format type, opaque, derived from format string */
    int type;
    /* user string, heading or trailing padding, depending on the type */
    char ustr[YOLOG_FMT_USTR_MAX];
};

struct yolog_msginfo_st {
    const char *co_line;
    const char *co_title;
    const char *co_reset;

    const char *m_func;
    const char *m_file;
    const char *m_prefix;

    int m_level;
    int m_line;
    unsigned long m_time;
};

struct yolog_output_st {
    FILE *fp;
    struct yolog_fmt_st *fmtv;
    int use_color;
    int level;
};

struct yolog_context;

typedef struct yolog_context_group {
    struct yolog_context *contexts;
    int ncontexts;
    yolog_callback cb;
    struct yolog_output_st o_file;
    struct yolog_output_st o_screen;
} yolog_context_group;

typedef struct yolog_context {
    /**
     * The minimum allowable logging level.
     * Performance number so we don't have to iterate over the entire
     * olevels array each time. This should be kept in sync with sync_levels
     * after any modification to olevels
     */
    yolog_level_t level;

    struct yolog_context_group *parent;

    /**
     * Array of per-output-type levels
     */
    yolog_level_t olevels[YOLOG_OUTPUT_COUNT];

    /**
     * This is a human-readable name for the context. This is the 'prefix'.
     */
    const char *prefix;

    /**
     * If this subsystem logs to its own file, then it is set here
     */
    struct yolog_output_st *o_alt;
} yolog_context;


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
YOLOG_API
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
YOLOG_API
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
YOLOG_API
void
yolog_init_defaults(yolog_context_group *grp,
                    yolog_level_t default_level,
                    const char *color_env,
                    const char *level_env);


/**
 * Compile a format string into a format structure.
 *
 *
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
 *
 *
 * @param fmt the format string
 * @return a list of allocated format structures, or NULL on error. The
 * format structure list may be freed by free()
 */
YOLOG_API
struct yolog_fmt_st *
yolog_fmt_compile(const char *fmt);


/**
 * Sets the format string of a context.
 *
 * Internally this calls fmt_compile and then sets the format's context,
 * freeing any existing context.
 *
 * @param ctx the context which should utilize the format
 * @param fmt a format string.
 * @param replace whether to replace the existing string (if any)
 *
 * @return 0 on success, -1 if there was an error setting the format.
 *
 *
 */
YOLOG_API
int
yolog_set_fmtstr(struct yolog_output_st *output,
                 const char *fmt,
                 int replace);


YOLOG_API
void
yolog_set_screen_format(yolog_context_group *grp,
                        const char *format);

/**
 * Yolog maintains a global object for messages which have no context.
 * This function gets this object.
 */
YOLOG_API
yolog_context *
yolog_get_global(void);

/**
 * This will read a file and apply debug settings from there..
 *
 * @param contexts an aray of logging contexts
 * @param ncontext the count of contexts
 * @param filename the filename containing the settings
 * @param cb a callback to invoke when an unrecognized line is found
 * @param error a pointer to a string which shall contain an error
 *
 * @return true on success, false on failure. error will be set on failure as
 * well. Should be freed with 'free()'
 */
YOLOG_API
int
yolog_parse_file(yolog_context_group *grp,
                 const char *filename);


YOLOG_API
void
yolog_parse_envstr(yolog_context_group *grp,
                const char *envstr);

/**
 * These functions are mainly private
 */

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


void
yolog_fmt_write(struct yolog_fmt_st *fmts,
                FILE *fp,
                const struct yolog_msginfo_st *minfo);


void
yolog_sync_levels(yolog_context *ctx);

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

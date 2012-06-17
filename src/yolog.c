#if (defined(__unix__) && (!defined(_POSIX_SOURCE)))
#define _POSIX_SOURCE
#endif /* __unix__ */

#ifdef __linux__
#define _GNU_SOURCE
#endif /* __linux__ */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <assert.h>

struct yolog_context;

struct yolog_colors_st {
    const char *co_line;
    const char *co_title;
    const char *co_reset;
};

struct yolog_implicit_st {
    int level;

    /** Meta/Context information **/
    int m_line;
    const char *m_func;
    const char *m_file;

    struct yolog_context *ctx;
};

#ifdef __unix__
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#ifdef __linux__
#include <syscall.h>
/**
 * Linux can get the thread ID,
 * but glibc says it doesn't provide a wrapper for gettid()
 **/
#define yolog_fprintf_thread(f) fprintf(f, "%lu", syscall(SYS_gettid))

#else /* other POSIX non-linux systems */
static void
yolog_fprintf_thread(FILE *f) {
    pthread_t pt = pthread_self();
    unsigned char *ptc = (unsigned char*)(void*)(&pt);
    size_t ii;
    fprintf(f, "0x");

    for (ii = 0; ii < sizeof(pt); ii++) {
        fprintf(f, "%02x", (unsigned)(ptc[ii]));
    }
}
#endif /* __linux__ */


#define yolog_dest_lock(ctx) flockfile(ctx->dest)
#define yolog_dest_unlock(ctx) funlockfile(ctx->dest)
#define yolog_get_pid getpid

/**
 * Stuff for C89-mode logging. We can't use variadic macros
 * so we rely on a shared global context. sucks, it does.
 */

static pthread_mutex_t Yolog_Global_Mutex;

#define yolog_global_init() pthread_mutex_init(&Yolog_Global_Mutex, NULL)
#define yolog_global_lock() pthread_mutex_lock(&Yolog_Global_Mutex)
#define yolog_global_unlock() pthread_mutex_unlock(&Yolog_Global_Mutex)

#else
#define yolog_fprintf_thread(f)
#define yolog_dest_lock(ctx)
#define yolog_dest_unlock(ctx)
#define yolog_global_init()
#define yolog_global_lock()
#define yolog_global_unlock()
#define yolog_get_pid() -1
#endif /* __unix __ */

#include "yolog.h"

static struct yolog_implicit_st Yolog_Implicit;
static
yolog_context Yolog_Global_Context = {
        YOLOG_DEFAULT,
        YOLOG_FLAGS_DEFAULT,
        "",
        NULL
};


/*just some macros*/

#define _FG "3"
#define _BG "4"
#define _BRIGHT_FG "1"
#define _INTENSE_FG "9"
#define _DIM_FG "2"
#define _YELLOW "3"
#define _WHITE "7"
#define _MAGENTA "5"
#define _CYAN "6"
#define _BLUE "4"
#define _GREEN "2"
#define _RED "1"
#define _BLACK "0"
/*Logging subsystem*/




static
const char *
yolog_strlevel(yolog_level_t level)
{
    if (!level) {
        return "";
    }

#define X(n, i) \
    if (level == i) { return #n; }
    YOLOG_XLVL(X)
#undef X
    return "";
}

void
yolog_init_defaults(yolog_context *contexts,
                    int ncontext,
                    yolog_level_t default_level,
                    yolog_flags_t flags,
                    const char *color_env,
                    const char *level_env)

{
    const char *envtmp;
    int itmp = 0, ii;


    if (color_env && (envtmp = getenv(color_env))) {
        sscanf(envtmp, "%d", &itmp);
        if (itmp) {
            flags |= YOLOG_F_COLOR;
        } else {
            flags &= ~(YOLOG_F_COLOR);
        }
    }

    itmp = 0;
    if (level_env && (envtmp = getenv(level_env))) {
        sscanf(envtmp, "%d", &itmp);
        if (itmp != YOLOG_DEFAULT) {
            default_level = itmp;
        }
    }

    for (ii = 0; ii < ncontext; ii++) {
        yolog_context *ctx = contexts + ii;
        ctx->logfmt = YOLOG_FORMAT_DEFAULT;

        if (ctx->level == YOLOG_DEFAULT) {
            ctx->level = default_level;
            if (ctx->level == default_level) {
                ctx->level = YOLOG_INFO;
            }
        }

        ctx->flags |= flags;
        if (ctx->dest == NULL) {
            ctx->dest = stderr;
        }

        if (ctx->prefix == NULL) {
            ctx->prefix = "";
        }
    }

    if (contexts != &Yolog_Global_Context &&
            Yolog_Global_Context.dest == NULL) {

        yolog_init_defaults(&Yolog_Global_Context, 1,
                            default_level,
                            flags,
                            color_env,
                            level_env);
    }

    yolog_global_init();
}

static void
yolog_get_formats(yolog_context *ctx,
                  int level,
                  struct yolog_colors_st *colors)
{
    if (ctx->flags & YOLOG_F_COLOR) {

        colors->co_title = "\033[" _INTENSE_FG _MAGENTA "m";
        colors->co_reset = "\033[0m";

        switch(level) {
        case YOLOG_CRIT:
        case YOLOG_ERROR:
            colors->co_line = "\033[" _BRIGHT_FG ";" _FG _RED "m";
            break;
        case YOLOG_WARN:
            colors->co_line = "\033[" _FG _YELLOW "m";
            break;
        case YOLOG_DEBUG:
            colors->co_line = "\033[" _DIM_FG ":" _WHITE "m";
            break;
        default:
            colors->co_line = "";
            break;
        }
    } else {
        colors->co_line = "";
        colors->co_title = "";
        colors->co_reset = "";
    }
}

#define CAN_LOG(lvl, ctx) \
    (level >= ctx->level)

void
yolog_vlogger(yolog_context *ctx,
              yolog_level_t level,
              const char *file,
              int line,
              const char *fn,
              const char *fmt,
              va_list ap)
{
    struct yolog_colors_st colors;
    const char *prefix;
    const char *fmtp;

    if (!ctx) {
        ctx = &Yolog_Global_Context;
    }

    if (CAN_LOG(level, ctx) == 0 || ctx->dest == NULL) {
        return;
    }

    prefix = ctx->prefix;
    if (prefix == NULL || *prefix == '\0') {
        prefix = "-";
    }

    yolog_dest_lock(ctx);

    yolog_get_formats(ctx, level, &colors);

    fmtp = ctx->logfmt;
    while (*fmtp) {
        char optbuf[128] = { 0 };
        int optpos = 0;

        if ( !(*fmtp == '%' && fmtp[1] == '(')) {
            fwrite(fmtp, 1, 1, ctx->dest);
            fmtp++;
            continue;
        }

        fmtp+= 2;

        while (*fmtp && *fmtp != ')') {
            optbuf[optpos] = *fmtp;
            optpos++;
            fmtp++;
        }

#define _cmpopt(s) (strncmp(optbuf, s, \
                        optpos > (sizeof(s)-1) \
                        ? sizeof(s) - 1 : optpos ) \
                        == 0)

        if (_cmpopt("ep")) {
            /* epoch */
            fprintf(ctx->dest, "%lu", time(NULL));


        } else if (_cmpopt("pi")) {
            /* pid */
            fprintf(ctx->dest, "%d", yolog_get_pid());

        } else if (_cmpopt("ti")) {
            /* tid */
            yolog_fprintf_thread(ctx->dest);

        } else if (_cmpopt("le")) {
            /* level */
            fprintf(ctx->dest, yolog_strlevel(level));

        } else if (_cmpopt("pr")) {
            /* prefix */
            fprintf(ctx->dest,
                    "%s%s%s",
                    colors.co_title, prefix, colors.co_reset);

        } else if (_cmpopt("fi")) {
            /* filename */
            fprintf(ctx->dest, "%s", file);

        } else if (_cmpopt("li")) {
            /* line */
            fprintf(ctx->dest, "%d", line);

        } else if (_cmpopt("fu")) {
            /* function */
            fprintf(ctx->dest, "%s", fn);

        } else if (_cmpopt("co")) {
            /* color */
            fprintf(ctx->dest, "%s", colors.co_line);

        } else {
            /** Invalid format string */
        }
        fmtp++;
    }
#undef _cmpopt

    vfprintf(ctx->dest, fmt, ap);
    fprintf(ctx->dest, "%s\n", colors.co_reset);
    fflush(ctx->dest);
    yolog_dest_unlock(ctx);
}

void
yolog_logger(yolog_context *ctx,
             yolog_level_t level,
             const char *file,
             int line,
             const char *fn,
             const char *fmt,
             ...)
{
    va_list ap;
    va_start(ap, fmt);
    yolog_vlogger(ctx, level, file, line, fn, fmt, ap);
    va_end(ap);
}

int
yolog_implicit_begin(yolog_context *ctx,
                     int level,
                     const char *file,
                     int line,
                     const char *fn)
{
    if (!ctx) {
        ctx = &Yolog_Global_Context;
    }

    if (CAN_LOG(level, ctx) == 0 || ctx->dest == NULL) {
        return 0;
    }

    yolog_global_lock();

    Yolog_Implicit.ctx = ctx;
    Yolog_Implicit.level = level;
    Yolog_Implicit.m_file = file;
    Yolog_Implicit.m_line = line;
    Yolog_Implicit.m_func = fn;
    return 1;
}

void
yolog_implicit_end(void)
{
    yolog_global_unlock();
}

void
yolog_implicit_logger(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    yolog_vlogger(Yolog_Implicit.ctx,
                  Yolog_Implicit.level,
                  Yolog_Implicit.m_file,
                  Yolog_Implicit.m_line,
                  Yolog_Implicit.m_func,
                  fmt,
                  ap);

    va_end(ap);
}

yolog_context *
yolog_get_global(void) {
    return &Yolog_Global_Context;
}

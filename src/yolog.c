
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

/* needed for flockfile/funlockfile */
#if (defined(__unix__) && (!defined(_POSIX_SOURCE)))
#define _POSIX_SOURCE
#endif /* __unix__ */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

struct yolog_context;

struct yolog_implicit_st {
    int level;

    /** Meta/Context information **/
    int m_line;
    const char *m_func;
    const char *m_file;

    struct yolog_context *ctx;
};

/**
 * Stuff for C89-mode logging. We can't use variadic macros
 * so we rely on a shared global context. sucks, it does.
 */

#ifdef __unix__
#include <pthread.h>
static pthread_mutex_t Yolog_Global_Mutex;

#define yolog_global_init() pthread_mutex_init(&Yolog_Global_Mutex, NULL)
#define yolog_global_lock() pthread_mutex_lock(&Yolog_Global_Mutex)
#define yolog_global_unlock() pthread_mutex_unlock(&Yolog_Global_Mutex)
#define yolog_dest_lock(ctx) flockfile(ctx->fp)
#define yolog_dest_unlock(ctx) funlockfile(ctx->fp)

#else
#define yolog_dest_lock(ctx)
#define yolog_dest_unlock(ctx)
#define yolog_global_init()
#define yolog_global_lock()
#define yolog_global_unlock()
#endif /* __unix __ */

#include "yolog.h"

static struct yolog_implicit_st Yolog_Implicit;
static
yolog_context Yolog_Global_Context = {
        YOLOG_LEVEL_UNSET, /* level */
        NULL, /* parent */
        { 0 }, /* level settings */
        "", /* prefix */
};

static
yolog_context_group Yolog_Global_CtxGroup = {
        &Yolog_Global_Context,
        1
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

YOLOG_API
void
yolog_init_defaults(struct yolog_context_group *grp,
                    yolog_level_t default_level,
                    const char *color_env,
                    const char *level_env)
{
    const char *envtmp;
    int itmp = 0, ii;
    int use_color = 0;

    if (!grp) {
        grp = &Yolog_Global_CtxGroup;
    }

    if (color_env && (envtmp = getenv(color_env))) {
        sscanf(envtmp, "%d", &itmp);
        if (itmp) {
            use_color = 1;
        }
    }

    itmp = 0;
    if (level_env && (envtmp = getenv(level_env))) {
        sscanf(envtmp, "%d", &itmp);
        if (itmp != YOLOG_DEFAULT) {
            default_level = itmp;
        }
    }

    if (!grp->o_screen.fmtv) {
        grp->o_screen.fmtv = yolog_fmt_compile(YOLOG_FORMAT_DEFAULT);
    }

    if (use_color) {
        grp->o_screen.use_color = 1;
    }

    if (!grp->o_screen.fp) {
        grp->o_screen.fp = stderr;
    }

    if (default_level == YOLOG_LEVEL_UNSET) {
        default_level = YOLOG_INFO;
    }

    grp->o_screen.level = default_level;

    for (ii = 0; ii < grp->ncontexts; ii++) {
        yolog_context *ctx = grp->contexts + ii;
        ctx->parent = grp;

        if (ctx->prefix == NULL) {
            ctx->prefix = "";
        }

        yolog_sync_levels(ctx);
    }

    if (grp != &Yolog_Global_CtxGroup) {
        yolog_init_defaults(&Yolog_Global_CtxGroup,
                            default_level,
                            color_env,
                            level_env);
    } else {
        yolog_global_init();
    }
}

static int
output_can_log(yolog_context *ctx,
            int level,
            int oix,
            struct yolog_output_st *output)
{
    if (output == NULL) {
        return 0;
    }

    if (output->fp == NULL) {
        return 0;
    }

    if (ctx->olevels[oix] != YOLOG_LEVEL_UNSET) {
        if (ctx->olevels[oix] <= level) {
            return 1;
        } else {
            return 0;
        }
    }

    if (output->level != YOLOG_LEVEL_UNSET) {
        if (output->level <= level) {
            return 1;
        }
    }
    return 0;
}



static int
ctx_can_log(yolog_context *ctx,
            int level,
            struct yolog_output_st *outputs[YOLOG_OUTPUT_COUNT+1])
{
    int ii;
    outputs[YOLOG_OUTPUT_GFILE] = &ctx->parent->o_file;
    outputs[YOLOG_OUTPUT_SCREEN] = &ctx->parent->o_screen;
    outputs[YOLOG_OUTPUT_PFILE] = ctx->o_alt;

    if (ctx->level != YOLOG_LEVEL_UNSET && ctx->level > ctx->level) {
        return 0;
    }

    for (ii = 0; ii < YOLOG_OUTPUT_COUNT; ii++) {
        if (output_can_log(ctx, level, ii, outputs[ii])) {
            return 1;
        }
    }
    return 0;
}


static void
yolog_get_formats(struct yolog_output_st *output,
                  int level,
                  struct yolog_msginfo_st *colors)
{
    if (output->use_color) {

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
        case YOLOG_TRACE:
        case YOLOG_STATE:
        case YOLOG_RANT:
            colors->co_line = "\033[" _DIM_FG ";" _FG _CYAN "m";
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
    struct yolog_msginfo_st msginfo;
    const char *prefix;
    int ii;
    struct yolog_output_st *outputs[YOLOG_OUTPUT_COUNT];

    if (!ctx) {
        ctx = &Yolog_Global_Context;
    }

    if (!ctx_can_log(ctx, level, outputs)) {
        return;
    }

    prefix = ctx->prefix;
    if (prefix == NULL || *prefix == '\0') {
        prefix = "-";
    }

    if (ctx->parent->cb) {
        ctx->parent->cb(ctx, level, ap);
    }

    msginfo.m_file = file;
    msginfo.m_level = level;
    msginfo.m_line = line;
    msginfo.m_prefix = prefix;
    msginfo.m_func = fn;

    for (ii = 0; ii < YOLOG_OUTPUT_COUNT; ii++) {
        struct yolog_output_st *out = outputs[ii];
        FILE *fp;
        struct yolog_fmt_st *fmtv;

        if (!output_can_log(ctx, level, ii,  out)) {
            continue;
        }

        fmtv = out->fmtv;
        fp = out->fp;

        yolog_get_formats(out, level, &msginfo);

        yolog_dest_lock(out);

#ifndef va_copy
#define YOLOG_VACOPY_OVERRIDE
#ifdef __GNUC__
#define va_copy __va_copy
#else
#define va_copy(dst, src) (dst) = (src)
#endif /* __GNUC__ */
#endif

        yolog_fmt_write(fmtv, fp, &msginfo);
        {
            va_list vacp;
            va_copy(vacp, ap);
            vfprintf(fp, fmt, vacp);
            va_end(vacp);
        }
        fprintf(fp, "%s\n", msginfo.co_reset);
        fflush(fp);
        yolog_dest_unlock(out);
    }
#ifdef YOLOG_VACOPY_OVERRIDE
#undef va_copy
#undef YOLOG_VACOPY_OVERRIDE
#endif
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
    struct yolog_output_st *outputs[YOLOG_OUTPUT_COUNT];
    if (!ctx) {
        ctx = &Yolog_Global_Context;
    }

    if (!ctx_can_log(ctx, level, outputs)) {
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


YOLOG_API
void
yolog_set_screen_format(yolog_context_group *grp,
                        const char *format)
{
    if (!grp) {
        grp = &Yolog_Global_CtxGroup;
    }

    yolog_set_fmtstr(&grp->o_screen, format, 1);
}

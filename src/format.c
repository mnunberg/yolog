#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef __unix__
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#ifdef __linux__
#include <sys/syscall.h>
/**
 * Linux can get the thread ID,
 * but glibc says it doesn't provide a wrapper for gettid()
 **/

#ifndef _GNU_SOURCE
/* if _GNU_SOURCE was defined on the commandline, then we should already have
 * a prototype
 */

int syscall(int, ...);
#endif
#define yolog_fprintf_thread(f) fprintf(f, "%d", syscall(SYS_gettid))

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


#define yolog_get_pid getpid

#else
#define yolog_fprintf_thread(f)
#define yolog_get_pid() -1

#endif /* __unix__ */


#include "yolog.h"

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

YOLOG_API
struct yolog_fmt_st *
yolog_fmt_compile(const char *fmtstr)
{
    const char *fmtp = fmtstr;
    struct yolog_fmt_st *fmtroot, *fmtcur;
    size_t n_alloc = sizeof(*fmtroot) * 16;
    size_t n_used = 0;
    size_t nstr = 0;

    fmtroot = malloc(n_alloc);
    fmtcur = fmtroot;

    memset(fmtcur, 0, sizeof(*fmtcur));

    /* first format is a leading user string */
    fmtcur->type = YOLOG_FMT_USTRING;
    while (fmtp && *fmtp && *fmtp != '%' && fmtp[1] == '(') {
        fmtcur->ustr[nstr] = *fmtp;
        nstr++;
        fmtp++;
        if (nstr > sizeof(fmtcur->ustr)-1) {
            goto GT_ERROR;
        }
    }

    fmtcur->ustr[nstr] = '\0';

    while (*fmtp) {
        char optbuf[128] = { 0 };
        size_t optpos = 0;

        if ( !(*fmtp == '%' && fmtp[1] == '(')) {
            fmtcur->ustr[nstr] = *fmtp;
            nstr++;
            if (nstr > sizeof(fmtcur->ustr)-1) {
                goto GT_ERROR;
            }

            fmtcur->ustr[nstr] = '\0';
            fmtp++;
            continue;
        }

        fmtp+= 2;

        while (*fmtp && *fmtp != ')') {
            optbuf[optpos] = *fmtp;
            optpos++;
            fmtp++;
        }

        /**
         * We've pushed trailing data into the current format structure. Time
         * to create a new one
         */

        n_used++;
        nstr = 0;

        if ( (n_used+1) > (n_alloc / sizeof(*fmtcur))) {
            n_alloc *= 2;
            fmtroot = realloc(fmtroot, n_alloc);
        }

        fmtcur = fmtroot + n_used;
        memset(fmtcur, 0, sizeof(*fmtcur));


    #define _cmpopt(s) (strncmp(optbuf, s, \
                        optpos > (sizeof(s)-1) \
                        ? sizeof(s) - 1 : optpos ) \
                        == 0)

        if (_cmpopt("ep")) {
            /* epoch */
            fmtcur->type = YOLOG_FMT_EPOCH;

        } else if (_cmpopt("pi")) {
            /* pid */
            fmtcur->type = YOLOG_FMT_PID;

        } else if (_cmpopt("ti")) {
            /* tid */
            fmtcur->type = YOLOG_FMT_TID;

        } else if (_cmpopt("le")) {
            /* level */
            fmtcur->type = YOLOG_FMT_LVL;

        } else if (_cmpopt("pr")) {
            /* prefix */
            fmtcur->type = YOLOG_FMT_TITLE;

        } else if (_cmpopt("fi")) {
            /* filename */
            fmtcur->type = YOLOG_FMT_FILENAME;

        } else if (_cmpopt("li")) {
            /* line */
            fmtcur->type = YOLOG_FMT_LINE;

        } else if (_cmpopt("fu")) {
            /* function */
            fmtcur->type = YOLOG_FMT_FUNC;

        } else if (_cmpopt("co")) {
            /* color */
            fmtcur->type = YOLOG_FMT_COLOR;
        } else {
            goto GT_ERROR;
        }

        fmtp++;
    }
    #undef _cmpopt

    fmtroot[n_used+1].type = YOLOG_FMT_LISTEND;
    return fmtroot;

    GT_ERROR:
    free (fmtroot);
    return NULL;
}


void
yolog_fmt_write(struct yolog_fmt_st *fmts,
                FILE *fp,
                const struct yolog_msginfo_st *minfo)
{
    struct yolog_fmt_st *fmtcur = fmts;

    while (fmtcur->type != YOLOG_FMT_LISTEND) {
        switch (fmtcur->type) {
        case YOLOG_FMT_USTRING:
            fprintf(fp, "%s", fmtcur->ustr);
            goto GT_NOUSTR;
            break;

        case YOLOG_FMT_EPOCH:
            fprintf(fp, "%d", (int)time(NULL));
            break;

        case YOLOG_FMT_PID:
            fprintf(fp, "%d", yolog_get_pid());
            break;

        case YOLOG_FMT_TID:
            yolog_fprintf_thread(fp);
            break;

        case YOLOG_FMT_LVL:
            fprintf(fp, "%s", yolog_strlevel(minfo->m_level));
            break;

        case YOLOG_FMT_TITLE:
            fprintf(fp, "%s%s%s",
                    minfo->co_title, minfo->m_prefix, minfo->co_reset);
            break;

        case YOLOG_FMT_FILENAME:
            fprintf(fp, "%s", minfo->m_file);
            break;

        case YOLOG_FMT_LINE:
            fprintf(fp, "%d", minfo->m_line);
            break;

        case YOLOG_FMT_FUNC:
            fprintf(fp, "%s", minfo->m_func);
            break;

        case YOLOG_FMT_COLOR:
            fprintf(fp, "%s", minfo->co_line);
            break;

        default:
            break;
        }

        if (fmtcur->ustr[0]) {
            fprintf(fp, "%s", fmtcur->ustr);
        }

        GT_NOUSTR:
        fmtcur++;
        continue;
    }
}

int
yolog_set_fmtstr(struct yolog_output_st *output,
                 const char *fmt,
                 int replace)
{
    struct yolog_fmt_st *newfmt;

    if (output->fmtv && replace == 0) {
        return -1;
    }

    newfmt = yolog_fmt_compile(fmt);
    if (!newfmt) {
        return -1;
    }

    if (output->fmtv) {
        free (output->fmtv);
    }
    output->fmtv = newfmt;
    return 0;
}

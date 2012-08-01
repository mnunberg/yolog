#include "yolog.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>

#ifdef __unix__
#include <strings.h>
#else
#define strcasecmp _stricmp
#endif

#ifndef ATTR_UNUSED
#ifdef __GNUC__
#define ATTR_UNUSED __attribute__((unused))
#else
#define ATTR_UNUSED
#endif
#endif

#ifndef GENYL_APESQ_INLINED

#ifdef YOLOG_APESQ_STATIC

#define APESQ_API static ATTR_UNUSED
#include "apesq/apesq.c"

#else
#include <apesq.h>

#endif /* YOLOG_APESQ_STATIC */

#else
#endif /* GENYL_APESQ_INLINED */

static yolog_context *
yolog_context_by_name(yolog_context *contexts,
                      int ncontexts,
                      const char *name)
{
    int ii = 0;
    for (ii = 0; ii < ncontexts; ii++) {
        if (strcasecmp(contexts[ii].prefix, name) == 0 ) {
            return contexts + ii;
        }
    }
    return NULL;
}

static int
yolog_level_by_name(const char *lvlstr)
{
#define X(b,v) if (strcasecmp(lvlstr, #b) == 0) { return v; }
    YOLOG_XLVL(X);
#undef X

    return -1;

}


void
yolog_sync_levels(yolog_context *ctx)
{
    int ii;
    int minlevel = 0xff;
    for (ii = 0; ii < YOLOG_OUTPUT_COUNT; ii++) {
        if (ctx->olevels[ii] < minlevel) {
            minlevel = ctx->olevels[ii];
        }
    }
    if (minlevel != 0xff) {
        ctx->level = minlevel;
    }
}

/**
 * Try and configure logging preferences from the environment, this only
 * affects subsystems and doesn't modify things like format strings or color
 * preferences (which are specified in other environment variables).
 *
 * The format of the environment string should be like:
 *
 * MYPROJ_DEBUG="subsys1:error,subsys2:warn"
 */
YOLOG_API
void
yolog_parse_envstr(yolog_context_group *grp,
                const char *envstr)
{
    char *cp = malloc(strlen(envstr)+2);
    char *p = cp;
    int slen;

    strcpy(cp, envstr);
    slen = strlen(cp);

    if (cp[slen-1] != ',') {
        cp[slen] = ',';
        cp[slen+1] = '\0';
    }

    if (!grp) {
        grp = yolog_get_global()->parent;
    }

    do {
        int minlvl;
        struct yolog_context *ctx;

        char *subsys = p;
        char *level = strchr(subsys, ':');

        if (!level) {
            fprintf(stderr,
                    "Yolog: Couldn't find ':' in environment string (%s)\n",
                    subsys);
            break;
        }

        *level = '\0';
        level++;

        p = strchr(level, ',');
        if (p) {
            *p = '\0';
            p++;
        }

        ctx = yolog_context_by_name(grp->contexts, grp->ncontexts, subsys);

        if (!ctx) {
            fprintf(stderr, "Yolog: Unrecognized subsystem '%s'\n",
                    subsys);
            goto GT_NEXT;
        }

        minlvl = yolog_level_by_name(level);
        if (minlvl == -1) {
            fprintf(stderr, "Yolog: Bad level specified '%s'\n", level);
            goto GT_NEXT;
        }

        ctx->olevels[YOLOG_OUTPUT_SCREEN] = minlvl;
        yolog_sync_levels(ctx);
        printf("Setting %s (%d) for %s\n", level, minlvl, subsys);

        GT_NEXT:
        ;
    } while (p && *p);

    free (cp);
}

static int
get_minlevel(struct apesq_entry_st *secent)
{
    struct apesq_value_st *value = apesq_get_values(
            APESQ_SECTION(secent), "MinLevel");

    int level;
    if (!value) {
        return -1;
    }
    level = yolog_level_by_name(value->strdata);
    if (level == -1) {
        fprintf(stderr, "Yolog: Unrecognized level '%s'\n",
                value->strdata);
    }
    return level;
}

struct format_info_st {
    struct yolog_fmt_st *fmt;
    int used;
};

enum {
    FMI_SCREEN,
    FMI_FILE,
    FMI_DEFAULT,
    FMI_COUNT
};

static FILE *
open_new_file(const char *path, const char *mode)
{
    FILE *fp = fopen(path, mode);
    if (!fp) {
        return fp;
    }
    fprintf(fp, "--- Mark at %lu ---\n", (unsigned long)time(NULL));
    return fp;
}

static void
handle_subsys_output(
        yolog_context *ctx,
        struct apesq_entry_st *ent,
        char *logroot,
        struct yolog_fmt_st *fmtdef,
        int *fmtdef_used)
{
    int minlevel = -1;

    struct apesq_entry_st **current;
    struct apesq_value_st *apval;
    struct apesq_entry_st **oents = apesq_get_sections(ent, "Output");

    if (!oents) {
        return;
    }

    for (current = oents; *current; current++) {
        struct apesq_section_st *osec = APESQ_SECTION(*current);
        char **onames = osec->secnames;

        if (!onames) {
            fprintf(stderr, "Yolog: Output without target\n");
            continue;
        }

        for (; *onames; onames++) {
            int olix;

            if (strcmp(*onames, "$screen$") == 0) {
                olix = YOLOG_OUTPUT_SCREEN;

            } else if (strcmp(*onames, "$globalfile$") == 0) {
                olix = YOLOG_OUTPUT_GFILE;

            } else {
                FILE *fp;
                char *fname;
                olix = YOLOG_OUTPUT_PFILE;

                if ((*onames)[0] == '/' || *logroot == '\0') {
                    fname = *onames;
                    fp = open_new_file(*onames, "a");

                } else {
                    char fpath[16384] = { 0 };
                    assert (*logroot);

                    fname = fpath;
                    sprintf(fname, "%s/%s", logroot, *onames);
                    fp = open_new_file(fname, "a");
                }

                if (!fp) {
                    fprintf(stderr,
                            "Yolog: Couldn't open output '%s': %s\n",
                            fname,
                            strerror(errno));
                    continue;
                }

                if (ctx->o_alt != NULL) {
                    fprintf(stderr, "Yolog: Multiple output files for "
                            "subsystem '%s'\n",
                            ctx->prefix);

                    fclose(fp);
                    continue;
                }

                assert (ctx->o_alt == NULL);
                ctx->o_alt = calloc(1, sizeof(*ctx->o_alt));
                ctx->o_alt->fp = fp;

                if ( (apval = apesq_get_values(osec, "Format"))) {
                    ctx->o_alt->fmtv = yolog_fmt_compile(apval->strdata);

                } else {
                    ctx->o_alt->fmtv = fmtdef;
                    *fmtdef_used = 1;
                }
            }

            /**
             * Get the minimum level for the format..
             */
            if ( (minlevel = get_minlevel(*current)) != -1) {
                ctx->olevels[olix] = minlevel;
            }

            if (olix == YOLOG_OUTPUT_PFILE) {

                if ((apval = apesq_get_values(osec, "Format"))) {
                    ctx->o_alt->fmtv = yolog_fmt_compile(apval->strdata);
                }

                apesq_read_value(osec, "Color", APESQ_T_BOOL, 0,
                                 &ctx->o_alt->use_color);
            }
        }
    }
    free (oents);
}

YOLOG_API
int
yolog_parse_file(yolog_context_group *grp,
                 const char *filename)
{
    struct apesq_entry_st *root = apesq_parse_file(filename);
    struct apesq_value_st *apval = NULL;
    struct apesq_entry_st **secents = NULL, **cursecent = NULL;
    struct yolog_fmt_st *fmtdfl = NULL;
    int fmtdfl_used = 0;
    struct apesq_section_st *secroot;

    char logroot[8192] = { 0 };
    int minlevel = -1;
    int gout_count = 0;

    if (!root) {
        return 0;
    }

    if (!grp) {
        grp = yolog_get_global()->parent;
    }

    secroot = APESQ_SECTION(root);

    if ( (apval = apesq_get_values(secroot, "LogRoot"))) {
        strcat(logroot, apval->strdata);
    }

    if ((apval = apesq_get_values(secroot, "Format"))) {
        fmtdfl = yolog_fmt_compile(apval->strdata);
    }

    secents = apesq_get_sections(root, "Output");

    if (!secents) {
        goto GT_NO_OUTPUTS;
    }

    for (cursecent = secents; *cursecent && gout_count < 2; cursecent++) {
        struct apesq_section_st *sec = APESQ_SECTION(*cursecent);
        struct yolog_output_st *out;

        FILE *fp;

        if (!sec->secnames) {
            fprintf(stderr, "Yolog: Output section without any target");
            continue;
        }

        if (strcmp(sec->secnames[0], "$screen$") == 0) {
            fp = stderr;
            out = &grp->o_screen;

        } else {
            char destpath[16384] = { 0 };
            if (sec->secnames[0][0] == '/' || *logroot == '\0') {
                strcpy(destpath, sec->secnames[0]);
            } else {
                sprintf(destpath, "%s/%s", logroot, sec->secnames[0]);
            }
            fp = open_new_file(destpath, "a");
            if (!fp) {
                fprintf(stderr, "Yolog: Couldn't open '%s': %s\n",
                        destpath, strerror(errno));
                continue;
            }
            out = &grp->o_file;
        }

        out->fp = fp;

        if ( (apval = apesq_get_values(sec, "Format"))) {

            if (out->fmtv) {
                free(out->fmtv);
            }

            out->fmtv = yolog_fmt_compile(apval->strdata);
        } else if (fmtdfl) {
            out->fmtv = fmtdfl;
            fmtdfl_used = 1;
        } else {
            if (out->fmtv) {
                free (out->fmtv);
            }
            out->fmtv = yolog_fmt_compile(YOLOG_FORMAT_DEFAULT);
        }

        minlevel = get_minlevel(*cursecent);
        if (minlevel != -1) {
            out->level = minlevel;
        }

        apesq_read_value(sec, "Color", APESQ_T_BOOL, 0, &out->use_color);
        gout_count++;
    }


    free (secents);
    secents = NULL;

    GT_NO_OUTPUTS:

    secents = apesq_get_sections(root, "Subsys");
    if (!secents) {
        goto GT_NO_SUBSYS;
    }

    for (cursecent = secents; *cursecent; cursecent++) {
        struct apesq_section_st *sec = APESQ_SECTION(*cursecent);
        char **secnames = sec->secnames;

        if (!secnames) {
            fprintf(stderr, "Yolog: Subsys section without any specifier\n");
            continue;
        }

        for (; *secnames; secnames++) {
            int tmplevel;

            struct yolog_context *ctx =
                    yolog_context_by_name(grp->contexts,
                                          grp->ncontexts,
                                          *secnames);
            if (!ctx) {

                if (grp != yolog_get_global()->parent) {
                    /* don't care about defaults */
                    fprintf(stderr, "Yolog: No such context '%s'\n",
                            *secnames);
                }
                continue;
            }

            ctx->parent = grp;
            handle_subsys_output(ctx, *cursecent, logroot,
                                 fmtdfl,
                                 &fmtdfl_used);

            tmplevel = get_minlevel(*cursecent);
            if (tmplevel != -1) {
                int ii;
                for (ii = 0; ii < YOLOG_OUTPUT_COUNT; ii++) {
                    if (ctx->olevels[ii] == YOLOG_LEVEL_UNSET) {
                        ctx->olevels[ii] = tmplevel;
                    }
                }
            }

            ctx->level = YOLOG_LEVEL_UNSET;
            yolog_sync_levels(ctx);
        }
    }
    free (secents);

    GT_NO_SUBSYS:
    ;

    if (!fmtdfl_used) {
        free(fmtdfl);
    }
    apesq_free(root);
    return 0;
}


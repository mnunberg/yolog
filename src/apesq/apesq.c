#ifndef APESQ_NO_INCLUDE
#include "apesq.h"
#endif /* APESQ_NO_INCLUDE */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

/* LONG_MAX for strtol */
#include <limits.h>

/* HUGE_VAL for strtod */
#include <math.h>

struct token_st;

struct token_st {
    struct token_st *next;
    char delim_end;
    char *str;
    int len;
};

static int
shift_string(char *str, int nshift)
{
    memmove(str, str + nshift, strlen(str)-(nshift-1));
    return strlen(str);
}

static char *
apesq__strdup(char *src)
{
    char *dest = calloc(1, strlen(src) + 1);
    strcpy(dest, src);
    return dest;
}

static int
apesq__strcasecmp(char *a, char *b)
{
    while (*(a++) && *(b++)) {
        if (tolower(*a) != tolower(*b)) {
            return 1;
        }
    }

    return !(*a == *b);
}

static struct token_st *
get_tokens(char *line)
{
    struct token_st *root;
    struct token_st *cur;

    int in_quote = 0;
    int in_delim = 0;

    char *lcopy = apesq__strdup(line);
    char *p = lcopy;
    char *last_tok_begin = p;

    root = malloc(sizeof(*root));
    root->str = apesq__strdup(p);
    root->next = NULL;
    cur = root;

    for (; *p; p++) {
        if (in_delim) {
            if (isspace(*p) == 0 && *p != '=' && *p != ',') {
                /* end of delimiter */
                last_tok_begin = p;
                in_delim = 0;
                cur->next = calloc(1, sizeof(*cur->next));
                cur = cur->next;
            } else {
                continue;
            }
        }

        if (*p == '"' || *p == '\'') {
            if (!in_quote) {
                /* string, ignore everything until the next '"' */
                in_quote = *p;
                shift_string(p, 1);
            } else if (in_quote == *p) {
                /* string end */
                in_quote = 0;
                shift_string(p, 1);
            }
        }

        if (in_quote) {
            /* ignore string data */
            continue;
        }

        /* not a string and not in middle of a delimiter. end current token */
        if (isspace(*p) || *p == ',' || *p == '=') {

            size_t toklen = p - last_tok_begin;
            assert (in_delim == 0);

            if (cur->str) {
                free(cur->str);
            }

            cur->str = malloc(toklen + 1);
            cur->delim_end = *p;
            cur->len = toklen;

            memcpy(cur->str, last_tok_begin, toklen);
            cur->str[toklen] = '\0';
            in_delim = 1;
        }
    }

    if (cur == root) {
        cur->len = strlen(cur->str);
    } else {
        cur->len = strlen(last_tok_begin);

        if (cur->str) {
            free(cur->str);
        }

        cur->str = malloc(cur->len+1);
        strcpy(cur->str, last_tok_begin);
    }

    free(lcopy);
    return root;
}

static void
free_tokens(struct token_st *root)
{
    while (root) {
        struct token_st *next;

        if (root->str) {
            free (root->str);
        }

        next = root->next;
        free(root);

        root = next;
    }
}

static struct apesq_entry_st *
append_new_entry(struct apesq_entry_st *parent)
{
    struct apesq_section_st *section;
    struct apesq_entry_st *ret = calloc(1, sizeof(*ret));

    assert (parent->value.next == NULL);
    assert (parent->value.type == APESQ_T_SECTION);
    section =  parent->value.u.section;

    if (!section->e_tail) {
        assert (section->e_head == NULL);
        section->e_head = ret;
        section->e_tail= ret;

    } else {
        section->e_tail->next = ret;
        section->e_tail = ret;
    }

    ret->parent = parent;
    return ret;
}

static int
get_section_names(
        struct apesq_section_st *section,
        struct token_st *toknames)
{
    int snames_count = 0;
    char **secnames;
    char **secnamep;
    struct token_st *tmp = toknames;

    while (tmp) {
        snames_count++;
        tmp = tmp->next;
    }

    secnames = calloc(snames_count + 1, sizeof(char*));
    secnamep = secnames;

    while (toknames) {
        if (!toknames->next) {
            if (toknames->str[toknames->len-1] != '>') {
                return -1;

            } else {
                toknames->str[toknames->len-1] = '\0';
            }
        }

        *secnamep = toknames->str;
        secnamep++;

        toknames->str = NULL;
        toknames = toknames->next;
    }

    section->secnames = secnames;

    return 0;
}

static struct apesq_entry_st *
create_new_section(struct apesq_entry_st *parent,
                   struct token_st *tokens)
{
    struct apesq_entry_st *new_ent = append_new_entry(parent);
    struct apesq_section_st *new_section = calloc(1, sizeof(*new_section));
    struct token_st *cur = tokens;
    char *sectype;

    new_ent->value.type = APESQ_T_SECTION;
    new_ent->value.u.section = new_section;

    sectype = calloc(1, cur->len + 1);
    new_section->sectype = sectype;

    strcpy(sectype, cur->str);
    if (sectype[cur->len-1] == '>') {
        sectype[cur->len-1] = '\0';
        cur->len--;

    } else {
        get_section_names(new_section, cur->next);
    }

    new_section->sectype = sectype;
    new_ent->key = apesq__strdup(sectype);
    return new_ent;
}

static int
parse_line(char *line,
           struct apesq_entry_st **sec_ent,
           char **errorp)
{
    struct token_st *tokens = get_tokens(line);
    struct token_st *cur = tokens, *last;
    struct apesq_section_st *sec = (*sec_ent)->value.u.section;
    struct apesq_entry_st *new_ent;
    int ret = 1;

    last = tokens;
    while (last->next) {
        last = last->next;
    }

#define LINE_ERROR(s) \
    *errorp = s; \
    ret = 0; \
    goto GT_RET;

    if (cur->str[0] == '<') {
        char begin_chars[2];
        begin_chars[0] = cur->str[0];
        begin_chars[1] = cur->str[1];

        if (begin_chars[1] == '/') {
            cur->len = shift_string(cur->str, 2);
        } else {
            cur->len = shift_string(cur->str, 1);
        }

        if (last->str[last->len-1] != '>') {
            LINE_ERROR("Section statement does not end with '>'");
        }

        if (cur->len < 3) {
            LINE_ERROR("Section name too short");
        }

        if (begin_chars[1] == '/') {
            if (cur->str[cur->len-1] != '>') {
                LINE_ERROR("Garbage at closing tag");
            }

            cur->str[cur->len-1] = '\0';
            cur->len--;

            if (!sec->sectype || strcmp(sec->sectype, cur->str) != 0) {
                LINE_ERROR("Closing tag name does not match opening");
            }

            *sec_ent = (*sec_ent)->parent;
            goto GT_RET;
        } else {
            *sec_ent = create_new_section(*sec_ent, tokens);
            goto GT_RET;
        }
    }

    /**
     * Otherwise, key-value pairs
     */

    new_ent = append_new_entry(*sec_ent);
    if (cur->str[0] == '-' || cur->str[0] == '+') {
        char bval = cur->str[0];
        struct apesq_value_st *value = &new_ent->value;
        memmove(cur->str, cur->str + 1, cur->len);

        value->type = APESQ_T_BOOL;
        value->u.num = (bval == '-') ? 0 : 1;
        new_ent->key = cur->str;
        cur->str = NULL;

        if (cur->next) {
            LINE_ERROR("Boolean options cannot be lists");
        }

    } else {
        /* no-boolean, key with possible multi-value pairs */

        struct apesq_value_st *value = &new_ent->value;
        new_ent->key = cur->str;
        cur->str = NULL;
        cur = cur->next;

        if (!cur) {
            LINE_ERROR("Lone token is invalid");
        }

        value->type = APESQ_T_STRING;
        value->strdata = cur->str;
        cur->str = NULL;

        cur = cur->next;
        while (cur) {
            value->next = calloc(1, sizeof(*value));
            value = value->next;

            value->type = APESQ_T_STRING;
            value->strdata = cur->str;
            cur->str = NULL;

            cur = cur->next;
        }
    }

    GT_RET:
    free_tokens(tokens);
    return ret;

#undef LINE_ERROR
}

APESQ_API
struct apesq_entry_st *
apesq_parse_string(char *str, int nstr)
{
    char *p, *end, *line;
    struct apesq_entry_st *root, *cur;

    if (nstr == -1) {
        nstr = strlen(str) + 1;
    }

    end =  str + nstr;
    root = calloc(1, sizeof(*root));
    root->parent = NULL;

    root->value.type = APESQ_T_SECTION;
    root->value.u.section = calloc(1, sizeof(struct apesq_section_st));

    root->value.u.section->sectype = apesq__strdup("!ROOT!");
    root->key = apesq__strdup("!ROOT!");

    p = str;
    cur = root;


    while (*p) {
        char *errp = NULL;
        char *line_end;

        line = p;

        for (; p < end && *p != '\n'; p++);
        *p = '\0';
        line_end = p-1;

        /* strip leading whitespace */
        for (; line < p && isspace(*line); line++);

        /* strip trailing whitespace */
        while (line_end > line && isspace(*line_end)) {
            *line_end = '\0';
            line_end--;
        }

        /* pointer to the beginning of the next line */
        p++;


        if (*line == '#' || line == p-1) {
            continue;
        }

        if (!parse_line(line, &cur, &errp)) {
            if (errp) {
                printf("Got error while parsing '%s': %s\n", line, errp);
            } else {
                printf("Parsing done!\n");
            }

            break;
        }
    }
    return root;
}

APESQ_API
struct apesq_entry_st *
apesq_parse_file(const char *path)
{
    FILE *fp;
    int fsz;
    char *buf;
    struct apesq_entry_st *ret;

    if (!path) {
        return NULL;
    }
    fp = fopen(path, "r");
    if (!fp) {
        return NULL;
    }

    fseek(fp, 0L, SEEK_END);
    fsz = ftell(fp);
    if (fsz < 1) {
        fclose(fp);
        return NULL;
    }

    fseek(fp, 0L, SEEK_SET);
    buf = malloc(fsz+1);
    buf[fsz] = '\0';

    if (fread(buf, 1, fsz, fp) == -1) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    ret = apesq_parse_string(buf, fsz);
    free(buf);
    return ret;
}

APESQ_API
void
apesq_dump_section(struct apesq_entry_st *root, int indent)
{
    struct apesq_section_st *section = root->value.u.section;
    struct apesq_entry_st *ent;
    int ii;
    char s_indent[64] = { 0 };


    assert (root->value.next == NULL);
    assert (root->value.type == APESQ_T_SECTION);


    for (ii = 0; ii < indent * 3; ii++) {
        s_indent[ii] = ' ';
    }

    printf("%sSection Type(%s), ", s_indent, section->sectype);

    if (section->secnames) {
        char **namep = section->secnames;
        while (*namep) {
            printf("Name: %s, ", *namep);
            namep++;
        }
    }
    printf("\n");

    ent = section->e_head;
    while (ent) {
        struct apesq_value_st *value = &ent->value;

        printf("%s Key(%s): ", s_indent, ent->key);

        if (ent->value.type == APESQ_T_SECTION) {
            printf("\n");
            apesq_dump_section(ent, indent + 1);
            goto GT_CONT;
        }

        while (value) {
            assert (value->next != value);

            if (value->type == APESQ_T_BOOL) {
                printf("Boolean(%d) ", value->u.num);
            } else {
                printf("String(\"%s\") ", value->strdata);
            }
            value = value->next;
        }

        printf("\n");

        GT_CONT:
        ent = ent->next;
    }
}

APESQ_API
void
apesq_free(struct apesq_entry_st *root)
{
    struct apesq_section_st *section = root->value.u.section;
    struct apesq_entry_st *ent = section->e_head;

    while (ent) {

        struct apesq_entry_st *e_next = ent->next;
        struct apesq_value_st *value = &ent->value;

        while (value) {
            struct apesq_value_st *v_next = value->next;

            if (value->type == APESQ_T_SECTION) {
                apesq_free(ent);
                goto GT_NEXT_ENT;

            } else {

                if (value->strdata) {
                    free (value->strdata);
                    value->strdata = NULL;
                }

                if (value != &ent->value) {
                    free(value);
                }
            }

            value = v_next;
        }
        if (ent->key) {
            free (ent->key);
        }
        free (ent);

        GT_NEXT_ENT:
        ent = e_next;
    }

    if (section->sectype) {
        free (section->sectype);
    }

    if (section->secnames) {
        char **namep = section->secnames;
        while (*namep) {
            free (*namep);
            namep++;
        }
        free (section->secnames);
    }
    free (section);

    if (root->key) {
        free (root->key);
    }

    free (root);
}

/**
 * Retrieve a specific section, or class of sections
 * returns a null-terminated array of pointers to entries with sections:
 *
 * e.g.
 * apesq_find_section(root, "/Subsys")
 */

APESQ_API
struct apesq_entry_st **
apesq_get_sections(struct apesq_entry_st *root, const char *name)
{
    struct apesq_entry_st **ret;
    struct apesq_section_st *section = APESQ_SECTION(root);
    struct apesq_entry_st *ent;
    struct apesq_entry_st **p_ret;

    int ret_count = 0;
    assert (root->value.type == APESQ_T_SECTION);

    ent = section->e_head;

    for (ent = section->e_head; ent; ent = ent->next) {
        if (ent->value.type != APESQ_T_SECTION ||
                strcmp(APESQ_SECTION(ent)->sectype, name) != 0) {
            continue;
        }
        ret_count++;
    }

    if (!ret_count) {
        return NULL;
    }

    ret = calloc(ret_count + 1, sizeof(*ret));
    p_ret = ret;

    for (ent = section->e_head; ent; ent = ent->next) {
        if (ent->value.type != APESQ_T_SECTION ||
                strcmp(APESQ_SECTION(ent)->sectype, name) != 0) {
            continue;
        }
        *(p_ret++) = ent;
    }
    return ret;
}

APESQ_API
struct apesq_value_st *
apesq_get_values(struct apesq_section_st *section, const char *key)
{
    struct apesq_entry_st *ent = section->e_head;

    while (ent) {
        if (ent->key && strcmp(key, ent->key) == 0) {
            return &ent->value;
        }
        ent = ent->next;
    }

    return NULL;
}

APESQ_API
int
apesq_read_value(struct apesq_section_st *section,
                 const void *param,
                 enum apesq_type type,
                 int flags,
                 void *out)
{
    const struct apesq_value_st *value;
    if (flags & APESQ_F_VALUE) {
        value = (const struct apesq_value_st*)param;
    } else {
        value = apesq_get_values(section, (const char*)param);
    }

    if (!value) {
        return APESQ_VALUE_ENOENT;
    }

    if (value->next && (flags & APESQ_F_MULTIOK) == 0) {
        return APESQ_VALUE_EISPLURAL;
    }

    if (type == APESQ_T_BOOL) {
        if (value->type == APESQ_T_BOOL) {
            *(int*)out = value->u.num;
            return APESQ_VALUE_OK;
        } else {
            if (apesq__strcasecmp("on", value->strdata) == 0) {
                *(int*)out = 1;
                return APESQ_VALUE_OK;
            } else if (apesq__strcasecmp("off", value->strdata) == 0) {
                *(int*)out = 0;
                return APESQ_VALUE_OK;
            } else {
                /* try numeric conversion */
                return apesq_read_value(section, param, APESQ_T_INT,
                                        flags, out);
            }
        }

    } else if (type == APESQ_T_INT) {
        char *endptr;
        long int ret = strtol(value->strdata, &endptr, 10);

        /*stupid editor */
        if (ret == LONG_MAX || *endptr != '\0') {
            return APESQ_VALUE_ECONVERSION;
        }

        *(int*)out = ret;
        return APESQ_VALUE_OK;
    } else if (type == APESQ_T_DOUBLE) {
        char *endptr;
        double ret = strtod(value->strdata, &endptr);
        if (ret == HUGE_VAL || *endptr != '\0') {
            return APESQ_VALUE_ECONVERSION;
        }
        *(double*)out = ret;
        return APESQ_VALUE_OK;
    }
    return APESQ_VALUE_EINVAL;
}

#ifndef APESQ_H_
#define APESQ_H_


/**
 * Apachesque is a configuration parsing library which can handle apache-style
 * configs. It can probably also handle more.
 *
 * Configuration is parsed into a tree which is comprised of three components
 *
 * 1) Entry.
 *      This is the major component, it has a 'key' which is the 'name' under
 *      which it may be found. Entries contain values
 *
 * 3) Value.
 *      This contains application-level data which is provided as a string
 *      value to a 'Key'. Values may be boolean or string. Functions can
 *      try to coerce values on-demand.
 *
 * 2) Section.
 *      This is a type of value which itself may contain multiple entries,
 *      in a configuration file this looks like
 *
 *      <SectionType SectionName1, SectionName2>
 *          ....
 *      </SectionType>
 *
 *      Particular about APQE is that sections can have multiple names, so
 *      things may be aliased/indexed/whatever.
 */

/**
 * Enumeration of types, used largely internally
 */
enum apesq_type {
    APESQ_T_STRING,
    APESQ_T_INT,
    APESQ_T_DOUBLE,
    APESQ_T_BOOL,
    APESQ_T_SECTION,
    APESQ_T_LIST
};

/**
 * Return values for read_value()
 */
enum {
    APESQ_VALUE_OK = 0,
    APESQ_VALUE_EINVAL,
    APESQ_VALUE_ENOENT,
    APESQ_VALUE_ECONVERSION,
    APESQ_VALUE_EISPLURAL
};

/**
 * Flags for read_value()
 */
enum {
    APESQ_F_VALUE = 0x1,
    APESQ_F_MULTIOK = 0x2
};

#ifndef APESQ_API
#define APESQ_API
#endif

struct apesq_entry_st;
struct apesq_section_st {
    /* section type */
    char *sectype;

    /* section name(s) */
    char **secnames;

    /* linked list of entries */
    struct apesq_entry_st *e_head;
    struct apesq_entry_st *e_tail;
};

struct apesq_value_st;
struct apesq_value_st {
    /* value type */
    enum apesq_type type;
    struct apesq_value_st *next;

    /* string data, not valid for booleans */
    char *strdata;
    union {
        int num;
        struct apesq_section_st *section;
    } u;
};

struct apesq_entry_st {
    struct apesq_entry_st *next;
    struct apesq_entry_st *parent;
    char *key;
    struct apesq_value_st value;
    /**
     * user pointer, useful for things like checking if the application
     * has traversed this entry yet
     */
    void *user;
};

/**
 * Macro to extract the section from an entry
 */
#define APESQ_SECTION(ent) \
    ((ent)->value.u.section)

/**
 * Parse a string. Returns the root entry on sucess, or NULL on error.
 * @param str the string to parse. This string *will* be modified, so copy it
 * if you care.
 *
 * @param nstr length of the string, or -1 to have strlen called on it. (non-NUL
 * terminated strings are not supported here anyway, so this should
 */
APESQ_API
struct apesq_entry_st *
apesq_parse_string(char *str, int nstr);

/* wrapper around parse_string */
APESQ_API
struct apesq_entry_st *
apesq_parse_file(const char *path);

/**
 * Frees a configuration tree
 */
APESQ_API
void
apesq_free(struct apesq_entry_st *root);

/**
 * Get a list of sections of the given type
 * @param root the root entry (which should be of T_SECTION)
 * @param name the name of the section type
 *
 * @return a null-terminated array of pointers to T_SECTION entries
 */
APESQ_API
struct apesq_entry_st **
apesq_get_sections(struct apesq_entry_st *root, const char *name);


/**
 * Get a list of values for the key. A key can have more than a single value
 * attached to it; in this case, subsequent values may be accessed using the
 * ->next pointer
 *
 * @param section the section (see APESQ_SECTION)
 * @param key the key for which to read the values
 */
APESQ_API
struct apesq_value_st *
apesq_get_values(struct apesq_section_st *section, const char *key);

/**
 * This will read and possibly coerce a single value to the requested type
 * @param section the section
 *
 * @param param - lookup. This is dependent on the flags. If flags is 0
 * or does not contain F_VALUE, then this is a string with which to look up
 * the entry. If flags & F_VALUE then this is apesq_value_st pointer.
 *
 * @param type - The type into which to coerce
 *
 * @param flags can be a bitwise-OR'd combination of F_VALUE and F_MULTIOK,
 *  the latter of which implies that trailing values are not an error
 *
 * @param out - a pointer to the result. This is dependent on the type requested:
 *      T_INT: int*
 *      T_DOUBLE: double*
 *      T_BOOL: int*
 *
 * @return A status code (APESQ_VALUE_*), should be 'OK' if the conversion
 * was successful, or an error otherwise.
 */
APESQ_API
int
apesq_read_value(struct apesq_section_st *section,
                 const void *param,
                 enum apesq_type type,
                 int flags,
                 void *out);

/**
 * Dump a tree repres
 */
APESQ_API
void
apesq_dump_section(struct apesq_entry_st *root, int indent);

#endif /* APESQ_H_ */

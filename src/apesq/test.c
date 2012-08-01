#include "apesq.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    struct apesq_entry_st *root;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    root = apesq_parse_file(argv[1]);
    if (!root) {
        fprintf(stderr, "Couldn't parse file\n");
        exit(EXIT_FAILURE);
    }
    apesq_dump_section(root, 0);

    {
        int status, coval;
        struct apesq_entry_st **sections =
                apesq_get_sections(root, "Global");
        struct apesq_entry_st **secp;

        struct apesq_value_st *value;
        printf("Found 'Global' section %p\n", sections);
        value = apesq_get_values(APESQ_SECTION(*sections), "Format");
        printf("Got value %p\n", value);
        printf("Value for 'format' is %s\n", value->strdata);

        status = apesq_read_value(APESQ_SECTION(*sections),
                                  "EchoToScreen",
                                  APESQ_T_BOOL,
                                  0,
                                  &coval);
        if (status != APESQ_VALUE_OK) {
            fprintf(stderr, "Couldn't get 'EchoToScreen: EC=%d\n", status);
        } else {
            printf("EchoToScreen: %d\n", coval);
        }

        free(sections);

        sections = apesq_get_sections(root, "Subsys");
        for (secp = sections; *secp; secp++) {
            char **namep;
            printf("Got section %s\n", APESQ_SECTION(*secp)->sectype);
            for (namep = APESQ_SECTION(*secp)->secnames; *namep; namep++) {
                printf(" (%s) ", *namep);
            }
            printf("\n");
            apesq_dump_section(*secp, 0);
        }
    }

    apesq_free(root);
    return 0;
}

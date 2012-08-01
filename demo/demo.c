#include "yolog_out/myproj_yolog.h"
#include <pthread.h>

void *create_func(void *arg)
{
    log_info("I am a thread!");
    log_io_crit("IO Error!!!");
    log_config_warn("Warning.. Something is wrong..");
    log_io_rant("IO Ranting!!");
    return NULL;
}

int main(int argc, char **argv)
{
    int ii;

    pthread_t threads[10];
    char *newfmt = "[%(level)] PID %(pid) %(epoch) %(tid) %(color)";
    char *fname = NULL;
    if (argc == 2) {
        fname = argv[1];
    }

    myproj_yolog_init(fname);

    log_error("This is an error");
    log_config_trace("Reading line 'blah...'");
    log_io_warn("Grrr!!!");
    log_config_crit("Couldn't parse configuration!");
    log_debug("This is a debug message");
    log_info("This is informative");
    log_info("Changing format message");

    myproj_yolog_set_screen_format(&myproj_yolog_log_group, newfmt);
    myproj_yolog_set_screen_format(NULL, newfmt);

    log_error("This should print the timestamp!");



    for (ii = 0; ii < 10; ii++) {
        pthread_create(threads+ii, NULL, create_func, NULL);
    }

    for (ii = 0; ii < 10; ii++) {
        pthread_join(threads[ii], NULL);
    }

    log_warn("Exiting..");

    return 0;
}

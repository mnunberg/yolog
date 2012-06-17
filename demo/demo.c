#include "yolog_out/myproj_yolog.h"
#include <pthread.h>

void *create_func(void *arg)
{
    log_info("I am a thread!");
    log_io_crit("IO Error!!!");
    log_config_warn("Warning.. Something is wrong..");
    return NULL;
}

int main(void)
{
    int ii;

    pthread_t threads[10];
    char *newfmt = "[%(level)] PID %(pid) %(epoch) %(tid) %(color)";


    myproj_yolog_init();
    log_error("This is an error");
    log_io_warn("Grrr!!!");
    log_config_crit("Couldn't parse configuration!");
    log_debug("This is a debug message");

    log_info("Changing format message");


    for (ii = 0; ii < myproj_yolog_subsys_count(); ii++) {
        myproj_yolog_context *ctx = myproj_yolog_logging_contexts + ii;
        ctx->logfmt = newfmt;
    }

    myproj_yolog_get_global()->logfmt = newfmt;

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

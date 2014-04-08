#ifndef MD_BROKER_H_
#define MD_BROKER_H_

#include <pureble.h>


typedef struct {
    pint8 daemon;
    const char* logfile;
    const char* pidfile;
    const char* transport;
} md_app_params;


void broker_main_loop();

#endif

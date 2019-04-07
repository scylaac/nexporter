#ifndef _CMDLINE_H
#define _CMDLINE_H

#include "config.h"

void gather_config (int argc, char* argv[], Config *conf);
void parse_args (int argc, char* argv[], Config *conf);
void parse_env (Config *conf);

#endif

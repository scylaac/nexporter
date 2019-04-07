#include "common.h"
#include "cmdline.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "httpserver.h"

int main (int argc, char* argv[])
{
	Config conf;
	gather_config (argc, argv, &conf);
	dump_config (&conf);

	start_export (&conf);

	return 0;
}

#include "common.h"
#include "cmdline.h"
#include "config.h"
#include "httpserver.h"

#include <stdio.h>
#include <stdlib.h>

#include <signal.h>

void handle_sigint (int sig)
{
	printf (MESG_INFO "Terminating on user interrupt.\n");
	exit (0);
}

int main (int argc, char* argv[])
{
	signal (SIGINT, handle_sigint);

	Config conf;
	gather_config (argc, argv, &conf);
	dump_config (&conf);

	start_export (&conf);

	return 0;
}

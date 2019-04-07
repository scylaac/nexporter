#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <unistd.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"
#include "cmdline.h"
#include "config.h"

static char optstring[] = "f:";

void gather_config (int argc, char* argv[], Config* conf)
{
	/*	Gathers configuration info from:
		1) Command line arguments.
		2) Configuration file (location specified using cmdline args)
		3) environment variables (anticipated)
	*/

	/*	Make a default configuration	*/
	defaultConfig (conf);		
	parse_args (argc, argv, conf);
}

void parse_args (int argc, char* argv[], Config* conf)
{
	char opt;
	char* config_path = NULL;
	int config_fd;

	while ((opt = getopt (argc, argv, optstring)) != -1)
	{
		switch (opt)
		{
			case 'f':
				config_path = optarg;
				break;

			case '?':
				//printf (MESG_FAIL "Unknown option.\n");
				exit (ERROR_CMDLINE_UNKNOWN_OPTION);
				break;
		}
	}

	if (config_path != NULL)
	{
		/*	Try opening config file	*/
		if ((config_fd = open (config_path, O_RDONLY)) == -1)
		{
			printf (MESG_FAIL "Unable to open \"%s\"\n", config_path);
			exit (ERROR_CONFIG_OPEN);
		}
		printf (MESG_INFO "Config path: %s\n", config_path);
	
		/*	Try parsing it	*/
		if (!read_from_file (config_fd, conf, config_path))
		{
			printf (MESG_WARN "Invalid configuration syntax, using default.\n");
			defaultConfig (conf);
		}
		close (config_fd);
	}
	else 
	{
		/*	No config path provided, using default	*/	
		printf (MESG_WARN "No configuration path given, using default settings.\n");
		defaultConfig (conf);
	}
}

void parse_env (Config* conf)
{
	/*	Do nothing for now	*/
	;
}

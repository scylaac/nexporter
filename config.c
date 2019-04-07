#include "config.h"
#include "common.h"

#include <stdio.h> 
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <arpa/inet.h>

const char* keyword [NUM_EXPORTS] = {
	[EXPORT_STAT] = KEYWORD_EXPORT_STAT,
	[EXPORT_MEMINFO] = KEYWORD_EXPORT_MEMINFO,
	[EXPORT_RAPL] = KEYWORD_EXPORT_RAPL
};

void defaultConfig (Config* conf)
{
	conf -> address = (uint64_t) 0;
	conf -> port = DEFAULT_PORT;
	for (int i = 0; i < NUM_EXPORTS; i++)
		conf -> export_flags [i] = false;

	conf -> export_flags[EXPORT_STAT] = true; 
}

ssize_t get_line (int fd, char* buffer, size_t bufsize)
{
	for (int i = 0; i < bufsize; i++)	
		if (read (fd, buffer + i, 1) == 0)
		{
			if (i == 0)
				return -1;
			else
				buffer [i] = '\0';
		}
		else if (buffer[i] == _EOL)
		{
			buffer[i] = '\0';
			return i;
		}

	// If the loop is completed, 
	// the buffer is depleted
	return -2;
}

_Bool next_line (int fd)
{
	char ch;
	while (read (fd, &ch, 1) != 0)
		if (ch == _EOL)
			return true;

	// If read() returns 0, EOF was encountered.
	return false;
}

ssize_t get_token 
	(const char* srcstr, ssize_t start, char* dstbuf, size_t dstsize)
{
	enum {
		SKIP_SPACE,
		TOKEN
	}	parse_state = SKIP_SPACE;
	
	for (int i = start, j = 0; j < dstsize; i++)
	{
		//printf (MESG_WARN "i = %d, j = %d\n", i, j);
		switch (parse_state)
		{
			case SKIP_SPACE:
				switch (srcstr[i])
				{
					case '\0':
						// Got no token left
						return -1;

					case _EOL:
					case ' ':
					case '\t':
						break;

					default:
						parse_state = TOKEN;
						dstbuf [j++] = srcstr [i];
						break;
				}
				break;

			case TOKEN:
				switch (srcstr[i])
				{
					case '\0':
					case _EOL:
					case ' ':
					case '\t':
						dstbuf[j] = '\0';
						return i;

					default:
						dstbuf [j++] = srcstr [i];
						break;
				}
				break;
		}
	}

	// If it hits the end of buffer
	return -2;
}


_Bool read_from_file (int fd, Config *conf, const char* filepath)
{
	/*	Parsing-related states	*/
	enum {
		GET_KEYWORD,
		GET_ADDRESS,
		GET_PORT,
		GET_BOOL

	}	parse_state = GET_KEYWORD;
	int export_switch_idx;


	/*	Tokenizing-related states	*/
	char linebuf [LINE_BSIZE];
	char tokenbuf [TOKEN_BSIZE];
	char gpbuffer [TOKEN_BSIZE];
	int getline_status, lineno = 0, line_idx, token_idx;


	while ((getline_status = get_line (fd, linebuf, LINE_BSIZE)) >= 0)
	{
		lineno += 1;
		printf (MESG_DONE "Got line %02d >>%s<<\n", lineno, linebuf);
		
		line_idx = 0;
		token_idx = 0;

		while ((line_idx = get_token (linebuf, line_idx, tokenbuf, TOKEN_BSIZE)) > 0)
		{
			printf (MESG_INFO "Got token >>%s<< at idx = %d\n", tokenbuf, line_idx);
		
			if ('#' == tokenbuf[0])
			{
				printf (MESG_WARN "Comment detected, skipping to next line.\n");
				break;
			}
				
			switch (parse_state)
			{
				case GET_KEYWORD:
					if (0 != token_idx)
					{
						printf (MESG_FAIL "Garbage found on this line."
							" (%s: %d)\n", filepath, lineno);
						return false;
					}
					if (0 == strcmp (KEYWORD_ADDR, tokenbuf))
						parse_state = GET_ADDRESS;

					else if (0 == strcmp (KEYWORD_PORT, tokenbuf))
						parse_state = GET_PORT;
					
					else for (int i = 0; i < NUM_EXPORTS; i++)
						if (0 == strcmp (keyword[i], tokenbuf))
						{
							parse_state = GET_BOOL;
							export_switch_idx = i;
						}

					break;

				case GET_ADDRESS:
					if (!parse_address (tokenbuf, &(conf -> address)))
					{
						printf (MESG_FAIL "Invalid IPv4 address." 
							" (%s: %d)\n", filepath, lineno);
						return false;
					}
					parse_state = GET_KEYWORD;
					break;
						
				case GET_PORT:
					if (!parse_port (tokenbuf, &(conf -> port)))
					{
						printf (MESG_FAIL "Invalid port number."
							" (%s: %d)\n", filepath, lineno);
						return false;
					}
					parse_state = GET_KEYWORD;
					break;

				case GET_BOOL:
					if (0 == strcmp (KEYWORD_TRUE, tokenbuf))
						conf -> export_flags [export_switch_idx] = true;
					else if (0 == strcmp (KEYWORD_FALSE, tokenbuf))
						conf -> export_flags [export_switch_idx] = false;
					else
					{
						printf (MESG_FAIL "Please type \"yes\" or \"no\"."
							"(%s: %d)\n", filepath, lineno);
						return false;
					}
					parse_state = GET_KEYWORD;
					break;
			}
			token_idx += 1;


		}

		switch (parse_state)
		{
			case GET_ADDRESS:
				printf (MESG_FAIL "Missing address."
					"(%s: %d)\n", filepath, lineno);
				return -1;

			case GET_PORT:
				printf (MESG_FAIL "Missing port number."
					"(%s: %d)\n", filepath, lineno);
				return -1;

			case GET_BOOL:
				printf (MESG_FAIL "Missing \"yes\" or \"no\"."
					"(%s: %d)\n", filepath, lineno);
				return -1;
		}
	
		switch (line_idx)
		{
			case -1:
				printf (MESG_WARN "Hit EOL.\n");
				break;

			case -2:
				printf (MESG_FAIL "Token buffer depleted.\n");
				return false;
		}
	
	}

	switch (getline_status)
	{
		case -1:
			printf (MESG_WARN "Hit EOF.\n");
			break;

		case -2:
			printf (MESG_FAIL "Line buffer depleted.\n");
			return false;
	}

	return true;
}

void dump_config (const Config *conf)
{
	char addrbuf [16];
	inet_ntop (AF_INET, (const void*) &(conf -> address), addrbuf, 16);

	printf (MESG_INFO PROG_NAME " configuration:\n");
	printf (MESG_INDENT   "===================================\n");
	printf (MESG_INDENT "  %-14s: %s\n", "Address", addrbuf);
	printf (MESG_INDENT "  %-14s: %d\n", "Port", conf -> port);
	printf (MESG_INDENT " --------------------------------- \n");

	for (int i = 0; i < NUM_EXPORTS; i++)
		printf (MESG_INDENT "  %-14s: %s\n", keyword [i], 
			(conf -> export_flags[i] ? "YES": "NO"));
	printf (MESG_INDENT "===================================\n\n");
}

_Bool parse_port (const char* portstr, uint16_t* port)
{
	long value;
	char* endptr_value;
	value = strtol (portstr, &endptr_value, 10);
	if (*endptr_value != '\0'
		|| value <= 0 || value > 65535)
		return false;

	*port = (uint16_t) value;
	return true;
}

_Bool parse_address (const char* addrstr, uint32_t* addr)
{
	if (inet_pton (AF_INET, addrstr, addr) != 1)
		return false;
	return true;
}



/*	Testbench main() of config.c	*/
static int test1 ()
{
	int fd = open ("/etc/fstab", O_RDONLY);	
	if (fd == -1)
	{
		printf (MESG_FAIL "Cannot open /etc/fstab.\n");
		exit (ERROR_CONFIG_OPEN);
	}
	
	int status, lineno = 1, line_idx;
	char linebuf [1024];
	char tokenbuf [256];
	while ((status = get_line (fd, linebuf, 1024)) >= 0)
	{
		printf (MESG_DONE "Got line %02d >>%s<<\n", lineno++, linebuf);
		
		line_idx = 0;
		while ((line_idx = get_token (linebuf, line_idx, tokenbuf, 256)) > 0)
			printf (MESG_INFO "Got token >>%s<< at idx = %d\n", tokenbuf, line_idx);
		
		switch (line_idx)
		{
			case -1:
				printf (MESG_WARN "Hit EOL.\n");
				break;

			case -2:
				printf (MESG_FAIL "Token buffer depleted.\n");
				exit (ERROR_BUFFER_OVERFLOW);
		}
	
	}


	switch (status)
	{
		case -1:
			printf (MESG_WARN "Hit EOF.\n");
			break;

		case -2:
			printf (MESG_FAIL "Line buffer depleted.\n");
			exit (ERROR_BUFFER_OVERFLOW);
	}

	return 0;
}

static int test2 ()
{
	uint32_t address;
	uint16_t port;

	printf (MESG_INFO "parse port: %d\n", parse_port ("12222", &port));
	printf (MESG_INFO "parse addr: %d\n", parse_address("101.255.33.3", &address));

	printf (MESG_DONE "Port = %d, Addr = 0x%08x\n", port, address);
	return 0;
}

static int test3 ()
{
	Config conf;	
	defaultConfig (&conf);
	conf.address = 0xcabadaea;
	dump_config (&conf);
}

static int test4 ()
{
	#define _TEST4_DEFAULT_CONF "example.conf"

	Config conf;
	defaultConfig (&conf);

	int conf_fd = open (_TEST4_DEFAULT_CONF, O_RDONLY);
	if (conf_fd == -1)
	{
		printf (MESG_FAIL "Unable to open " _TEST4_DEFAULT_CONF ".\n");
		exit (ERROR_CONFIG_OPEN);
	}

	read_from_file (conf_fd, &conf, _TEST4_DEFAULT_CONF);
	dump_config (&conf);

	return 0;
}

static int main ()
{
	test4();
}


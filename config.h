#ifndef _CONFIG_H
#define _CONFIG_H

#define LINE_BSIZE 1024
#define TOKEN_BSIZE 128

#define _EOL '\n'
#define _COMMENT '#'

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define DEFAULT_PORT 15000

#define KEYWORD_ADDR	"Address"
#define KEYWORD_PORT	"Port"

#define NUM_EXPORTS 3
enum {
	EXPORT_STAT = 0,
	EXPORT_MEMINFO = 1,
	EXPORT_RAPL = 2
};

#define KEYWORD_EXPORT_STAT			"ProcStat"
#define KEYWORD_EXPORT_MEMINFO		"MemInfo"
#define KEYWORD_EXPORT_RAPL			"RAPLMeter"

#define KEYWORD_TRUE				"yes"
#define KEYWORD_FALSE				"no"

const char* keyword [NUM_EXPORTS]; 

typedef struct
{
	uint32_t address;
	uint16_t port;
	_Bool export_flags [NUM_EXPORTS];

} Config;

void defaultConfig (Config* conf);

ssize_t get_line (int fd, char* buffer, size_t bufsize);
_Bool next_line (int fd);
ssize_t get_token (const char* srcstr, ssize_t start, char* dstbuf, size_t dstsize);

_Bool read_from_file (int fd, Config *conf, const char* filepath);
int write_to_file (int fd, Config *conf);

_Bool parse_port (const char* portstr, uint16_t* port);
_Bool parse_address (const char* addrstr, uint32_t* addr);

void dump_config (const Config *conf);

#endif

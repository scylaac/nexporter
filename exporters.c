#define _EXPORT_C

#include "exporters.h"
#include "common.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <fnmatch.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#define SYS_CPU_DEVICE_DIR "/sys/devices/system/cpu"

static int nprocs = -1;

Exporter exporters [NUM_EXPORTS] = {
	[EXPORT_STAT] = export_ProcStat,
	[EXPORT_MEMINFO] = export_Meminfo,
	[EXPORT_RAPL] = export_RAPLMeter
};

static _Bool init_procs ()
{
	int cpu_count = 0;
	DIR* sys_cpu_dir = opendir (SYS_CPU_DEVICE_DIR);

	if (sys_cpu_dir == NULL)
		return false;
	
	const struct dirent *cpu_dir;
	while ((cpu_dir = readdir (sys_cpu_dir)) != NULL)
		if (fnmatch ("cpu[0-9]*", cpu_dir -> d_name, 0) == 0)
			cpu_count ++;
	
	closedir (sys_cpu_dir);
	
	printf (MESG_WARN "init: got %d cpus.\n", cpu_count);
	nprocs = cpu_count;
	return true;
}

static _Bool parse_dashlist (char* list, _Bool* appeared, int length)
{
	enum {
		STATE_BEGIN,
		STATE_HASDIGIT,
		STATE_RANGE,
		STATE_RANGE_HASDIGIT
	}	parse_state = STATE_BEGIN;

	int num1 = 0, num2 = 0;
	char ch;
	
	for (int j = 0; j < length; j++)
		appeared [j] = false;

	for (int i = 0; list[i] != '\0'; i++)
	{
		ch = list[i];
		
		switch (parse_state)
		{
			case STATE_BEGIN:
				if ('0' <= ch && ch <= '9')
				{
					num1 = num1 * 10 + (ch - '0');
					parse_state = STATE_HASDIGIT;
				}
				else
					return false;
					
				break;


			case STATE_HASDIGIT:
				if ('0' <= ch && ch <= '9')
					num1 = num1 * 10 + (ch - '0');
				else if ('-' == ch)
					parse_state = STATE_RANGE;
				else if (',' == ch)
				{
					appeared [num1] = true;
					parse_state = STATE_BEGIN;
				}
				else
					return false;

				break;

			case STATE_RANGE:
				if ('0' <= ch && ch <= '9')
				{
					num2 = num2 * 10 + (ch - '0');
					parse_state = STATE_RANGE_HASDIGIT;
				}
				else
					return false;

				break;
				
			case STATE_RANGE_HASDIGIT:
				if ('0' <= ch && ch <= '9')
					num2 = num2 * 10 + (ch - '0');
				else if (',' == ch)
				{
					if (num2 < num1)
						return false;

					for (int j = num1; j <= num2; j++)
						appeared [j] = true;

					parse_state = STATE_BEGIN;
				}
				else
					return false;

				break;
		}
	}

	switch (parse_state)
	{
		case STATE_BEGIN:
		case STATE_RANGE:
			return false;

		case STATE_HASDIGIT:
			appeared[num1] = true;
			break;
			
		case STATE_RANGE_HASDIGIT:
			if (num2 < num1)
				return false;

			for (int j = num1; j <= num2; j++)
				appeared [j] = true;
			
			break;
	}
	return true;
};


#define CPUTIME_EXPORT_FIELDS 7
enum {
	CPU_USER,
	CPU_NICE,
	CPU_SYSTEM,
	CPU_IDLE,
	CPU_IOWAIT,
	CPU_IRQ,
	CPU_SOFTIRQ
};

const char* cputime_keywords [CPUTIME_EXPORT_FIELDS] = {
	[CPU_USER] = "user",
	[CPU_NICE] = "nice",
	[CPU_SYSTEM] = "system",
	[CPU_IDLE] = "idle",
	[CPU_IOWAIT] = "iowait",
	[CPU_IRQ] = "irq",
	[CPU_SOFTIRQ] = "softirq"
};


#define PROCS_EXPORT_FIELDS 5
enum {
	PROCS_CTXT,
	PROCS_BTIME,
	PROCS_CREATED,
	PROCS_RUNNING,
	PROCS_BLOCKED
};

const char* procs_keywords [PROCS_EXPORT_FIELDS] = {
	[PROCS_CTXT] = "ctxt",
	[PROCS_BTIME] = "btime",
	[PROCS_CREATED] = "processes",
	[PROCS_RUNNING] = "procs_running",
	[PROCS_BLOCKED] = "procs_blocked"
};


ssize_t export_ProcStat (char* export_buf, int bufsize)
{
	/*	Data staging area	*/
	static uint64_t cputime_global [CPUTIME_EXPORT_FIELDS];
	static uint64_t procs_stat [PROCS_EXPORT_FIELDS];
	static uint64_t** cputime;
	static _Bool* cpuonline;

	if (nprocs == -1)	
		if (!init_procs ())
		{
			printf (MESG_FAIL "Unable to determine the number of CPUs.\n");
			return -1;
		}
		else
		{
			cputime = (uint64_t**) malloc (sizeof(uint64_t*) * nprocs);
			for (int i = 0; i < nprocs; i++)
				cputime[i] = (uint64_t*) malloc (sizeof (uint64_t) 
					* CPUTIME_EXPORT_FIELDS);

			cpuonline = (_Bool*) malloc (sizeof (_Bool) * nprocs);
		}


	/*	Scrape cpu online status	*/
	int cpuonline_fd = open (SYS_CPU_DEVICE_DIR "/online", O_RDONLY);
	char gpbuffer [GPBUFSIZE];
	char tokenbuf [GPBUFSIZE];

	if (cpuonline_fd == -1 
		|| get_line (cpuonline_fd, gpbuffer, GPBUFSIZE) < 0
		|| !parse_dashlist (gpbuffer, cpuonline, nprocs))
	{
		printf (MESG_FAIL "Unable to determine which CPUs are online.\n");
		return -1;
	}

	close (cpuonline_fd);

	for (int i = 0; i < nprocs; i++)
		printf (MESG_INFO "CPU %d is %s\n", i, 
			(cpuonline[i] ? "ONLINE" : "OFFLINE"));

	
	/*	Scrape /proc/stat	*/
	enum {
		GET_CPUTIME_GLOBAL,
		GET_CPUTIME,
		GET_PROCS,
		DONE
	}	proc_parse_status = GET_CPUTIME_GLOBAL;


	int stat_fd = open ("/proc/stat", O_RDONLY);
	int stat_scrape_status = 0, line_idx, field_idx;
	
	while (proc_parse_status != DONE &&
		(stat_scrape_status = get_line (stat_fd, gpbuffer, GPBUFSIZE)) > 0)
	{
		line_idx = 0;
		switch (proc_parse_status)
		{
			case GET_CPUTIME_GLOBAL:
				if ((line_idx = get_token (
					gpbuffer, line_idx, tokenbuf, GPBUFSIZE)) <= 0)
				{
					printf (MESG_FAIL "Unable to parse /proc/stat.");
					return -1;
				}

				printf (MESG_WARN "Assuming: %s == %s\n", tokenbuf, "cpu");

				for (int i = 0; i < CPUTIME_EXPORT_FIELDS; i++)
				{
					line_idx = get_token (
						gpbuffer, line_idx, tokenbuf, GPBUFSIZE);
					
					cputime_global [i] = strtol (tokenbuf, NULL, 10);
				}

				proc_parse_status = GET_CPUTIME;
				field_idx = -1;	// this variable has mutiple uses
				for (int i = 0; i < nprocs; i++)
					if (cpuonline[i])
					{
						field_idx = i;
						break;
					}

				if (field_idx == -1)
				{
					printf (MESG_WARN "No ONLINE CPU detected.");
					/*	Skip the intr line, and jump directly into GET_PROCS 	*/
					next_line (stat_fd);
					proc_parse_status = GET_PROCS;
					field_idx = 0;
				}
				break;
			
			case GET_CPUTIME:
				if ((line_idx = get_token (
					gpbuffer, line_idx, tokenbuf, GPBUFSIZE)) <= 0)
				{
					printf (MESG_FAIL "Unable to parse /proc/stat.");
					return -1;
				}
	
				printf (MESG_WARN "Assuming: %s == %s%d\n", 
					tokenbuf, "cpu", field_idx);

				for (int j = 0; j < CPUTIME_EXPORT_FIELDS; j++)
				{
					line_idx = get_token (
						gpbuffer, line_idx, tokenbuf, GPBUFSIZE);
					
					cputime [field_idx][j] = strtol (tokenbuf, NULL, 10);
				}
	

				/*	Jump to the next online CPU	*/
				int tmp_idx = -1;
				for (int i = field_idx + 1; i < nprocs; i++)
					if (cpuonline[i])
					{
						tmp_idx = i;
						break;
					}

				field_idx = tmp_idx;

				if (field_idx == -1)
				{
					/*	trick: skip the following super-long 'intr' line	*/
					next_line (stat_fd);

					field_idx = 0;
					proc_parse_status = GET_PROCS;
				}
				break;


			case GET_PROCS:
				if ((line_idx = get_token (
					gpbuffer, line_idx, tokenbuf, GPBUFSIZE)) <= 0)
				{
					printf (MESG_FAIL "Unable to parse /proc/stat.");
					return -1;
				}

				printf (MESG_WARN "Assuming: %s == %s\n", 
					tokenbuf, procs_keywords[field_idx]);

				line_idx = get_token (
					gpbuffer, line_idx, tokenbuf, GPBUFSIZE);

				procs_stat [field_idx] = strtol (tokenbuf, NULL, 10);	


				if (field_idx == PROCS_EXPORT_FIELDS - 1)
					proc_parse_status = DONE;
				else
					field_idx += 1;

				break;
		}
	}

	close (stat_fd);

	if (stat_scrape_status < 0 || proc_parse_status != DONE)
	{
		printf (MESG_FAIL "Unable to parse /proc/stat.\n");
		return -1;
	}

	/*
	printf (MESG_INFO   "Global CPU time statistics:\n");
	printf (MESG_INDENT "==============================\n");
	for (int j = 0; j < CPUTIME_EXPORT_FIELDS; j++)
		printf (MESG_INDENT "  %-10s: %lu\n", 
			cputime_keywords[j], cputime_global[j]);
	printf (MESG_INDENT "==============================\n\n");


	for (int i = 0; i < nprocs; i++)
	{
		printf (MESG_INFO "CPU %d is %s\n",
			i, (cpuonline[i] ? "ONLINE": "OFFLINE"));

		if (cpuonline[i])
		{
			printf (MESG_INDENT "==============================\n");
			for (int j = 0; j < CPUTIME_EXPORT_FIELDS; j++)
				printf (MESG_INDENT "  %-10s: %lu\n", 
					cputime_keywords[j], cputime [i][j]);
			printf (MESG_INDENT "==============================\n\n");
		}
		else printf ("\n");

	}

	printf (MESG_INFO "Process statistics:\n");
	printf (MESG_INDENT "==============================\n");
	for (int j = 0; j < PROCS_EXPORT_FIELDS; j++)
		printf (MESG_INDENT "  %-10s: %lu\n",
			procs_keywords[j], procs_stat[j]);
	printf (MESG_INDENT "==============================\n\n");
	*/

	char* cursor = export_buf;

	/*	Exporting the cpuonline metrics	*/
	cursor += sprintf (cursor, "%s %s_%s %s\n", 
						PROM_HELP_HEADER,
						PROC_STAT_HEADER,
						PROC_STAT_CPUONLINE,
						"CPU online status, 1 for online, 0 for offline.");
	
	cursor += sprintf (cursor, "%s %s_%s %s\n", 
						PROM_TYPE_HEADER,
						PROC_STAT_HEADER,
						PROC_STAT_CPUONLINE,
						PROM_GAUGE);
	
		
	for (int i = 0; i < nprocs; i++)
		cursor += sprintf (cursor, "%s_%s{%s=\"%d\"} %d\n",
						PROC_STAT_HEADER,
						PROC_STAT_CPUONLINE,
						PROC_STAT_CPU_TAG, i,
						(cpuonline[i] ? 1: 0));


	/*	Exporting the cputime metrics	*/
	cursor += sprintf (cursor, "%s %s_%s %s\n", 
						PROM_HELP_HEADER,
						PROC_STAT_HEADER,
						PROC_STAT_CPUTIME,
						"cputime metrics in jiffies.");
	
	cursor += sprintf (cursor, "%s %s_%s %s\n", 
						PROM_TYPE_HEADER,
						PROC_STAT_HEADER,
						PROC_STAT_CPUTIME,
						PROM_GAUGE);


	for (int j = 0; j < CPUTIME_EXPORT_FIELDS; j++)
		cursor += sprintf (cursor, "%s_%s{%s=\"%s\",%s=\"%s\"} %lu\n", 
						PROC_STAT_HEADER,
						PROC_STAT_CPUTIME,
						PROC_STAT_CPU_TAG,
						PROC_STAT_CPU_TAG_GLOBAL_VALUE,
						PROC_STAT_CPUMODE_TAG,
						cputime_keywords [j],
						cputime_global [j]);

	for (int i = 0; i < nprocs; i++)
	{
		if (!cpuonline[i]) continue;
		
		for (int j = 0; j < CPUTIME_EXPORT_FIELDS; j++)
			cursor += sprintf (cursor, "%s_%s{%s=\"%d\",%s=\"%s\"} %lu\n", 
						PROC_STAT_HEADER,
						PROC_STAT_CPUTIME,
						PROC_STAT_CPU_TAG, i,
						PROC_STAT_CPUMODE_TAG,
						cputime_keywords [j],
						cputime [i][j]);
	}

	/*	Exporting the global process metrics	*/
	for (int i = 0; i < PROCS_EXPORT_FIELDS; i++)
	{
		cursor += sprintf (cursor, "%s %s_%s %s\n", 
						PROM_TYPE_HEADER,
						PROC_STAT_HEADER,
						procs_keywords[i],
						PROM_GAUGE);

		cursor += sprintf (cursor, "%s_%s %lu\n", 
						PROC_STAT_HEADER,
						procs_keywords[i],
						procs_stat [i]);
	}
	return (cursor - export_buf);
}

ssize_t export_Meminfo (char* export_buf, int bufsize)
{
	return true;
}

ssize_t export_RAPLMeter (char* export_buf, int bufsize)
{
	return true;
}

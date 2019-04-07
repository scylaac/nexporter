#ifndef _EXPORTERS_H
#define _EXPORTERS_H

#include "config.h"

#define PROM_HELP_HEADER "# HELP"
#define PROM_TYPE_HEADER "# TYPE"

#define PROM_GAUGE	"gauge"
#define PROM_CTR	"counter"
#define PROM_SMRY	"summary"
#define PROM_HIST	"histogram"

#define PROC_STAT_HEADER "nstat"
	#define PROC_STAT_CPUONLINE "cpuonline"
	#define PROC_STAT_CPUTIME "cputime"
		#define PROC_STAT_CPUMODE_TAG "mode"
		#define PROC_STAT_CPU_TAG "cpu"
			#define PROC_STAT_CPU_TAG_GLOBAL_VALUE "global"


typedef ssize_t (*Exporter)(char* export_buf, int bufsize);

#ifndef _EXPORT_C
extern Exporter exporters [NUM_EXPORTS];
#endif

ssize_t export_ProcStat (char* export_buf, int bufsize) __attribute__((used));
ssize_t export_Meminfo (char* export_buf, int bufsize) __attribute__((used));
ssize_t export_RAPLMeter (char* export_buf, int bufsize) __attribute__((used));


#endif

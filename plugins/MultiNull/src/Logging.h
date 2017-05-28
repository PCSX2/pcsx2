
#ifndef MULTINULL_LOGGING_H
#define MULTINULL_LOGGING_H
#include <stdio.h>
#include <stdbool.h>
typedef struct
{
	const char* logName;
	const char* confDir;
	const char* logDir;
	FILE* currentLog;
} loggingInfo;

#ifdef MULTINULL_ENABLE_LOGGING
bool openLog(loggingInfo* li);
void closeLog(loggingInfo* li);
void doLog(loggingInfo* li, const char* fmt, ...);
void setLogDir(loggingInfo* li, const char* logdir);
void setConfigDir(loggingInfo* li, const char* confdir);
void setLogName(loggingInfo* li, const char* logName);
#else
#define openLog(...)
#define closeLog(...)
#define doLog(...)
#define setLogDir(...)
#define setConfigDir(...)
#define setLogName(...)
#endif

#endif
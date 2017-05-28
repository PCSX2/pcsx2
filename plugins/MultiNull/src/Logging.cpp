
#include "Main.h"
#ifdef MULTINULL_ENABLE_LOGGING
bool openLog(loggingInfo* li) {
    bool result = true;
    if (li->currentLog) return result;
    if (li->logDir == NULL) li->logDir = "logs";
    if (li->confDir == NULL) li->confDir = "inis";
    char *filepath;
	asprintf(&filepath,"%s%s%s%s",li->logDir,"/",li->logName,".log");
    li->currentLog = fopen(filepath, "w");
    if (li->currentLog != NULL)
        setvbuf(li->currentLog, NULL, _IONBF, 0);
    else {
        result = false;
    }
    return result;
}

void closeLog(loggingInfo* li) {
	if (li->currentLog) {
        fclose(li->currentLog);
        li->currentLog = NULL;
    }
}

void doLog(loggingInfo* li, const char* fmt, ...) {
	if (li->currentLog == NULL) return;
	//fprintf(li->currentLog, textToLog);
	va_list list;
	va_start(list, fmt);
	vfprintf(li->currentLog, fmt, list);
	va_end(list);
}

void setLogDir(loggingInfo* li, const char* logdir) {
	li->logDir = (logdir == NULL) ? "logs" : logdir;
	closeLog(li);
	openLog(li);
}

void setConfigDir(loggingInfo* li, const char* confdir) {
	li->confDir = (confdir == NULL) ? "inis" : confdir;
}

void setLogName(loggingInfo* li, const char* logName) {
    li->logName = logName;
}
#endif

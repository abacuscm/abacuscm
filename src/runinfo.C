/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "runinfo.h"
#include "logger.h"

#include <string>
#include <fstream>
#include <string.h>
#include <stdlib.h>

using namespace std;

RunInfo::RunInfo(const char *filename) {
    // initialise sane defaults
    time = 0;
    timeExceeded = cpuExceeded = realTimeExceeded = false;
    exitCode = signal = -1;
    normalTerm = signalTerm = false;
    signalName = NULL;

    ifstream file_runinfo(filename);
    while (!file_runinfo.eof()) {
        string theLine;
        getline(file_runinfo, theLine);
        if (theLine.length() == 0)
            continue;
        const char *line = theLine.c_str();
        if (strncmp(line, "RUNERR", 6)  == 0) {
            log(LOG_ERR, "%s", line + 7);
        }
        else if (strncmp(line, "RUNINF", 6) == 0) {
            const char *sub = line + 7;
            if (strncmp(sub, "TIME", 4) == 0) {
                char reason[1024];
                sscanf(sub + 5, "%u %s", &time, reason);
                if (strcmp(reason, "ok") == 0) {
                    // finished in sane amount of time
                }
                else if (strcmp(reason, "cpu_exceeded") == 0) {
                    // CPU time exceeded
                    timeExceeded = cpuExceeded = true;
                }
                else if (strcmp(reason, "realtime_exceeded") == 0) {
                    // real-time exceeded
                    timeExceeded = realTimeExceeded = true;
                }
                else {
                    // uh-oh
                    // flag this somehow: set time exceeded but neither of the realTime or cpu variants
                    timeExceeded = true;
                    log(LOG_DEBUG, "Invalid run-info string: '%s'", line + 7);
                }
            }
            else if (strncmp(sub, "TERM", 4) == 0) {
                const char *termType = sub + 5;
                if (strncmp(termType, "normal", 6) == 0) {
                    // normal termination
                    sscanf(termType + 7, "excode=%d", &exitCode);
                    normalTerm = true;
                }
                else if (strncmp(termType, "signal", 6) == 0) {
                    // signal'ed termination
                    sscanf(termType + 7, "signum=%d signame=%as", &signal, &signalName);
                    signalTerm = true;
                }
                else {
                    // uh-oh
                    log(LOG_DEBUG, "Invalid run-info string: '%s'", line + 7);
                }
            }
            else {
                // uh-oh
                log(LOG_DEBUG, "Invalid run-info string: '%s'", line + 7);
            }
        }
        else {
            // uh-oh
            log(LOG_DEBUG, "Invalid run-info string: '%s'", line + 7);
        }
    }
    file_runinfo.close();
}

RunInfo::~RunInfo() {
    if (signalName)
        free(signalName);
}

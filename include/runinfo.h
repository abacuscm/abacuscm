/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef RUN_INFO_H
#define RUN_INFO_H

#if HAVE_CONFIG_H
# include <config.h>
#endif

class RunInfo {
public:
    RunInfo(const char *filename);
    ~RunInfo();

    unsigned int time;
    bool timeExceeded;
    bool cpuExceeded;
    bool realTimeExceeded;

    int exitCode;
    int signal;
    char *signalName;
    bool signalTerm;
    bool normalTerm;
};

#endif

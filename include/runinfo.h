#ifndef RUN_INFO_H
#define RUN_INFO_H

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

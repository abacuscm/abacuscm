#define _GNU_SOURCE
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>

#include "logger.h"

#if defined(REG_RIP)
#define REGFORMAT "%016lx"
#elif defined(REG_EIP)
#define REGFORMAT "%08x"
#else
#define REGFORMAT "%x"
#endif

static void signal_segv(int signum, siginfo_t* info, void*ptr) {
	static const char *si_codes[3] = {"", "SEGV_MAPERR", "SEGV_ACCERR"};

	int i, f = 0;
	ucontext_t *ucontext = (ucontext_t*)ptr;
	Dl_info dlinfo;
	void **bp = 0;
	void *ip = 0;

	clog(LOG_CRIT, "Segmentation Fault!");
	clog(LOG_CRIT, "info.si_signo = %d", signum);
	clog(LOG_CRIT, "info.si_errno = %d", info->si_errno);
	clog(LOG_CRIT, "info.si_code  = %d (%s)", info->si_code, si_codes[info->si_code]);
	clog(LOG_CRIT, "info.si_addr  = %p", info->si_addr);
	for(i = 0; i < NGREG; i++)
		clog(LOG_CRIT, "reg[%02d]       = 0x" REGFORMAT, i, ucontext->uc_mcontext.gregs[i]);
#if defined(REG_RIP)
	ip = (void*)ucontext->uc_mcontext.gregs[REG_RIP];
	bp = (void**)ucontext->uc_mcontext.gregs[REG_RBP];
#elif defined(REG_EIP)
	ip = (void*)ucontext->uc_mcontext.gregs[REG_EIP];
	bp = (void**)ucontext->uc_mcontext.gregs[REG_EBP];
#else
	clog(LOG_CRIT, "Unable to retrieve Instruction Pointer (not printing stack trace).");
#endif
	while(bp && ip) {
		if(!dladdr(ip, &dlinfo))
			break;

		clog(LOG_CRIT, "% 2d: %p <%s+%u> (%s)",
				++f,
				ip,
				dlinfo.dli_sname,
				(unsigned)(ip - dlinfo.dli_saddr),
				dlinfo.dli_fname);

		if(dlinfo.dli_sname && !strcmp(dlinfo.dli_sname, "main"))
			break;

		ip = bp[1];
		bp = (void**)bp[0];
	}
	exit (-1);
}

int setup_sigsegv() {
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = signal_segv;
	action.sa_flags = SA_SIGINFO;
	if(sigaction(SIGSEGV, &action, NULL) < 0) {
		perror("sigaction");
		return 0;
	}

	return 1;
}
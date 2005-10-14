#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>

#ifndef VERSION
#define VERSION "trunk"
#endif

static struct option const long_options[] = {
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'v'},
	{"debug", no_argument, 0, 'd'},
	
	{"chroot", required_argument, 0, 'r'},
	{"cputime", required_argument, 0, 'c'},
	{"realtime", required_argument, 0, 't'},
	{"memory", required_argument, 0, 'm'},
	{"user", required_argument, 0, 'u'},
	{"group", required_argument, 0, 'g'},
	{"nproc", required_argument, 0, 'n'},
	{NULL, 0, NULL, 0}
};

static const char *optstring = "hvdr:c:t:m:u:g:n:";

static unsigned cputime = 0;
static unsigned realtime = 0;
static unsigned memlimit = 0;
static unsigned nproc = 0;
static char* chrootdir = NULL;
static int verbose = 0;
static uid_t to_uid = 0;
static gid_t to_grp = 0;
static const char* to_uname = NULL;
static const char* to_gname = NULL;
static FILE* runmsg = NULL;

static volatile int realtime_fired = 0;

static void sig_alarm(int i __attribute__((unused))) {
	realtime_fired = 1;
}

static void msgout(const char* prefix, const char* fmt, va_list ap) {
	fprintf(runmsg, "%s ", prefix);
	vfprintf(runmsg, fmt, ap);
}

static void errmsg(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
static void errmsg(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	msgout("RUNERR", fmt, ap);
	va_end(ap);
}

static void infmsg(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
static void infmsg(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	msgout("RUNINF", fmt, ap);
	va_end(ap);
}

static void msg(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
inline
static void msg(const char* fmt, ...) {
	if(!verbose)
		return;
	
	va_list ap;
	va_start(ap, fmt);
	msgout("RUNDBG", fmt, ap);
	va_end(ap);
}

static void help() {
	fprintf(runmsg, "USAGE: runlimit [options] command [params]\n"
			"  Where options is 0 or more of:\n"
			"    --help, -h      Display this text and exit\n"
			"    --version, -v   Display version info and exit\n"
			"    --chroot, -r    chroot to given directory before executing command\n"
			"    --cputime, -c   CPU-time limit in milliseconds\n"
			"    --realtime, -t  realtime limit in milliseconds\n"
			"    --memory, -m    Virtual memory size limit in bytes\n"
			"    --user, -u      User to run command as\n"
			"    --group, -g     Group to run command as\n"
			"    --nproc, -n     Max number of processes (BE WARNED: This is enforced on a per user basis)\n"
	   );
}

void __attribute__((noreturn)) do_child(char **argv) {
	struct rlimit limit;

	if(chrootdir) {
		if(geteuid() == 0) {
			if(chroot(chrootdir) < 0) {
				errmsg("chroot: %s\n", strerror(errno));
				exit(-1);
			}
			if(chdir("/") < 0) {
				errmsg("chdir(\"/\") - non-critical: %s\n", strerror(errno));
			}
		} else { // we are not root!
			errmsg("Need suid root in order chroot!  chdir()ing instead.\n");
			if(chdir(chrootdir) < 0) {
				errmsg("chdir: %s\n", strerror(errno));
				exit(-1);
			}
		}
	}

	if(geteuid() == 0) { // we are root - we need to drop...
		if(to_grp)
			setgid(to_grp);
		else
			setgid(getgid());

		setgroups(0, NULL);

		if(to_uid)
			setuid(to_uid);
		else
			setuid(getuid());	
	} else if(to_grp || to_uid)
		errmsg("Cannot change user/group since effective user id of runlimit is not root\n");
	
	if(cputime) {
		/**
		 * this rlimit is in seconds, and we always want to
		 * overshoot by at least 10ms (kernel is accurate upto
		 * about 1ms in measured times).
		 */
		limit.rlim_cur = (cputime + 1010) / 1000;
		limit.rlim_max = (cputime + 1010) / 1000;

		if(setrlimit(RLIMIT_CPU, &limit) < 0) {
			errmsg("setrlimit(RLIMIT_CPU): %s\n", strerror(errno));
			exit(-1);
		}
	}

	if(memlimit) {
		limit.rlim_cur = memlimit;
		limit.rlim_max = memlimit;

		if(setrlimit(RLIMIT_AS, &limit) < 0) {
			errmsg("setrlimit(RLIMIT_AS): %s\n", strerror(errno));
			exit(-1);
		}
        }

	if (1) { /* FIXME: take a command line option */
		limit.rlim_cur = 0x1000000; /* 16MB */
		limit.rlim_max = 0x1000000;

		if (setrlimit(RLIMIT_FSIZE, &limit) < 0) {
			errmsg("setrlimit(RLIMIT_FSIZE): %s\n", strerror(errno));
			exit(-1);
		}
        }

	if(nproc) {
		limit.rlim_cur = nproc;
		limit.rlim_max = nproc;

		if(setrlimit(RLIMIT_NPROC, &limit) < 0) {
			errmsg("setrlimit(RLIMIT_NPROC): %s\n", strerror(errno));
			exit(-1);
		}
	}

	if(access(argv[0], X_OK) < 0) {
		errmsg("access: %s\n", strerror(errno));
		exit(-1);
	}
	
	setsid();

	fflush(runmsg);
	close(3);

	execve(argv[0], argv, NULL);
	perror("execve");
	exit(-1);
}

int main(int argc, char** argv) {
	int c;
	char *err;
	int i;
	pid_t pid;

	runmsg = fdopen(3, "w");
	if(!runmsg) {
		dup2(2,3);
		runmsg = fdopen(3, "w");
		errmsg("Using stderr for runinfo\n");
	}

	while((c = getopt_long(argc, argv, optstring, long_options, NULL)) != EOF) {
		switch(c) {
		case 'h':
			help();
			return 0;
		case 'v':
			fprintf(runmsg, "runlimit %s\n", VERSION);
			return 0;
		case 'd':
			verbose = 1;
			break;
		case 'r':
			chrootdir = optarg;
			break;
		case 't':
			realtime = strtol(optarg, &err, 0);
			if(*err) {
				errmsg("'%s' is not a valid integer!\n", optarg);
				return -1;
			}
			break;
		case 'c':
			cputime = strtol(optarg, &err, 0);
			if(*err) {
				errmsg("'%s' is not a valid integer!\n", optarg);
				return -1;
			}
			break;
		case 'm':
			memlimit = strtol(optarg, &err, 0);
			if(*err) {
				errmsg("'%s' is not a valid integer!\n", optarg);
				return -1;
			}
			break;
		case 'n':
			nproc = strtol(optarg, &err, 0);
			if(*err) {
				errmsg("'%s' is not a valid integer!\n", optarg);
				return -1;
			}
			break;
		case 'u':
			{
				struct passwd* pwent = getpwnam(optarg);
				if(!pwent) {
					errmsg("User '%s' does not exist - continueing anyway\n",
							optarg);
				} else {
					to_uname = optarg;
					to_uid = pwent->pw_uid;
					if(!to_gname)
						to_grp = pwent->pw_gid;
				}
			}
			break;
		case 'g':
			{
				struct group* grent = getgrnam(optarg);
				if(!grent) {
					errmsg("Group '%s' does not exist - continueing anyway\n",
							optarg);
				} else {
					to_gname = optarg;
					to_grp = grent->gr_gid;
				}
			}
			break;
		default:
			msg("Ignoring unknown switch %c.\n", c);
		}
	}

	msg("runlimit options:\n");
	msg("chroot:          '%s'\n", chrootdir);
	msg("CPU-time limit:  %ums\n", cputime);
	msg("Real-time limit: %ums\n", realtime);
	msg("Memory limit:    %ubytes\n", memlimit);
	msg("Target User:     '%s'\n", to_uname);
	msg("Target Group:    '%s'\n", to_gname);
	msg("Target uid:      %d\n", to_uid);
	msg("Target gid:      %d\n", to_grp);

	if(realtime && cputime && cputime > realtime) {
		errmsg("realtime must be greater than cputime.\n");
		return -1;
	}

	if(argc == optind) {
		errmsg("Missing program to execute.\n");
		help();
		return -1;
	}

	msg("program to execute:  '%s'\n", argv[optind]);
	msg("program arguments:\n");
	for(i = optind + 1; i < argc; ++i)
		msg("%d '%s'\n", i - optind, argv[i]);
	msg("---\n");

	if(argv[argc]) {
		errmsg("argv[] must have a NULL at the end!");
		return -1;
	}
	
	pid = fork();
	if(pid < 0) {
		errmsg("fork: %s\n", strerror(errno));
		return -1;
	} else if(pid == 0) {
		do_child(&argv[optind]);
	} else {
		pid_t res;
		int status;
		char* term_reason = NULL;
		struct rusage cpu_usage;

		if(realtime) {
			struct sigaction action;
			
			memset(&action, 0, sizeof(action));
			action.sa_handler = sig_alarm;
			sigaction(SIGALRM, &action, NULL);
			
			alarm((realtime + 1050) / 1000);
		}
		while((res = waitpid(pid, &status, 0)) < 0) {
			if(errno == EINTR && realtime_fired) {
				if(kill(-pid, SIGKILL) < 0)
					errmsg("kill(SIGKILL): %s\n", strerror(errno));
				if(kill(-pid, SIGCONT) < 0) // in case of STOPed child.
					errmsg("kill(SIGKILL): %s\n", strerror(errno));
				term_reason = "realtime";
			} else
				errmsg("waitpid: %s\n", strerror(errno));
		}
		if(realtime)
			alarm(0);

		// Nuke any other processes that may have been spawned.
		if(kill(-pid, SIGKILL) < 0 && errno != ESRCH)
			errmsg("kill(SIGKILL): %s\n", strerror(errno));
		if(kill(-pid, SIGCONT) < 0 && errno != ESRCH)
			errmsg("kill(SIGKILL): %s\n", strerror(errno));

		// determine _what_ caused the process to die.
		// yes, I know all these asprintf's causes a memory leak.
		if(WIFEXITED(status)) {
			asprintf(&term_reason, "normal excode=%d", WEXITSTATUS(status));
		} else if(WIFSIGNALED(status)) {
			int signum = WTERMSIG(status);
			const char* name = strsignal(signum);
			if(!name)
				name = "unknown";
			asprintf(&term_reason, "signal signum=%d signame=%s", signum, name);
		} else
			term_reason = "unknown";
		infmsg("TERM %s\n", term_reason);

		if(getrusage(RUSAGE_CHILDREN, &cpu_usage) == 0) {
			unsigned totaltime = 0;
			totaltime += cpu_usage.ru_utime.tv_sec * 1000000;
			totaltime += cpu_usage.ru_stime.tv_sec * 1000000;
			totaltime += cpu_usage.ru_utime.tv_usec;
			totaltime += cpu_usage.ru_stime.tv_usec;
			totaltime = (totaltime + 500) / 1000; // round to closest millisec.
			
			const char *time_reason = "ok";
			if(cputime && totaltime > cputime)
				time_reason = "cpu_exceeded";
			if(realtime_fired)
				time_reason = "realtime_exceeded";

			infmsg("TIME %u %s\n", totaltime, time_reason);
		} else
			infmsg("TIME noinfo\n");
		return 0;
	}
	return -1;
}

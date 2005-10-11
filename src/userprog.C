#include "userprog.h"
#include "logger.h"
#include "config.h"
#include "buffer.h"

#include <sys/wait.h>
#include <fcntl.h>
#include <sstream>

using namespace std;

map<string, UserProgFunctor> UserProg::_functors
		__attribute__((init_priority(101)));

UserProg::UserProg() {
}

UserProg::~UserProg() {
}

int UserProg::execcompiler(list<string> p_argv) {
	pid_t pid = fork();
	if(pid < 0) {
		lerror("fork");
		return -1;
	} else if(pid == 0) {
		char ** argv = new char*[p_argv.size() + 1];
		char ** ptr = argv;
		list<string>::iterator i;
		for(i = p_argv.begin(); i != p_argv.end(); ++i)
			*ptr++ = strdup(i->c_str());
		*ptr = NULL;
		// we are not interrested in compiler output.
		close(1);
		close(2);
		int fd = open("/dev/null", O_RDONLY);
		if(fd > 0) {
			if(fd != 1)
				dup2(fd, 1);
			if(fd != 2)
				dup2(fd, 2);
			if(fd > 2)
				close(fd);
		}
		execve(*argv, argv, NULL);
		lerror("execve");
		exit(-1);
	} else { // parent
		int status;
		while(waitpid(pid, &status, 0) < 0)
			lerror("waitpid");
		return status;
	}
}

void UserProg::setRootDir(string root) {
	_runlimit_args.push_back("-r");
	_runlimit_args.push_back(root);
}

void UserProg::setCPUTime(unsigned cputime) {
	ostringstream tmpstrm;
	tmpstrm << cputime;

	_runlimit_args.push_back("-c");
	_runlimit_args.push_back(tmpstrm.str());
}

void UserProg::setRealTime(unsigned realtime) {
	ostringstream tmpstrm;
	tmpstrm << realtime;

	_runlimit_args.push_back("-t");
	_runlimit_args.push_back(tmpstrm.str());
}

void UserProg::setMemLimit(unsigned bytes) {
	ostringstream tmpstrm;
	tmpstrm << bytes;

	_runlimit_args.push_back("-m");
	_runlimit_args.push_back(tmpstrm.str());
}
	
void UserProg::setRuntimeUser(std::string uname) {
	_runlimit_args.push_back("-u");
	_runlimit_args.push_back(uname);
}

void UserProg::setRuntimeGroup(std::string gname) {
	_runlimit_args.push_back("-g");
	_runlimit_args.push_back(gname);
}

void UserProg::setMaxProcs(unsigned nproc) {
	ostringstream tmpstrm;
	tmpstrm << nproc;

	_runlimit_args.push_back("-n");
	_runlimit_args.push_back(tmpstrm.str());
}

void UserProg::registerLanguage(string lang, UserProgFunctor func) {
	_functors[lang] = func;
}

UserProg* UserProg::createUserProg(std::string lang) {
	UserProgFunctor func = _functors[lang];
	if(!func)
		return NULL;
	return func();
}

string UserProg::sourceFilename(const Buffer&) {
	return "source";
}

int UserProg::exec(int fd_in, int fd_out, int fd_err, int fd_run) {
	list<string> prog_argv = getProgramArgv();
	
	char ** argv = new char*[_runlimit_args.size() + prog_argv.size() + 3];
	char ** ptr = argv;

	*ptr++ = strdup(Config::getConfig()["marker"]["runlimit"].c_str());
	
	list<string>::iterator i;
	for(i = _runlimit_args.begin(); i != _runlimit_args.end(); ++i)
		*ptr++ = strdup(i->c_str());

	*ptr++ = strdup("--");

	for(i = prog_argv.begin(); i != prog_argv.end(); ++ i)
		*ptr++ = strdup(i->c_str());

	*ptr = NULL;

	for(ptr = argv; *ptr; ptr++)
		log(LOG_DEBUG, "'%s'", *ptr);

	close(0);
	close(1);
	close(2);
	dup2(fd_in, 0);
	dup2(fd_out, 1);
	dup2(fd_err, 2);
	close(fd_in);
	close(fd_out);
	close(fd_err);

	if(fd_run != 3) {
		close(3); // will most probably fail ...
		if(dup2(fd_run, 3) < 0)
			lerror("dup2");
		close(fd_run);
	}

	execve(*argv, argv, NULL);

	lerror("execve");
	exit(-1);
}

/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "userprog.h"
#include "logger.h"
#include "acmconfig.h"
#include "buffer.h"
#include "misc.h"

#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

using namespace std;

map<string, UserProgFunctor> UserProg::_functors
		__attribute__((init_priority(101)));

UserProg::UserProg() {
	_work_dir = "/";
}

UserProg::~UserProg() {
}

int UserProg::execcompiler(list<string> p_argv, string compiler_log) {
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
		int fd = open(compiler_log.c_str(), O_RDWR | O_CREAT, 0600);
		if(fd > 0) {
			if(fd != 1)
				dup2(fd, 1);
			if(fd != 2)
				dup2(fd, 2);
			if(fd > 2)
				close(fd);
		}
		execv(*argv, argv);
		lerror("execv");
		/* Note: not 'exit', since that would call all our destructors
		 */
		_exit(-1);
	} else { // parent
		int status;
		while(waitpid(pid, &status, 0) < 0)
			lerror("waitpid");
		return status;
	}
}

list<string> UserProg::getProgramEnv() {
	return list<string>();
}

void UserProg::setWorkDir(string dir) {
	_work_dir = dir;
}

void UserProg::setRootDir(string root) {
	_runlimit_args.push_back("-r");
	_runlimit_args.push_back(root);
}

void UserProg::setCPUTime(unsigned cputime) {
	_runlimit_args.push_back("-c");
	_runlimit_args.push_back(to_string(cputime));
}

void UserProg::setRealTime(unsigned realtime) {
	_runlimit_args.push_back("-t");
	_runlimit_args.push_back(to_string(realtime));
}

void UserProg::setMemLimit(unsigned bytes) {
	_runlimit_args.push_back("-m");
	_runlimit_args.push_back(to_string(bytes));
}

void UserProg::setFileLimit(unsigned bytes) {
	_runlimit_args.push_back("-f");
	_runlimit_args.push_back(to_string(bytes));
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
	_runlimit_args.push_back("-n");
	_runlimit_args.push_back(to_string(nproc));
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

int UserProg::exec(int fd_in, int fd_out, int fd_err, int fd_run, const string &evaluator) {
	if (chdir(_work_dir.c_str()) < 0) {
		lerror("chdir");
		_exit(-1);
	}

	list<string> prog_argv = getProgramArgv();
	list<string> prog_env = getProgramEnv();

	char ** argv = new char*[_runlimit_args.size() + 2 * prog_env.size() + prog_argv.size() + 4];
	char ** ptr = argv;

	if (evaluator != "")
		*ptr++ = strdup(evaluator.c_str());
	*ptr++ = strdup(Config::getConfig()["marker"]["runlimit"].c_str());

	list<string>::iterator i;
	for(i = _runlimit_args.begin(); i != _runlimit_args.end(); ++i)
		*ptr++ = strdup(i->c_str());
	for(i = prog_env.begin(); i != prog_env.end(); ++i) {
		*ptr++ = strdup("-e");
		*ptr++ = strdup(i->c_str());
	}

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

	execv(*argv, argv);

	lerror("execv");
	_exit(-1);
}

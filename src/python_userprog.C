/**
 * Copyright (c) 2009 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */

#include "userprog.h"
#include "acmconfig.h"
#include "logger.h"
#include "buffer.h"
#include "misc.h"

#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

using namespace std;

class Python_UserProg : public UserProg {
private:
	string _progname;

public:
	Python_UserProg();
	virtual ~Python_UserProg();
protected:
	virtual list<string> getProgramArgv();
	virtual list<string> getProgramEnv();
public:
	virtual string sourceFilename(const Buffer&);
	virtual bool compile(string infile, string compiler_log, string outdir);
};

Python_UserProg::Python_UserProg() {
	_progname = "userprog.py";
}

Python_UserProg::~Python_UserProg() {
	/* Nothing required */
}

list<string> Python_UserProg::getProgramArgv() {
	list<string> argv;
	argv.push_back("./userprog");
	return argv;
}

list<string> Python_UserProg::getProgramEnv() {
	list<string> env;
	env.push_back("LD_LIBRARY_PATH=/");
	return env;
}

string Python_UserProg::sourceFilename(const Buffer&) {
	return _progname;
}

bool Python_UserProg::compile(string infile, string compiler_log, string outdir)
{
	list<string> argv;

	string interpreter = Config::getConfig()["python"]["interpreter"];
	if (interpreter == "") {
		log(LOG_INFO, "[python][interpreter] not set, defaulting to /usr/bin/python3");
		interpreter = "/usr/bin/python3";
	}
	argv.push_back(interpreter);
	string compiler = Config::getConfig()["python"]["compiler"];
	if (compiler == "") {
		log(LOG_INFO, "[python][compiler] not set, defaulting to %s/python_compile.py", BINDIR);
		compiler = BINDIR "/python_compile.py";
	}
	argv.push_back(compiler);
	argv.push_back("--");
	argv.push_back(infile);
	argv.push_back(outdir);
	return execcompiler(argv, compiler_log) == 0;
}

UserProg* Python_Functor() {
	return new Python_UserProg();
}

static __attribute__((constructor)) void init() {
	UserProg::registerLanguage("Python", Python_Functor);
}

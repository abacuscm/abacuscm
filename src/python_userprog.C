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

#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

using namespace std;

class Python_UserProg : public UserProg {
private:
	string _progname;
	string _dir;	

public:
	Python_UserProg();
	virtual ~Python_UserProg();
protected:
	virtual list<string> getProgramArgv();
public:
	virtual void setRootDir(string root);
	virtual string sourceFilename(const Buffer&);
	virtual bool compile(string infile, string compiler_log, string outdir);
};

Python_UserProg::Python_UserProg() {
	_progname = "userprog.py";
}

Python_UserProg::~Python_UserProg() {
	/* Nothing required */
}

void Python_UserProg::setRootDir(string root) {
	_dir = root;
}

list<string> Python_UserProg::getProgramArgv() {
	list<string> argv;
	string interpreter = Config::getConfig()["python"]["interpreter"];
	if (interpreter == "") {
		log(LOG_INFO, "[python][interpreter] not set, defaulting to /usr/bin/python");
		interpreter = "/usr/bin/python";
	}
	argv.push_back(interpreter);
	
	string progname;
	if (_dir != "")
		progname = _dir + "/" + _progname;
	else
		progname = _progname;
	argv.push_back(progname);
	return argv;
}

string Python_UserProg::sourceFilename(const Buffer&) {
	return _progname;
}

bool Python_UserProg::compile(string infile, string compiler_log, string outdir)
{
	list<string> argv;

	argv.push_back("/bin/cp");
	argv.push_back("--");
	argv.push_back(infile);
	argv.push_back(outdir + "/" + _progname);
	return execcompiler(argv, compiler_log) == 0;
}

UserProg* Python_Functor() {
	return new Python_UserProg();
}

static __attribute__((constructor)) void init() {
	UserProg::registerLanguage("Python", Python_Functor);
}

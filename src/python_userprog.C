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

enum PythonVersion {
	PYTHON2x = 2, // Python 2.x
	PYTHON3x = 3  // Python 3.x
};

/** Get the internal name of a version of Python.
 *  Python 2.x is called "python" for backwards compatibility.
 *  Python 3.x is called "python3".
 */
string version_codename(PythonVersion v) {
	switch (v) {
    case PYTHON2x:
        return "python";
    case PYTHON3x:
        return "python3";
    default:
        log(LOG_ERR, "Invalid value for PythonVersion in version_codename: %d. Defaulting to 2.", (int)v);
        return "python";
	}
}

/** Get the source extension of a version of Python.
 */
string version_extension(PythonVersion v) {
	switch (v) {
    case PYTHON2x:
        return ".py";
    case PYTHON3x:
        return ".py3";
    default:
        log(LOG_ERR, "Invalid value for PythonVersion in version_extension: %d. Defaulting to 2.", (int)v);
        return ".py";
	}
}

class Python_UserProg : public UserProg {
private:
	string _progname;
	string _dir;
	PythonVersion _version;

public:
	Python_UserProg(PythonVersion v);
	virtual ~Python_UserProg();
protected:
	virtual list<string> getProgramArgv();
public:
	virtual void setRootDir(string root);
	virtual string sourceFilename(const Buffer&);
	virtual bool compile(string infile, string compiler_log, string outdir);
};

Python_UserProg::Python_UserProg(PythonVersion v) : _version(v) {
	_progname = "userprog" + version_extension(_version);
}

Python_UserProg::~Python_UserProg() {
	/* Nothing required */
}

void Python_UserProg::setRootDir(string root) {
	_dir = root;
}

list<string> Python_UserProg::getProgramArgv() {
	list<string> argv;
	string version = version_codename(_version);
	string interpreter = Config::getConfig()[version]["interpreter"];
	if (interpreter == "") {
		log(LOG_INFO, "[%s][interpreter] not set, defaulting to /usr/bin/%s", version.c_str(), version.c_str());
		interpreter = "/usr/bin/" + version;
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
	return new Python_UserProg(PYTHON2x);
}

UserProg* Python3_Functor() {
	return new Python_UserProg(PYTHON3x);
}

static __attribute__((constructor)) void init() {
	UserProg::registerLanguage("Python 2.x", Python_Functor);
	UserProg::registerLanguage("Python 3.x", Python3_Functor);
}

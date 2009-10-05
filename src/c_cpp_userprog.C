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
#include "acmconfig.h"
#include "logger.h"

using namespace std;

class C_CPP_UserProg : public UserProg{
private:
	string _language;
	string _progname;

	string compiler();
public:
	C_CPP_UserProg(string language);
	virtual ~C_CPP_UserProg();

protected:
	virtual list<string> getProgramArgv();
public:
	virtual string sourceFilename(const Buffer&);
	virtual bool compile(string infile, string compiler_log, string outdir);
};

C_CPP_UserProg::C_CPP_UserProg(string language) {
	_language = language;
}

C_CPP_UserProg::~C_CPP_UserProg() {
}

list<string> C_CPP_UserProg::getProgramArgv() {
	list<string> argv;
	argv.push_back(_progname);
	return argv;
}

string C_CPP_UserProg::sourceFilename(const Buffer&) {
	_progname = "userprog";
	if(_language == "c++")
		return _progname + ".cxx";
	else if(_language == "c")
		return _progname + ".c";
	else {
		log(LOG_DEBUG, "Unknown language '%s' in C_CPP_UserProg::sourceFilename(), defaulting to C++", _language.c_str());
		return _progname + ".cxx";
	}
}

bool C_CPP_UserProg::compile(string infile, string compiler_log, string outdir) {
	list<string> argv;
	argv.push_back(compiler());
	argv.push_back("-lm");
	argv.push_back("-static");
	argv.push_back("-s");
	argv.push_back("-O2");
	argv.push_back("-o");
	argv.push_back(outdir + "/" + _progname);
	argv.push_back(infile);
	return execcompiler(argv, compiler_log) == 0;
}

string C_CPP_UserProg::compiler() {
	string compiler = Config::getConfig()["compilers"][_language];

	if(compiler == "") {
		log(LOG_INFO, "Compiler for '%s' not specified!", _language.c_str());
		if(_language == "c")
			compiler = "/usr/bin/gcc";
		else if(_language == "c++")
			compiler = "/usr/bin/g++";
		else {
			log(LOG_DEBUG, "Only c and c++ has proper defaults, defaulting to g++");
			compiler = "/usr/bin/g++";
		}
	}
	return compiler;
}

UserProg* C_Functor() {
	return new C_CPP_UserProg("c");
}

UserProg* CPP_Functor() {
	return new C_CPP_UserProg("c++");
}

static __attribute__((constructor)) void init() {
	UserProg::registerLanguage("C", C_Functor);
	UserProg::registerLanguage("C++", CPP_Functor);
}

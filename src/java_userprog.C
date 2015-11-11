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
#include "buffer.h"
#include "misc.h"

#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>

using namespace std;

class Java_UserProg : public UserProg{
private:
	string _classname;
	string _cp_dir;
	unsigned _memlimit;
public:
	Java_UserProg();
	virtual ~Java_UserProg();

protected:
	virtual list<string> getProgramArgv();
public:
	virtual void setRootDir(string root);
	virtual void setMemLimit(unsigned memlimit);
	virtual void setMaxProcs(unsigned nproc);
	virtual string sourceFilename(const Buffer&);
	virtual bool compile(string infile, string compiler_log, string outdir);
};

Java_UserProg::Java_UserProg() {
	_memlimit = 0;
}

Java_UserProg::~Java_UserProg() {
}

list<string> Java_UserProg::getProgramArgv() {
	list<string> argv;
	string runtime = Config::getConfig()["java"]["runtime"];
	if(runtime == "") {
		log(LOG_INFO, "[java][runtime] not set, defaulting to /usr/bin/java");
		runtime = "/usr/bin/java";
	}

	argv.push_back(runtime);
	argv.push_back("-cp");
	argv.push_back(_cp_dir);
	if(_memlimit) {
		string memlimit = to_string(_memlimit);

		argv.push_back("-Xms" + memlimit);
		argv.push_back("-Xmx" + memlimit);
	}
	argv.push_back("-Djava.security.manager");
	argv.push_back("-Djava.security.policy==" + _cp_dir + "/java.policy");

	argv.push_back(_classname);
	return argv;
}

void Java_UserProg::setRootDir(string root) {
	_cp_dir = root;
}

void Java_UserProg::setMemLimit(unsigned limit) {
	_memlimit = limit;
}

void Java_UserProg::setMaxProcs(unsigned) {
	// TODO - ?!?
}

string Java_UserProg::sourceFilename(const Buffer& src) {
	_classname = "";

	int i;
	string sourcename = "";
	regex_t reg_pkg;
	if((i = regcomp(&reg_pkg, "(^|[[:space:]])package[[:space:]]+([A-Za-z_$][A-Za-z0-9_$]*(\\.[A-Za-z_][A-Za-z0-9_$]*)*)[[:space:]]*;", REG_EXTENDED)) != 0) {
		char err[1024];
		regerror(i, &reg_pkg, err, 1024);
		log(LOG_CRIT, "Error compiling java package name regex: %s", err);
		return "";
	}

	regex_t reg_cls;
	if((i = regcomp(&reg_cls, "(^|[[:space:]])public[[:space:]]+class[[:space:]]+([A-Za-z_$][A-Za-z0-9_$]*)[[:space:]{]", REG_EXTENDED)) != 0) {
		char err[1024];
		regerror(i, &reg_cls, err, 1024);
		log(LOG_CRIT, "Error compiling java class name regex: %s", err);
		return "";
	}

	regmatch_t pkg_match[4];
	regmatch_t cls_match[3];


	char* source = strndup((char*)src.data(), src.size());
	if(regexec(&reg_pkg, source, 4, pkg_match, 0) == 0) {
		char *tmp = strndup(source + pkg_match[2].rm_so, pkg_match[2].rm_eo - pkg_match[2].rm_so);
		_classname = tmp;
		_classname += ".";
		free(tmp);
	}
	if(regexec(&reg_cls, source, 3, cls_match, 0) == 0) {
		char *tmp = strndup(source + cls_match[2].rm_so, cls_match[2].rm_eo - cls_match[2].rm_so);
		sourcename = tmp;
		sourcename += ".java";
		_classname += tmp;
		free(tmp);
	}
	free(source);

	regfree(&reg_pkg);
	regfree(&reg_cls);

	log(LOG_DEBUG, "Using '%s' as classname and '%s' as source name", _classname.c_str(), sourcename.c_str());
	return sourcename;
}

bool Java_UserProg::compile(string infile, string compiler_log, string outdir) {
	string compiler = Config::getConfig()["java"]["compiler"];
	if(compiler == "") {
		log(LOG_INFO, "[java][compiler] not set, defaulting to /usr/bin/javac");
		compiler = "/usr/bin/javac";
	}

	string policy = Config::getConfig()["java"]["policy"];
	if(policy == "") {
		log(LOG_WARNING, "[java][policy] not set, defaulting to conf/java.policy");
		policy = "conf/java.policy";
	}
	ifstream policy_in(policy.c_str());
	if (!policy_in)
	{
		ofstream err(compiler_log.c_str());
		err << "Could not read policy file " << policy << ".\n";
		err << "Please contact a judge.\n";
		return false;
	}
	string policy_out_fname = outdir + "/java.policy";
	ofstream policy_out(policy_out_fname.c_str());
	if (!policy_out)
	{
		ofstream err(compiler_log.c_str());
		err << "Could not write policy file " << policy_out_fname << "\n";
		err << "Please contact a judge.\n";
		return false;
	}
	policy_out << policy_in.rdbuf();
	policy_in.close();
	policy_out.close();
	if (!policy_out)
	{
		ofstream err(compiler_log.c_str());
		err << "Error copying " << policy << " to " << policy_out_fname << "\n";
		err << "Please contact a judge.\n";
		return false;
	}

	list<string> argv;
	argv.push_back(compiler);
	argv.push_back("-d");
	argv.push_back(outdir);
	argv.push_back(infile);
	return execcompiler(argv, compiler_log) == 0;
}

UserProg* Java_Functor() {
	return new Java_UserProg();
}

static __attribute__((constructor)) void init() {
	UserProg::registerLanguage("Java", Java_Functor);
}

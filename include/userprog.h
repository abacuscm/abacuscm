/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __USERPROG_H__
#define __USERPROG_H__

#include <string>
#include <list>
#include <map>

class UserProg;
typedef UserProg* (*UserProgFunctor)();

class Buffer;

class UserProg {
private:
	static std::map<std::string, UserProgFunctor> _functors;

	std::list<std::string> _runlimit_args;
protected:
	/**
	 * should give the argv[] vector, argv[0] has to be
	 * the program name, relative to root dir.  If you
	 * override setRootDir() this may be a path to an
	 * interpreter.
	 */
	virtual std::list<std::string> getProgramArgv() = 0;

    int execcompiler(std::list<std::string> compiler_argv, std::string compiler_log);

	/**
	 * Will be called after the user program has been successfully compiled.
	 */
	void mark_compiled();
public:
	UserProg();
	virtual ~UserProg();

	/**
	 * Produce an executable.  In the case you scripts/interpreted languages
	 * you should either provide alternative sandboxing for the features from
	 * below that will break, or you need to put the entire interpretation
	 * environment _inside_ this directory.  See the JavaUserProg for an
	 * example.
	 */
    virtual bool compile(std::string infile, std::string compiler_log, std::string outdir) = 0;

	/**
	 * Since some compilers might require specific filenames this
	 * function provides the ability to specify a custom filename,
	 * the source is passed in-memory as well.  The default simply
	 * returns 'source'.  The name should not contain any slashes.
	 */
	virtual std::string sourceFilename(const Buffer&);

	virtual void setRootDir(std::string root); // will always be the same as outdir.
	virtual void setMemLimit(unsigned bytes);
	virtual void setFileLimit(unsigned bytes);
	virtual void setCPUTime(unsigned msecs);
	virtual void setRealTime(unsigned msecs);
	virtual void setRuntimeUser(std::string uname);
	virtual void setRuntimeGroup(std::string gname);
	virtual void setMaxProcs(unsigned nproc);

	/**
	 * fd_{in,out,err} == The fds to use as stdin, stdout and stderr.
	 *
	 * The process should _not_ fork, the call MUST NOT return.
	 */
	int exec(int fd_in, int fd_out, int fd_err, int fd_run) __attribute__((noreturn));

	/**
	 * Register a language.
	 */
	static void registerLanguage(std::string lang, UserProgFunctor func);

	static UserProg* createUserProg(std::string lang);
};

#endif

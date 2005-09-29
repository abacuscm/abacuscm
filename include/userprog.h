#ifndef __USERPROG_H__
#define __USERPROG_H__

#include <string>
#include <list>

class UserProg;
typedef UserProg* (*UserProgFunctor)();

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
	std::list<std::string> getProgramArgv() = 0;
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
	virtual bool compile(std::string infile, std::string outdir) = 0;

	virtual setRootDir(std::string root); // will always be the same as outdir.
	virtual setMemLimit(unsigned bytes);
	virtual setCPUTime(unsigned msecs);
	virtual setRealTime(unsigned msecs);

	/**
	 * in == input to be given.
	 * out == empty Buffer in where to store the output of program.
	 * run == capture stderr for runlimit feedback.
	 */
	void run(const Buffer&in, Buffer& out, Buffer& run);

	/**
	 * Register a language.
	 */
	static void registerLanguage(std::string lang, UserProgFunctor func);

	static UserProgFunctor* createUserProg(std::string lang);
};

#endif

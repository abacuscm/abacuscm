#ifndef __COMPILEDPROBLEMMARKER_H__
#define __COMPILEDPROBLEMMARKER_H__

#include "problemmarker.h"

#include <string>

class UserProg;
class Buffer;

class CompiledProblemMarker : public ProblemMarker {
private:
	UserProg *_uprog;
	int _cntr;
protected:
	/**
	 * In the simplest of cases you can call this to supply an input file,
	 * spec where the output file and runlimit files goes.
	 */
	int run(const char* infile, const char* outfile, const char* runfile);

	/**
	 * If you prefer, you can supply the Input Buffer and receive back an
	 * output and runlimit Buffer.  The data will currently still be dumped
	 * to disk first.  The filenames are {stdin,stdout,stderr}.${i} where i
	 * is a counter, starting at 1 and incremented with every call to _this_
	 * variation of run.
	 */
	int run(const Buffer& in, Buffer& out, Buffer& run);

	/**
	 * can be used to arbitrarily connect fds as you need.  In an interactive
	 * problem for example you can run the "official" program using a seperate
	 * UserProgram instance whilst using this version to execute the actual
	 * user program.
	 */
	int run(int fd_in, int fd_out, int fd_run) __attribute__((noreturn));

	/**
	 * You need to override this to receive control after the user program has been compiled.
	 */
	virtual void mark_compiled() = 0;
public:
	CompiledProblemMarker();
	virtual ~CompiledProblemMarker();

	void mark();
};

#endif

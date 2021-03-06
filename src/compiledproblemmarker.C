/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "compiledproblemmarker.h"
#include "userprog.h"
#include "logger.h"
#include "buffer.h"
#include "acmconfig.h"
#include "misc.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <unistd.h>

using namespace std;

CompiledProblemMarker::CompiledProblemMarker() {
	_cntr = 0;
	_uprog = NULL;
}

CompiledProblemMarker::~CompiledProblemMarker() {
	if(_uprog)
		delete _uprog;
}

void CompiledProblemMarker::mark() {
	Config &config = Config::getConfig();
	const Buffer& code = submission();
	string lang = language();

	if(_uprog)
		delete _uprog;
	_uprog = UserProg::createUserProg(lang);
	if(!_uprog) {
		log(LOG_ERR, "Unable to find an appropriate compiler/test harness for language '%s'", lang.c_str());
		return;
	}

	string wdir = workdir();
	if(wdir == "")
		return;

	string fname = _uprog->sourceFilename(code);
	if(fname == "") {
		string compiler_log_filename = wdir + "/compiler_log";
		ofstream compiler_log(compiler_log_filename.c_str());
		compiler_log <<
			"Unable to automatically determine class name.\n"
			"Make sure that your main class is the first class in the source file,\n"
			"and that you have declared it as 'public class MyClass {'. Pay attention\n"
			"to case and whitespace; in particular there should be only whitespace\n"
			"between the name of your class and the opening {, and the keywords 'public'\n"
			"and 'class' should all be in lowercase. The class name must also be a valid\n"
			"Java identifier and contain only ASCII characters A-Z, a-z, 0-9, _.\n";
		compiler_log.close();
		addResultFile("Compilation log", compiler_log_filename, 16384);
		log(LOG_ERR, "Failed to determine class name for user program.");
		setResult(COMPILE_FAILED);
		return;
	}

	fname = wdir + "/" + fname;

	log(LOG_INFO, "Using source file '%s'", fname.c_str());
	ofstream source(fname.c_str());
	source << code;
	source.close();

	string jaildir = wdir + "/jail";

	if(mkdir(jaildir.c_str(), 0711) < 0) {
		lerror("mkdir");
	} else {
		string compiler_log = wdir + "/compiler_log";
		log(LOG_INFO, "Compiling user program ...");
		if(!_uprog->compile(fname, compiler_log, jaildir)) {
			setResult(COMPILE_FAILED);
			addResultFile("Compilation log",  compiler_log, 16384);
			log(LOG_ERR, "User program failed to compile.");
			return;
		}

		uint32_t timelimit;
		if (!from_string(attrib("time_limit"), timelimit) || timelimit == 0) {
			timelimit = 120;
			log(LOG_WARNING, "timelimit==0 is invalid, defaulting to %us.", (unsigned)timelimit);
		}
		else
			log(LOG_INFO, "Attribute time limit = %u", (unsigned)timelimit);

		uint32_t memorylimit;
		if (!from_string(attrib("memory_limit"), memorylimit) || memorylimit == 0) {
			// If you change this, remember to update the contestant manual.
			memorylimit = 256;
			log(LOG_WARNING, "memorylimit==0 is invalid, defaulting to %u MB.", (unsigned)memorylimit);
		}
		else
			log(LOG_INFO, "Attribute memory limit = %u", (unsigned)memorylimit);

		timelimit *= 1000; // convert to milli-seconds.
		memorylimit *= 1024 * 1024; // convert to bytes

		log(LOG_INFO, "User program compiled, preparing execution environment.");
		_uprog->setMemLimit(memorylimit);
		_uprog->setFileLimit(16 * 1024 * 1024);
		_uprog->setWorkDir(wdir);
		_uprog->setRootDir(jaildir);
		_uprog->setCPUTime(timelimit);
		_uprog->setRealTime(timelimit * 8);
		_uprog->setMaxProcs(1);

		if(config["marker"]["user"] != "")
			_uprog->setRuntimeUser(config["marker"]["user"]);
		if(config["marker"]["group"] != "")
			_uprog->setRuntimeGroup(config["marker"]["group"]);

		mark_compiled();
	}

	log(LOG_DEBUG, "All done with the marking process!");
}

int CompiledProblemMarker::run(const char* infile, const char* outfile, const char* errfile,  const char* runfile, const char *evaluator) {
	int fd_in = open(infile, O_RDONLY);
	if(fd_in < 0) {
		lerror("open(infile)");
		return -1;
	}

	int fd_out = open(outfile, O_WRONLY | O_CREAT, 0600);
	if(fd_out < 0) {
		lerror("open(outfile)");
		return -1;
	}

	int fd_err = open(errfile, O_WRONLY | O_CREAT, 0600);
	if(fd_err < 0) {
		lerror("open(errfile)");
		return -1;
	}

	int fd_run = open(runfile, O_WRONLY | O_CREAT, 0600);
	if(fd_run < 0) {
		lerror("open(runfile)");
		return -1;
	}

	pid_t pid = fork();
	if(pid < 0) {
		lerror("fork");
		return -1;
	} else if(pid == 0) {
		_uprog->exec(fd_in, fd_out, fd_err, fd_run, evaluator);
	} else {
		close(fd_in);
		close(fd_out);
		close(fd_err);
		close(fd_run);

		int status;

		while(waitpid(pid, &status, 0) < 0)
			lerror("waitpid");

		log(LOG_DEBUG, "User program terminated with excode=%d", status);
		return status;
	}
}

int CompiledProblemMarker::run(const Buffer& in, Buffer& out, Buffer& err) {
	string cntrstr = to_string(++_cntr);

	ofstream file_stdin((workdir() + "/stdin." + cntrstr).c_str());
	file_stdin << in;
	file_stdin.close();

	int res = run(
			(workdir() + "/stdin." + cntrstr).c_str(),
			(workdir() + "/stdout." + cntrstr).c_str(),
			(workdir() + "/stderr." + cntrstr).c_str(),
			(workdir() + "/runinfo." + cntrstr).c_str());

	struct stat statdata;
	int out_file = open((workdir() + "/stdout." + cntrstr).c_str(), O_RDONLY);
	if(out_file > 0) {
		if(fstat(out_file, &statdata) < 0) {
			lerror("fstat");
		} else {
			void* ptr = mmap(NULL, statdata.st_size, PROT_READ, MAP_PRIVATE, out_file, 0);
			if(!ptr) {
				lerror("mmap");
			} else {
				out.appendData(ptr, statdata.st_size);
				munmap(ptr, statdata.st_size);
			}
		}
		close(out_file);
	}

	int err_file = open((workdir() + "/stderr." + cntrstr).c_str(), O_RDONLY);
	if(err_file > 0) {
		if(fstat(err_file, &statdata) < 0) {
			lerror("fstat");
		} else {
			void* ptr = mmap(NULL, statdata.st_size, PROT_READ, MAP_PRIVATE, err_file, 0);
			if(!ptr) {
				lerror("mmap");
			} else {
				err.appendData(ptr, statdata.st_size);
				munmap(ptr, statdata.st_size);
			}
		}
		close(err_file);
	}

	return res;
}

void CompiledProblemMarker::parse_checker(istream &in, RunResult &status, string &info) {
	string line;
	bool found = false;
	status = OTHER;
	info = "";
	while (getline(in, line)) {
		istringstream toks(line);
		string tok, status_str;
		if (toks >> tok && tok == "STATUS") {
			toks >> status_str >> ws;
			if (!toks) {
				
				status = OTHER;
				info = "Invalid formatting on STATUS line";
				log(LOG_ERR, "Invalid formatting on STATUS line: '%s'", line.c_str());
				return;
			}
			getline(toks, info);

			bool matched = false;
			for (int i = 0; i <= OTHER; i++)
				if (status_str == runCodes[i])
				{
					status = (RunResult) i;
					matched = true;
					break;
				}
			if (!matched)
			{
				status = OTHER;
				info = "Unrecognised status code";
				log(LOG_ERR, "Unrecognised status code %s", status_str.c_str());
				return;
			}
			if (found) {
				status = OTHER;
				info = "Multiple STATUS lines found";
				log(LOG_ERR, "Multiple STATUS lines found");
				return;
			}
			found = true;
		}
	}
	if (!found) {
		status = OTHER;
		info = "Did not find STATUS line";
		log(LOG_ERR, "Did not find STATUS line");
	}
}

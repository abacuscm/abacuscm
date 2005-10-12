#include "compiledproblemmarker.h"
#include "userprog.h"
#include "logger.h"
#include "buffer.h"
#include "config.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
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
		setResult(COMPILE_FAILED);
		return;
	};

	fname = wdir + "/" + fname;
	
	log(LOG_INFO, "Using source file '%s'", fname.c_str());
	ofstream source(fname.c_str());
	source << code;
	source.close();

	string jaildir = wdir + "/jail";
	
	if(mkdir(jaildir.c_str(), 0711) < 0) {
		lerror("mkdir");
	} else {
		log(LOG_INFO, "Compiling user program ...");
		if(!_uprog->compile(fname, jaildir)) {
			setResult(COMPILE_FAILED);
			log(LOG_ERR, "User program failed to compile.");
			return;
		}

		uint32_t timelimit = strtoll(attrib("time_limit").c_str(), NULL, 0);
		log(LOG_INFO, "Attribute time limit = %u", (unsigned)timelimit);
		
		if(!timelimit) {
			timelimit = 120;
			log(LOG_WARNING, "timelimit==0 is invalid, defaulting to %us.", (unsigned)timelimit);
		}

		timelimit *= 1000; // convert to milli-seconds.
		
		log(LOG_INFO, "User program compiled, preparing execution environment.");
		_uprog->setMemLimit(64 * 1024 * 1024);
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

int CompiledProblemMarker::run(const char* infile, const char* outfile, const char* errfile,  const char* runfile) {
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
		_uprog->exec(fd_in, fd_out, fd_err, fd_run);
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
	ostringstream cntrstrm;
	cntrstrm << ++_cntr;

	ofstream file_stdin((workdir() + "/stdin." + cntrstrm.str()).c_str());
	file_stdin << in;
	file_stdin.close();

	int res = run(
			(workdir() + "/stdin." + cntrstrm.str()).c_str(),
			(workdir() + "/stdout." + cntrstrm.str()).c_str(),
			(workdir() + "/stderr." + cntrstrm.str()).c_str(),
			(workdir() + "/runinfo." + cntrstrm.str()).c_str());

	struct stat statdata;
	int out_file = open((workdir() + "/stdout." + cntrstrm.str()).c_str(), O_RDONLY);
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

	int err_file = open((workdir() + "/stderr." + cntrstrm.str()).c_str(), O_RDONLY);
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

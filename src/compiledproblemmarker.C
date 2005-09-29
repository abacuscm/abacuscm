#include "compiledproblemmarker.h"
#include "userprog.h"

void CompiledProblemMarker::mark(const Buffer&, const std::string& lang) {
	UserProg* uprog = UserProg::createUserProg(lang);
}

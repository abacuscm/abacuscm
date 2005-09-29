#ifndef __COMPILEDPROBLEMMARKER_H__
#define __COMPILEDPROBLEMMARKER_H__

#include "problemmarker.h"

class CompiledProblemMarker : public ProblemMarker {
public:
	void mark(const Buffer&, const std::string& lang);
};

#endif

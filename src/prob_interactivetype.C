/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "compiledproblemtype.h"

#include <algorithm>
#include <vector>
#include <string>

class InteractiveProblemType : public CompiledProblemType {
protected:
	virtual const std::string& problemType() const;
	virtual std::vector<std::string> getAttributeList() const;
public:
	InteractiveProblemType() {}
	virtual ~InteractiveProblemType() {}
};

const std::string& InteractiveProblemType::problemType() const {
	static std::string _type = "interactiveprob";
	return _type;
}

std::vector<std::string> InteractiveProblemType::getAttributeList() const {
	std::vector<std::string> result = CompiledProblemType::getAttributeList();
	result.push_back("evaluator F");
	result.push_back("time_limit I");
	result.push_back("memory_limit I");
	return result;
}

extern "C" void abacuscm_mod_init() {
	static InteractiveProblemType _interactive_prob_type;
	ProblemType::registerProblemType(&_interactive_prob_type);
}

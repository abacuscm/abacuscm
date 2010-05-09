/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id: prob_testcasedriventype.C 554 2009-10-05 12:06:39Z jkroon $
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
	return result;
}

static void init() __attribute__((constructor));
static void init() {
	static InteractiveProblemType _interactive_prob_type;
	ProblemType::registerProblemType(&_interactive_prob_type);
}

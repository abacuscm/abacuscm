/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "compiledproblemtype.h"

#include <algorithm>

using namespace std;

// TC == TestCase
class TCProblemType : public CompiledProblemType {
private:
	vector<string> _attr_list;
protected:
	virtual const string& problemType() const;
	virtual vector<string> getAttributeList() const;
public:
	TCProblemType();
	virtual ~TCProblemType();
};

TCProblemType::TCProblemType() {
	_attr_list.push_back("testcase (input F, output F)");
	_attr_list.push_back("ignore_whitespace {Yes,No}");
	_attr_list.push_back("time_limit I");
}

TCProblemType::~TCProblemType() {
}

const string& TCProblemType::problemType() const {
	static string _type = "tcprob";
	return _type;
}

vector<string> TCProblemType::getAttributeList() const {
	vector<string> result = CompiledProblemType::getAttributeList();
	for(vector<string>::const_iterator i = _attr_list.begin();
			i != _attr_list.end(); ++i) {
		result.push_back(*i);
	}
//	copy(_attr_list.begin(), _attr_list.end(), result.end());
	return result;
}

TCProblemType _tc_prob_type;

static void init() __attribute__((constructor));
static void init() {
	ProblemType::registerProblemType(&_tc_prob_type);
}

/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "problemtype.h"
#include "logger.h"

#include <sstream>
#include <iterator>

using namespace std;

ProblemType::TypeMap ProblemType::_problem_type_map;

ProblemType::ProblemType() {
	_attr_list.push_back("shortname S");
	_attr_list.push_back("longname S");
	_attr_list.push_back("multi_submit {No,Yes}");
}

ProblemType::~ProblemType() {
}

vector<string> ProblemType::getAttributeList() const {
	return _attr_list;
}

void ProblemType::registerProblemType(const ProblemType *prob) {
	log(LOG_INFO, "Registering ProblemType '%s'", prob->problemType().c_str());
	_problem_type_map[prob->problemType()] = prob;
}

vector<string> ProblemType::getProblemTypes() {
	vector<string> prob_types;

	TypeMap::iterator i;
	for(i = _problem_type_map.begin(); i != _problem_type_map.end(); ++i)
		prob_types.push_back(i->first);

	return prob_types;
}

string ProblemType::getProblemDescription(const string& problemtype) {
	TypeMap::iterator i = _problem_type_map.find(problemtype);

	if(i == _problem_type_map.end())
		return "";

	ostringstream descript;

	vector<string> attrlist = i->second->getAttributeList();
	copy(attrlist.begin(), attrlist.end(), ostream_iterator<string>(descript, ", "));

	string desc_str = descript.str();
	return desc_str.substr(0, desc_str.length() - 2);
}

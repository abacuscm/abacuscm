/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __PROBLEMTYPE_H__
#define __PROBLEMTYPE_H__

#include <string>
#include <vector>
#include <map>

class ProblemType {
private:
	typedef std::map<std::string, const ProblemType*> TypeMap;
	static TypeMap _problem_type_map;

	std::vector<std::string> _attr_list;
protected:
	ProblemType();
	virtual ~ProblemType();

	virtual const std::string& problemType() const = 0;
	virtual std::vector<std::string> getAttributeList() const;
public:
	static void registerProblemType(const ProblemType *prob);
	static std::vector<std::string> getProblemTypes();
	static std::string getProblemDescription(const std::string& problemid);
};

#endif

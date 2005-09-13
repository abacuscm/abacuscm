#ifndef __PROBLEMTYPE_H__
#define __PROBLEMTYPE_H__

#include <string>
#include <vector>
#include <map>

class ProblemType {
private:
	typedef std::map<std::string, const ProblemType*> TypeMap;
	static TypeMap _problem_type_map;
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

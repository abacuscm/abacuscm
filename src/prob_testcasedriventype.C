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
}

TCProblemType::~TCProblemType() {
}

const string& TCProblemType::problemType() const {
	static string _type = "tcprob";
	return _type;
}

vector<string> TCProblemType::getAttributeList() const {
	vector<string> result = CompiledProblemType::getAttributeList();
	copy(_attr_list.begin(), _attr_list.end(), result.end());
	return result;
}

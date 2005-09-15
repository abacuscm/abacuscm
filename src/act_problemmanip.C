#include "problemtype.h"
#include "clientaction.h"
#include "clientconnection.h"
#include "logger.h"
#include "messageblock.h"

#include <map>
#include <string>
#include <algorithm>
#include <iterator>
#include <sstream>

using namespace std;

class ActSetProbAttrs : public ClientAction {
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
};

bool ActSetProbAttrs::int_process(ClientConnection *cc, MessageBlock *mb) {
	string prob_type = (*mb)["prob_type"];
	if(prob_type == "")
		return cc->sendError("You must specify prob_type for new problems");
	
	string attr_desc = ProblemType::getProblemDescription(prob_type);
	if(attr_desc == "")
		return cc->sendError("Invalid prob_type or internal server error");
	
//	map<string, string> attributes;
	// this crap should be rewritten to make use of regular expressions
	// for example 'a S' relates to ^a$ for an attribute name, any attributes
	// matching that is valid.  in the same way a (b S, c S) can translate
	// to ^a.(b|c)$, again, each one needs to be checked for type.  Thus
	// this strategy will only become really handy when we start allowing
	// that dreaded * operator where the above can probably be broken down
	// to two distinct regular expressions and then a map can map a regex
	// onto a basic type of (S|I|F) to validate that.  For now the implemented
	// strategy is likely going to be more efficient.

	vector<string> stack;

	size_t pos = 0;
	while(pos < attr_desc.length()) {
		// find attribute name
		size_t epos = attr_desc.find(' ', pos);
		if(epos == string::npos) {
			log(LOG_NOTICE, "Invalid attr_desc string '%s'", attr_desc.c_str());
			return cc->sendError("Internal Server Error");
		}

		string attr_name = attr_desc.substr(pos, epos - pos);
		pos = epos + 1;

		if(pos >= attr_desc.length()) {
			log(LOG_NOTICE, "Invalid attr_desc string '%s'", attr_desc.c_str());
			return cc->sendError("Internal Server Error");
		} else if(attr_desc[pos] == '(') {
			stack.push_back(attr_name);
			pos ++;
		} else { // terminal
			ostringstream attrname;
			copy(stack.begin(), stack.end(), ostream_iterator<string>(attrname, "."));
			string attr = attrname.str();
			log(LOG_DEBUG, "attr: '%s'", attr.c_str());
			attr += attr_name;
			
			if(!mb->hasAttribute(attr)) {
				return cc->sendError("Missing attribute " + attr);
			} else if(attr_desc[pos] == 'S') {
			} else if(attr_desc[pos] == 'I') {
			} else if(attr_desc[pos] == 'F') {
			} else {
				log(LOG_NOTICE, "Invalid attr_desc string '%s'", attr_desc.c_str());
				return cc->sendError("Internal Server Error");
			}

			pos++;
//			attributes[attr] = 
		}

		while(pos < attr_desc.length() && attr_desc[pos] == ')') {
			stack.pop_back();
			pos++;
		}

		if(pos < attr_desc.length() && attr_desc[pos] == ',')
			pos++;

		while(pos < attr_desc.length() && isspace(attr_desc[pos]))
			pos++;
	}

	return cc->reportSuccess();
}

ActSetProbAttrs _act_setprobattrs;

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "setprobattrs", &_act_setprobattrs);
}

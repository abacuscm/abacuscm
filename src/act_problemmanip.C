#include "problemtype.h"
#include "clientaction.h"
#include "clientconnection.h"
#include "logger.h"
#include "messageblock.h"
#include "message.h"
#include "message_type_ids.h"

#include <map>
#include <string>
#include <algorithm>
#include <iterator>
#include <sstream>

#include <sys/types.h>
#include <regex.h>

using namespace std;

class ActSetProbAttrs : public ClientAction {
private:
	regex_t _file_reg;
protected:
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb);
public:
	ActSetProbAttrs();
	~ActSetProbAttrs();
};

class ProbMessage : public Message {
private:
	struct FileAttrib {
		string name;
		uint32_t len;
		uint8_t *data;
	};

	typedef map<string, FileAttrib> FileMap;
	typedef map<string, string> StringMap;
	typedef map<string, int32_t> IntMap;

	uint32_t _prob_id;
	
	FileMap _files;
	StringMap _strings;
	IntMap _ints;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	ProbMessage();
	ProbMessage(uint32_t prob_id);

	virtual ~ProbMessage();

	bool addIntAttrib(const string& name, int32_t value);
	bool addStringAttrib(const string& name, const string& value);
	bool addFileAttrib(const string& name, const string& filename, const char* buffer, uint32_t len);
	bool keepFileAttrib(const string& name);

	virtual bool process() const;
	virtual uint16_t message_type_id() const { return TYPE_ID_PROBLEMUPDATE; };
};

ProbMessage::ProbMessage() {
	_prob_id = ~0U;
}

ProbMessage::ProbMessage(uint32_t prob_id) {
	_prob_id = prob_id;
}

ProbMessage::~ProbMessage() {
	FileMap::iterator i;
	for(i = _files.begin(); i != _files.end(); ++i)
		delete []i->second.data;
}

uint32_t ProbMessage::storageRequired() {
	uint32_t total = 0;

	IntMap::iterator i;
	for(i = _ints.begin(); i != _ints.end(); ++i)
		total += 1 + i->first.length() + 1 + sizeof(i->second); // 'I' + attr_name + '\0' + bin_value

	StringMap::iterator s;
	for(s = _strings.begin(); s != _strings.end(); ++s)
		total += 1 + s->first.length() + s->second.length() + 2; // 'S' + attr_name + '\0' + value + '\0'

	FileMap::iterator f;
	for(f = _files.begin(); f != _files.end(); ++f)
		total += 1 + f->first.length() + 1 + f->second.name.length() + 1
			+ sizeof(f->second.len) + f->second.len; // 'F' + attr_name + '\0' + fname + '\0' + bin_len + data

	return total;
}

uint32_t ProbMessage::store(uint8_t *buffer, uint32_t size) {
	uint32_t used = 0;

	IntMap::iterator i;
	for(i = _ints.begin(); i != _ints.end(); ++i) {
		*buffer++ = 'I';
		strcpy((char*)buffer, i->first.c_str());
		buffer += i->first.length() + 1;
		*(int32_t*)buffer = i->second;
		buffer += sizeof(int32_t);
		used += 1 + i->first.length() + 1 + sizeof(i->second);
	}
	
	StringMap::iterator s;
	for(s = _strings.begin(); s != _strings.end(); ++s) {
		*buffer++ = 'S';
		strcpy((char*)buffer, s->first.c_str());
		buffer += s->first.length() + 1;
		strcpy((char*)buffer, s->second.c_str());
		buffer += s->second.length() + 1;
		used += 1 + s->first.length() + s->second.length() + 2;
	}

	FileMap::iterator f;
	for(f = _files.begin(); f != _files.end(); ++f) {
		*buffer++ = 'F';
		strcpy((char*)buffer, f->first.c_str());
		buffer += f->first.length() + 1;
		strcpy((char*)buffer, f->second.name.c_str());
		buffer += f->second.name.length() + 1;
		*(int32_t*)buffer = f->second.len;
		buffer += sizeof(int32_t);
		memcpy(buffer, f->second.data, f->second.len);
		buffer += f->second.len;
		used += 1 + f->first.length() + 1 + f->second.name.length() + 1
			+ sizeof(f->second.len) + f->second.len;
	}

	if(used > size) {
		log(LOG_CRIT, "Abacus is likely to segfault, we've just encountered a buffer overflow in ProbMessage::store");
		return ~0U;
	}

	return used;
}

uint32_t ProbMessage::load(const uint8_t *buffer, uint32_t size) {
	NOT_IMPLEMENTED();
	return ~0U;
}

bool ProbMessage::addIntAttrib(const string& name, int32_t value) {
	_ints[name] = value;
	return true;
}

bool ProbMessage::addStringAttrib(const string& name, const string& value) {
	_strings[name] = value;
	return true;
}

bool ProbMessage::addFileAttrib(const string& name, const string& filename, const char* buffer, uint32_t len) {
	uint8_t *bfr = new uint8_t[len];
	memcpy(bfr, buffer, len);

	if(_files.find(name) != _files.end())
		delete []_files[name].data;

	_files[name].name = filename;
	_files[name].len = len;
	_files[name].data = bfr;

	return true;
}

bool ProbMessage::keepFileAttrib(const string& name) {
	if(_files.find(name) != _files.end())
		delete []_files[name].data;
	_files[name].data = NULL;
	_files[name].name = "";
	_files[name].len = 0;

	return true;
}

bool ProbMessage::process() const {
	NOT_IMPLEMENTED();
	return false;
}

ActSetProbAttrs::ActSetProbAttrs() {
	regcomp(&_file_reg, "^([0-9]{1,7}) ([0-9]{1,7}) ([-[:alnum:] _.]+)$", REG_EXTENDED);
}

ActSetProbAttrs::~ActSetProbAttrs() {
	regfree(&_file_reg);
}

#define act_error(x)	{ delete msg; cc->sendError(x); }
bool ActSetProbAttrs::int_process(ClientConnection *cc, MessageBlock *mb) {
	uint32_t prob_id = ~0U;
	string prob_type = (*mb)["prob_type"];
	if(prob_type == "")
		return cc->sendError("You must specify prob_type for new problems");
	
	string attr_desc = ProblemType::getProblemDescription(prob_type);
	if(attr_desc == "")
		return cc->sendError("Invalid prob_type or internal server error");
	
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

	ProbMessage *msg = new ProbMessage(prob_id);
	
	size_t pos = 0;
	while(pos < attr_desc.length()) {
		// find attribute name
		size_t epos = attr_desc.find(' ', pos);
		if(epos == string::npos) {
			log(LOG_NOTICE, "Invalid attr_desc string '%s' (Invalid attr_name)", attr_desc.c_str());
			act_error("Internal Server Error");
		}

		string attr_name = attr_desc.substr(pos, epos - pos);
		pos = epos + 1;

		if(pos >= attr_desc.length()) {
			log(LOG_NOTICE, "Invalid attr_desc string '%s' (missing type)", attr_desc.c_str());
			act_error("Internal Server Error");
		} else if(attr_desc[pos] == '(') {
			stack.push_back(attr_name);
			pos ++;
		} else { // terminal
			ostringstream attrname;
			copy(stack.begin(), stack.end(), ostream_iterator<string>(attrname, "."));
			string attr = attrname.str();
			attr += attr_name;
			log(LOG_DEBUG, "attr: '%s'", attr.c_str());
			
			if(!mb->hasAttribute(attr)) {
				act_error("Missing attribute " + attr);
			} else if(attr_desc[pos] == 'S') {
				msg->addStringAttrib(attr, (*mb)[attr]);
			} else if(attr_desc[pos] == 'I') {
				const char* i = (*mb)[attr].c_str();
				log(LOG_DEBUG, "Testing '%s' for being an int!", i);
				char *eptr;
				long val = strtol(i, &eptr, 0);
				if(*eptr)
					act_error("Value for " + attr + " is not a valid integer");
				msg->addIntAttrib(attr, val);
			} else if(attr_desc[pos] == 'F') {
				string file_attr_desc = (*mb)[attr];
				if(file_attr_desc == "-") {
					msg->keepFileAttrib(attr);
				} else {
					regmatch_t m[4];
					if(regexec(&_file_reg, file_attr_desc.c_str(), 4, m, 0))
						act_error("Invalid filename structure, filename attributes must match the structure 'int int filename'");

					// no need for error checking, the regex already took care of that.
					int start = strtol(file_attr_desc.substr(m[1].rm_so, m[1].rm_eo - m[1].rm_so).c_str(), NULL, 0);
					int size  = strtol(file_attr_desc.substr(m[2].rm_so, m[2].rm_eo - m[2].rm_so).c_str(), NULL, 0);
					string filename  = file_attr_desc.substr(m[3].rm_so, m[3].rm_eo - m[3].rm_so);

					if(start + size > mb->content_size())
						act_error("File limits for file '" + attr + "' out of bounds for passed data");

					msg->addFileAttrib(attr, filename, &mb->content()[start], size);
				}
			} else if(attr_desc[pos] == '{') {
				epos = attr_desc.find('}', pos);
				bool correct = false;
				while(++pos < epos && !correct) {
					size_t ncomma = attr_desc.find(',', pos);
					if(ncomma > epos || ncomma == string::npos)
						ncomma = epos;

					correct = attr_desc.substr(pos, ncomma - pos) == (*mb)[attr];
					pos = ncomma;
				}
				pos = epos;
				if(!correct)
					act_error("Invalid value for attribute " + attr);
				msg->addStringAttrib(attr, (*mb)[attr]);
			} else {
				log(LOG_NOTICE, "Invalid attr_desc string '%s' (Invalid type '%c' for '%s')", attr_desc.c_str(), attr_desc[pos], attr.c_str());
				act_error("Internal Server Error");
			}

			pos++;
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

	return triggerMessage(cc, msg);
}
#undef act_error

ActSetProbAttrs _act_setprobattrs;

static Message* create_prob_message() {
	return new ProbMessage();
}

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "setprobattrs", &_act_setprobattrs);
	Message::registerMessageFunctor(TYPE_ID_PROBLEMUPDATE, create_prob_message);
}

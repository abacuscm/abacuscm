#include "problemtype.h"
#include "clientaction.h"
#include "clientconnection.h"
#include "logger.h"
#include "messageblock.h"
#include "message.h"
#include "message_type_ids.h"
#include "server.h"
#include "dbcon.h"

#include <map>
#include <string>
#include <algorithm>
#include <iterator>
#include <sstream>

#include <sys/types.h>
#include <regex.h>
#include <time.h>

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
	uint32_t _update_time;

	string _type;
	
	FileMap _files;
	StringMap _strings;
	IntMap _ints;

protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	ProbMessage();
	ProbMessage(uint32_t prob_id, const string& type);

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

ProbMessage::ProbMessage(uint32_t prob_id, const string& type) {
	_prob_id = prob_id;
	if(!prob_id)
		_prob_id = Server::nextProblemId();
	_update_time = ::time(NULL);
	_type = type;
}

ProbMessage::~ProbMessage() {
	FileMap::iterator i;
	for(i = _files.begin(); i != _files.end(); ++i)
		delete []i->second.data;
}

uint32_t ProbMessage::storageRequired() {
	if(_prob_id == ~0U) {
		log(LOG_ERR, "Attempt to pack improper ProbMessage");
		return ~0U;
	}
	uint32_t total = sizeof(_prob_id) + sizeof(_update_time);
	total += _type.length() + 1;

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

	*(uint32_t*)buffer = _prob_id; buffer += sizeof(uint32_t);
	*(uint32_t*)buffer = _update_time; buffer += sizeof(uint32_t);
	used += sizeof(uint32_t) * 2;

	strcpy((char*)buffer, _type.c_str());
	used += _type.length() + 1;
	buffer += _type.length() + 1;

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
	const uint8_t* pos = buffer;
	if(size < 2 * sizeof(uint32_t))
		return ~0U;
	_prob_id = *(uint32_t*)pos; pos += sizeof(uint32_t);
	_update_time = *(uint32_t*)pos; pos += sizeof(uint32_t);
	size -= sizeof(uint32_t) * 2;

	if(!checkStringTerms(pos, size, 1)) {
		log(LOG_ERR, "We need to pull a string indicating the type of the problem, yet the buffer does not contain any appropriate null terminators");
		return ~0U;
	}
	_type = (char*)pos;
	pos += _type.length() + 1;
	size -= _type.length() + 1;

	while(size) {
		size -= 1;
		switch(*pos++) {
		case 'S':
			{
				if(!checkStringTerms(pos, size, 2)) {
					log(LOG_ERR, "Invalid blob data in ProbMessage::load, need two 0-terminators in buffer for type S");
					return ~0U;
				}
				string attr = (const char*)pos;
				pos += attr.length() + 1;
				string value = (const char*)pos;
				pos += value.length() + 1;
				size -= attr.length() + value.length() + 2;
				addStringAttrib(attr, value);
			}
			break;
		case 'I':
			{
				if(!checkStringTerms(pos, size - sizeof(int32_t), 1)) {
					log(LOG_ERR, "Invalid blob data in ProbMessage::load, need one 0-terminator in buffer for type I, plus another %d bytes", (int)sizeof(int32_t));
					return ~0U;
				}
				string attr = (const char*)pos;
				pos += attr.length() + 1;
				int32_t value = *(int32_t*)pos;
				pos += sizeof(int32_t);
				size -= attr.length() + 1 + sizeof(int32_t);
				addIntAttrib(attr, value);
			}
			break;
		case 'F':
			{
				if(!checkStringTerms(pos, size - sizeof(int32_t), 2)) {
					log(LOG_ERR, "Invalid blob data in ProbMessage::load, need one 2-terminator in buffer for type F, plus another %d bytes", (int)sizeof(int32_t));
					return ~0U;
				}
				string attr = (const char*)pos;
				pos += attr.length() + 1;
				string name = (const char*)pos;
				pos += name.length() + 1;
				uint32_t filesize = *(uint32_t*)pos;
				pos += sizeof(int32_t);
				size -= attr.length() + name.length() + 2 + sizeof(int32_t);
				if(name == "")
					keepFileAttrib(attr);
				else {
					if(size < filesize) {
						log(LOG_ERR, "Insufficient data in buffer for filecontents of '%s', expected %d bytes, only %d available.", attr.c_str(), filesize, size);
						return ~0U;
					}
					addFileAttrib(attr, name, (const char*)pos, filesize);
					pos += filesize;
					size -= filesize;
				}
			}
			break;
		default:
			log(LOG_ERR, "Unknown type '%c' in ProbMessage::load()", *--pos);
			return ~0U;
		}
	}
	return pos - buffer;
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
	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	if(_prob_id == ~0U) {
		log(LOG_ERR, "Trying to process an improper ProbMessage!");
		return false;
	}

	string ex_prob_type = db->getProblemType(_prob_id);
	if(ex_prob_type != "") {
		uint32_t lst_updated = db->getProblemUpdateTime(_prob_id);
		if(lst_updated > _update_time) {
			log(LOG_INFO, "Discarding udpate for problem_id=%u that is older than current version", _prob_id);
			db->release();
			return true;
		}
		AttributeList cur_attrs = db->getProblemAttributes(_prob_id);

		AttributeList::iterator a;
		for(a = cur_attrs.begin(); a != cur_attrs.end(); ++a) {
			if(_ints.find(a->first) == _ints.end() &&
					_strings.find(a->first) == _strings.end() &&
					_files.find(a->first) == _files.end())
				db->delProblemAttribute(_prob_id, a->first);
		}
	}

	if(ex_prob_type != _type && !db->setProblemType(_prob_id, _type)) {
		db->release();
		return false;
	}

	bool result = true;

	FileMap::const_iterator f;
	for(f = _files.begin(); f != _files.end(); ++f) {
		if(f->second.name != "")
			result &= db->setProblemAttribute(_prob_id, f->first,
					f->second.name, f->second.data, f->second.len);
	}
	
	IntMap::const_iterator i;
	for(i = _ints.begin(); i != _ints.end(); ++i) {
		result &= db->setProblemAttribute(_prob_id, i->first, i->second);
	}
	
	StringMap::const_iterator s;
	for(s = _strings.begin(); s != _strings.end(); ++s) {
		result &= db->setProblemAttribute(_prob_id, s->first, s->second);
	}

	result &= db->setProblemUpdateTime(_prob_id, _update_time);
	
	if(!result)
		log(LOG_CRIT, "Setting of at least one attribute for problem_id=%u has failed, continuing anyway - please figure out what went wrong and fix it, then restart abacus to automatically recover", _prob_id);
	else
		db->setProblemUpdateTime(_prob_id, _update_time);

	db->release();
	
	return result;
}

ActSetProbAttrs::ActSetProbAttrs() {
	regcomp(&_file_reg, "^([0-9]{1,7}) ([0-9]{1,7}) ([-[:alnum:] _.]+)$", REG_EXTENDED);
}

ActSetProbAttrs::~ActSetProbAttrs() {
	regfree(&_file_reg);
}

#define act_error(x)	{ delete msg; return cc->sendError(x); }
bool ActSetProbAttrs::int_process(ClientConnection *cc, MessageBlock *mb) {
	uint32_t prob_id = atol((*mb)["prob_id"].c_str());
	
	string prob_type = (*mb)["prob_type"];
	if(prob_id && prob_type == "") {
		DbCon *db = DbCon::getInstance();
		if(!db)
			return cc->sendError("Error obtaining connection to database");
		prob_type = db->getProblemType(prob_id);
		db->release();
	}

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

	ProbMessage *msg = new ProbMessage(prob_id, prob_type);
	
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
					// TODO: Find a better heuristic.
					if(prob_id == 0)
						act_error("You cannot 'keep' a file that was never available to begin with.  This applies to 'new' problems only")
					else
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

static ActSetProbAttrs _act_setprobattrs;

static Message* create_prob_message() {
	return new ProbMessage();
}

static void init() __attribute__((constructor));
static void init() {
	ClientAction::registerAction(USER_TYPE_ADMIN, "setprobattrs", &_act_setprobattrs);
	Message::registerMessageFunctor(TYPE_ID_PROBLEMUPDATE, create_prob_message);
}
/**
 * Copyright (c) 2009 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "logger.h"
#include "acmconfig.h"
#include "serverconnection.h"
#include "messageblock.h"

#include <iostream>
#include <sstream>
#include <iterator>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <cassert>

using namespace std;

enum attribute_type {
	ATTR_INT,
	ATTR_STRING,
	ATTR_FILE,
	ATTR_ENUM,
	ATTR_LIST
};

struct attribute {
	string name;
	attribute_type type;
	vector<string> values;    // enums and lists

	bool found;
	string value;

	attribute() {}
	attribute(const string &name, attribute_type type, const vector<string> &values) :
		name(name), type(type), values(values), found(false) {}
};

class bad_format : public runtime_error
{
public:
	bad_format(const string &msg) : runtime_error(msg) {}
};

static const string special = "()[]{}*, ";

static bool is_special(char ch) {
	return special.find(ch) != string::npos;
}

static bool is_special(const string &s) {
	return s.size() == 1 && is_special(s[0]);
}

/* Splits a problem description into tokens. Special characters are
 * always separate tokens, otherwise tokens are broken by spaces.
 */
static vector<string> tokenize(const string &desc) {
	vector<string> ans;

	int p = 0;
	int sz = desc.size();
	while (true) {
		while (p < sz && desc[p] == ' ')
			p++;
		if (p == sz) return ans;

		if (is_special(desc[p])) {
			ans.push_back(desc.substr(p, 1));
			p++;
		}
		else {
			int q = p;
			while (q < sz && !is_special(desc[q]) && desc[q] != ' ')
				q++;
			ans.push_back(desc.substr(p, q - p));
			p = q;
		}
	}
	return ans;
}

/* Parses an enum {a,b,c} or list [a,b,c] from a token list. Returns the
 * elements.
 *
 * On entry, toks[ofs] points to the { or [.
 * On exit, toks[ofs] points to the } or ].
 */
static vector<string> parse_list(const vector<string> &toks, int &ofs) {
	int sz = toks.size();
	assert(ofs < sz && (toks[ofs] == "{" || toks[ofs] == "["));

	string target = toks[ofs] == "{" ? "}" : "]";
	ofs++;

	vector<string> ans;

	/* An empty list is a special case, so check for it */
	if (toks[ofs] == target)
		return ans;
	while (true)
	{
		if (ofs == sz || is_special(toks[ofs]) || toks[ofs].empty())
			throw bad_format("Bad formatting in list");

		ans.push_back(toks[ofs]);
		ofs++;
		if (toks[ofs] == target)
			return ans;
		else if (toks[ofs] != ",")
			throw bad_format("Bad formatting in list");
		ofs++;
	}
}

/* Takes a flat string describing the options that must be set and
 * turns it into a useful data structure.
 */
static map<string, attribute> parse_desc(const string &desc)
{
	vector<string> toks = tokenize(desc);
	int sz = toks.size();
	vector<string> name_stack;
	map<string, attribute> ans;

	int ofs = 0;
	if (ofs == sz) return ans;
	while (true) {
		string name, stype;
		attribute_type type;
		vector<string> values;

		if (ofs == sz || is_special(toks[ofs]) || toks[ofs].empty())
			throw bad_format("Missing name");
		name = toks[ofs++];

		if (ofs == sz)
			throw bad_format("Missing type");
		stype = toks[ofs];
		if (stype.size() != 1)
			throw bad_format("Bad type " + stype);

		switch (stype[0]) {
		case 'I':
			type = ATTR_INT;
			break;
		case 'S':
			type = ATTR_STRING;
			break;
		case 'F':
			type = ATTR_FILE;
			break;
		case '{':
			type = ATTR_ENUM;
			values = parse_list(toks, ofs);
			break;
		case '[':
			type = ATTR_LIST;
			values = parse_list(toks, ofs);
			break;
		case '(':
			name_stack.push_back(name);
			ofs++;
			continue;
		default:
			throw bad_format("Bad type " + stype);
		}

		ostringstream prefix;
		copy(name_stack.begin(), name_stack.end(), ostream_iterator<string>(prefix, "."));
		name = prefix.str() + name;
		ans[name] = attribute(name, type, values);

		ofs++;
		while (ofs < sz && toks[ofs] == ")") {
			if (name_stack.empty())
				throw bad_format("Mismatched )");
			name_stack.pop_back();
			ofs++;
		}
		if (ofs == sz) {
			if (!name_stack.empty())
				throw bad_format("Mismatched (");
			return ans;
		}
		if (toks[ofs] != ",")
			throw bad_format("Expecting ,");
		ofs++;
	}
}

/* Returns an exit status. Takes arguments starting from the type
 */
static int add_problem(ServerConnection &server_con, int argc, const char **argv) {
	string prob_type = argv[0];

	vector<string> types = server_con.getProblemTypes();
	if (find(types.begin(), types.end(), prob_type) == types.end()) {
		cerr << "Invalid problem type `" << prob_type << "'. Valid types are:\n";
		for (size_t i = 0; i < types.size(); i++)
			cerr << "    " << types[i] << "\n";
		return -1;
	}

	string desc = server_con.getProblemDescription(prob_type);
	vector<ProblemInfo> probs = server_con.getProblems();
	map<string, attribute> attribs;
	AttributeMap files, normal;
	ProblemList dependencies;

	try {
		attribs = parse_desc(desc);
	}
	catch (bad_format &e) {
		cerr << "Failed to parse problem format: " << e.what() << "\n";
		return -1;
	}

	/* Process the given arguments */
	for (int i = 1; i + 1 < argc; i += 2) {
		if (string(argv[i]) == "dependencies") {
			if (!dependencies.empty()) {
				cerr << "Dependencies listed twice\n";
				return -1;
			}
			/* Fake a list, to simplify the parsing */
			vector<string> toks = tokenize(argv[i + 1]);
			toks.insert(toks.begin(), "[");
			toks.push_back("]");

			vector<string> names;
			try {
				int ofs = 0;
				names = parse_list(toks, ofs);
				if (ofs != (int) toks.size() - 1) {
					throw bad_format("Trailing garbage after dependency list");
				}
			}
			catch (bad_format &e) {
				cerr << "Failed to parse dependency list: " << e.what() << "\n";
				return -1;
			}

			/* Maps problem short names to ids. The values are set to 0 once
			 * the dependency has been set, as a sentinel against
			 * double-inclusion.
			 */
			map<string, uint32_t> probs_map;
			for (size_t i = 0; i < probs.size(); i++)
				probs_map[probs[i].code] = probs[i].id;
			for (size_t i = 0; i < names.size(); i++) {
				if (!probs_map.count(names[i])) {
					cerr << "Unknown problem `" << names[i] << "'\n";
					return -1;
				}
				else if (probs_map[names[i]] == 0) {
					cerr << "Problem " << names[i] << " listed twice\n";
					return -1;
				}
				else
					dependencies.push_back(probs_map[names[i]]);
			}
		}
		else if (!attribs.count(argv[i])) {
			cerr << "Invalid attribute `" << argv[i] << "'. Valid attributes are:\n";
			for (map<string, attribute>::const_iterator j = attribs.begin(); j != attribs.end(); j++)
				cerr << "    " << j->first << "\n";
			return -1;
		}
		else {
			attribute &a = attribs[argv[i]];
			if (a.found) {
				cerr << "Attribute `" << argv[i] << "' specified twice\n";
				return -1;
			}

			switch (a.type) {
			case ATTR_FILE:
				files[argv[i]] = argv[i + 1];
				break;
			case ATTR_STRING:
			case ATTR_INT:
			case ATTR_ENUM:
			case ATTR_LIST:
				// We rely on the server to do validation
				normal[argv[i]] = argv[i + 1];
				break;
			}
		}
	}

	// We rely on the server to check that everything required is set
	if (!server_con.setProblemAttributes(0, prob_type, normal, files, dependencies))
		return -1;

	return 0;
}

int main(int argc, const char **argv) {
	if (argc < 5 || (argc & 1) == 0) {
		cerr << "Usage: " << argv[0] << " config username password type attrib value attrib value...\n";
		return -1;
	}

	string username = argv[2];
	string password = argv[3];

	ServerConnection _server_con;

	Config &conf = Config::getConfig();
	conf.load(DEFAULT_CLIENT_CONFIG);

	conf.load(argv[1]);

	log(LOG_DEBUG, "Connecting ...");

	if(!_server_con.connect(conf["server"]["address"], conf["server"]["service"]))
		return -1;

	log(LOG_DEBUG, "Authenticating ...");

	if(!_server_con.auth(username, password))
		return -1;

	int status = add_problem(_server_con, argc - 4, argv + 4);

	log(LOG_DEBUG, "Done.");

	_server_con.disconnect();
	return status;
}

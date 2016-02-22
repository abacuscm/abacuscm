/**
 * Copyright (c) 2013 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 */

#define __STDC_FORMAT_MACROS
#define __STDC_LIMIT_MACROS
#include "logger.h"
#include "acmconfig.h"
#include "serverconnection.h"
#include "messageblock.h"
#include "score.h"
#include "misc.h"

#include <vector>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <string>
#include <cassert>
#include <stdexcept>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>

using namespace std;

enum ProblemAttributeType {
	ATTR_INT,
	ATTR_STRING,
	ATTR_FILE,
	ATTR_ENUM,
	ATTR_LIST
};

struct ProblemAttribute {
	string name;
	ProblemAttributeType type;
	vector<string> values;    // enums and lists

	bool found;
	string value;

	ProblemAttribute() {}
	ProblemAttribute(const string &name, ProblemAttributeType type, const vector<string> &values) :
		name(name), type(type), values(values), found(false) {}
};

class ProblemBadFormat : public runtime_error {
public:
	ProblemBadFormat(const string &msg) : runtime_error(msg) {}
};

struct BalloonEvent
{
	time_t time;
	string contestant;
	string group;
	string problem;
};

class Standing : public Score {
private:
	vector<string> _raw;

public:
	const vector<string> &getRaw() const { return _raw; }

	Standing() {}
	Standing(const list<string> &row);
};

Standing::Standing(const list<string> &row) : _raw(row.begin(), row.end()) {
	setId(from_string<uint32_t>(_raw[STANDING_RAW_ID]));
	setUsername(_raw[STANDING_RAW_USERNAME]);
	setFriendlyname(_raw[STANDING_RAW_FRIENDLYNAME]);
	setGroup(_raw[STANDING_RAW_GROUP]);
	setContestant(bool(from_string<int>(_raw[STANDING_RAW_CONTESTANT])));
	setTotalTime(from_string<time_t>(_raw[STANDING_RAW_TOTAL_TIME]));
	setTotalSolved(from_string<unsigned int>(_raw[STANDING_RAW_TOTAL_SOLVED]));
	for (size_t i = STANDING_RAW_SOLVED; i < _raw.size(); i++)
		setSolved(i - STANDING_RAW_SOLVED, from_string<int>(_raw[i]));
}

typedef map<uint32_t, Standing> StandingMap;

struct Standings
{
	vector<string> header;
	vector<StandingMap::const_iterator> ordered;
	StandingMap scores;

	Standings &operator=(const Standings &standings);
	Standings(const Standings &standings);
	Standings() {}
};

struct CompareStanding {
	bool operator()(StandingMap::const_iterator a, StandingMap::const_iterator b) const {
		return Score::CompareRankingStable()(a->second, b->second);
	}
};

Standings &Standings::operator=(const Standings &standings) {
	header = standings.header;
	scores = standings.scores;
	ordered.clear();
	ordered.reserve(scores.size());
	for (StandingMap::const_iterator i = scores.begin(); i != scores.end(); ++i)
		ordered.push_back(i);
	sort(ordered.begin(), ordered.end(), CompareStanding());
	return *this;
}

Standings::Standings(const Standings &standings) {
	*this = standings;
}


static void usage() {
	cerr << 
		"Usage: abacustool [options] command arguments...\n"
		"\n"
		"Common options:\n"
		"  -c client.conf    Extra client config file\n"
		"  -u username       User to connect as [admin]\n"
		"  -p password       Password\n"
		"  -P passwordfile   File containing just the password\n"
	"  -s server.conf    Server config (to get password)\n"
		"Exactly one of -p, -P and -s must be specified.\n"
		"\n"
		"One-off commands:\n"
		"   adduser <username> <name> <password> <type> [<group>]\n"
		"   addgroup <group>\n"
		"   addproblem <type> <attrib1> <value1> <attrib2> <value2>...\n"
		"   replaceproblem <name> <type> <attrib1> <value1>...\n"
		"   addtime start|stop <time> [<group>]\n"
		"   setpass <user> <newpassword>\n"
		"   getbonus <user>\n"
		"   setbonus <user> <points> <seconds>\n"
		"   getsource <submission_id>\n"
		"   getlatestsource <user> <problem>\n"
		"   submit <problem> <language> <filename>\n"
		"Times for start/stop are passed to date(1) for interpretation.\n"
		"\n"
		"Commands that run forever:\n"
		"   balloon [<group>]\n"
		"   standings [<filename>]\n";
}

static string read_password(const char *filename) {
	string password;
	ifstream in(filename);
	if (!in) {
		cerr << "Could not open " << filename << ": ";
		perror("");
		exit(1);
	}
	getline(in, password);
	if (!in) {
		cerr << "Error reading " << filename << ": ";
		perror("");
		exit(1);
	}
	return password;
}

static string read_config_password(const char *filename) {
	Config conf;
	if (!conf.load(filename))
		exit(1);
	ConfigSection &section = conf["initialisation"];
	if (!section.count("admin_password")) {
		cerr << "No initialisation.admin_password found in " << filename << "\n";
		exit(1);
	}
	return section["admin_password"];
}

static void process_options(int argc, char * const *argv, string &user, string &password)
{
	user = "admin";
	password = "";
	string client_config = "";
	bool have_password = false;
	int opt;
	while ((opt = getopt(argc, argv, "+c:u:p:P:s:")) != -1) {
		switch (opt) {
		case 'c':
			client_config = optarg;
			break;
		case 'u':
			user = optarg;
			break;
		case 'p':
			if (have_password) {
				usage();
				exit(2);
			}
			password = optarg;
			have_password = true;
			break;
		case 'P':
			if (have_password) {
				usage();
				exit(2);
			}
			password = read_password(optarg);
			have_password = true;
			break;
		case 's':
			if (have_password) {
				usage();
				exit(2);
			}
			password = read_config_password(optarg);
			have_password = true;
			break;
		case '?':
		case ':':
			usage();
			exit(2);
		}
	}
	if (!have_password || optind >= argc) {
		usage();
		exit(2);
	}

	Config &conf = Config::getConfig();
	conf.load(DEFAULT_CLIENT_CONFIG1, true);
	conf.load(DEFAULT_CLIENT_CONFIG2, true);
	if (!client_config.empty()) {
		if (!conf.load(client_config)) {
			exit(1);
		}
	}
}

static bool connect(ServerConnection &con, const string &user, const string &password) {
	Config &conf = Config::getConfig();

	log(LOG_DEBUG, "Connecting ...\n");
	if (!con.connect(conf["server"]["address"], conf["server"]["service"]))
		return false;

	log(LOG_DEBUG, "Authenticating ...\n");
	if (!con.auth(user, password))
	{
		con.disconnect();
		return false;
	}
	return true;
}

// Retrieves a user ID from a user name, returns 0 if not found
static uint32_t find_user(ServerConnection &con, const string &username) {
	vector<UserInfo> users = con.getUsers();
	for (vector<UserInfo>::const_iterator pos = users.begin(); pos != users.end(); ++pos)
		if (pos->username == username)
			return pos->id;
	return 0;
}

// Retrieves a group ID from a group name; returns 0 if not found
static uint32_t find_group(ServerConnection &con, const string &group) {
	vector<GroupInfo> groups = con.getGroups();
	for (vector<GroupInfo>::const_iterator pos = groups.begin(); pos != groups.end(); ++pos)
		if (pos->groupname == group)
			return pos->id;
	return 0;
}

// Retrieves a problem short name from a group name; returns 0 if not found
static uint32_t find_problem(ServerConnection &con, const string &problem) {
	vector<ProblemInfo> problems = con.getProblems();
	for (vector<ProblemInfo>::const_iterator pos = problems.begin(); pos != problems.end(); ++pos)
		if (pos->code == problem)
			return pos->id;
	return 0;
}

/* Passes argument to date(1) to obtain a UNIX timestamp. While this is not exactly
 * secure, it gives more flexibility in specifying times than strptime(3).
 *
 * If the time could not be parsed, returns 0.
 */
static time_t interpret_date(const string &date) {
	int pipes[2];
	if (pipe(pipes) != 0) {
		perror("pipe");
		return 0;
	}
	pid_t pid = fork();
	switch (pid) {
	case -1:
		// failure
		close(pipes[0]);
		close(pipes[1]);
		return 0;
	case 0:
		// Child
		close(pipes[0]);
		close(0);
		close(1);
		if (dup2(pipes[1], 1) == -1) {
			perror("dup2");
			exit(127);
		}
		if (pipes[1] != 1)
			close(pipes[1]);
		execl("/bin/date", "/bin/date", "+%s", "--date", date.c_str(), NULL);
		perror("exec failed");
		exit(127);
	default: {
			// parent
			FILE *in;
			uintmax_t ts = 0;
			int status;

			close(pipes[1]);
			in = fdopen(pipes[0], "r");
			fscanf(in, "%" SCNuMAX, &ts);
			if (waitpid(pid, &status, 0) == -1) {
				perror("waitpid");
				ts = 0; // mark as failure
			}
			else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
				cerr << "date failed: " << status << "\n";
				ts = 0; // mark as failure
			}
			fclose(in);
			return ts;
		}
		break;
	}
}

static int do_adduser(ServerConnection &con, int argc, char * const *argv) {
	if (argc < 5 || argc > 6) {
		cerr << "Usage: adduser <username> <name> <password> <type> [<group>]\n";
		return 2;
	}
	string username = argv[1];
	string name = argv[2];
	string password = argv[3];
	string type = argv[4];
	string group = argc > 5 ? argv[5] : "default";
	uint32_t group_id = find_group(con, group);
	if (group_id == 0) {
		cerr << "No such group `" << group_id << "'\n";
		return 1;
	}
	if (!con.createuser(username, name, password, type, group_id))
		return 1;
	return 0;
}

static int do_addgroup(ServerConnection &con, int argc, char * const *argv) {
	if (argc != 2) {
		cerr << "Usage: addgroup <group>\n";
		return 2;
	}
	if (!con.creategroup(argv[1]))
		return 1;
	return 0;
}

static bool is_problem_special(char ch) {
	static const string problem_special = "()[]{}*?, ";
	return problem_special.find(ch) != string::npos;
}

static bool is_problem_special(const string &s) {
	return s.size() == 1 && is_problem_special(s[0]);
}

/* Splits a problem description into tokens. Special characters are
 * always separate tokens, otherwise tokens are broken by spaces.
 */
static vector<string> problem_tokenize(const string &desc) {
	vector<string> ans;

	int p = 0;
	int sz = desc.size();
	while (true) {
		while (p < sz && desc[p] == ' ')
			p++;
		if (p == sz) return ans;

		if (is_problem_special(desc[p])) {
			ans.push_back(desc.substr(p, 1));
			p++;
		}
		else {
			int q = p;
			while (q < sz && !is_problem_special(desc[q]) && desc[q] != ' ')
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
static vector<string> problem_parse_list(const vector<string> &toks, int &ofs) {
	int sz = toks.size();
	assert(ofs < sz && (toks[ofs] == "{" || toks[ofs] == "["));

	string target = toks[ofs] == "{" ? "}" : "]";
	ofs++;

	vector<string> ans;

	/* An empty list is a special case, so check for it */
	if (toks[ofs] == target)
		return ans;
	while (true) {
		if (ofs == sz || is_problem_special(toks[ofs]) || toks[ofs].empty())
			throw ProblemBadFormat("Bad formatting in list");

		ans.push_back(toks[ofs]);
		ofs++;
		if (toks[ofs] == target)
			return ans;
		else if (toks[ofs] != ",")
			throw ProblemBadFormat("Bad formatting in list");
		ofs++;
	}
}

/* Takes a flat string describing the options that must be set and
 * turns it into a useful data structure.
 */
static map<string, ProblemAttribute> problem_parse_desc(const string &desc) {
	vector<string> toks = problem_tokenize(desc);
	int sz = toks.size();
	vector<string> name_stack;
	map<string, ProblemAttribute> ans;

	int ofs = 0;
	if (ofs == sz) return ans;
	while (true) {
		string name, stype;
		ProblemAttributeType type;
		vector<string> values;

		if (ofs == sz || is_problem_special(toks[ofs]) || toks[ofs].empty())
			throw ProblemBadFormat("Missing name");
		name = toks[ofs++];

		if (ofs == sz)
			throw ProblemBadFormat("Missing type");
		stype = toks[ofs];
		if (stype.size() != 1)
			throw ProblemBadFormat("Bad type " + stype);

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
			values = problem_parse_list(toks, ofs);
			break;
		case '[':
			type = ATTR_LIST;
			values = problem_parse_list(toks, ofs);
			break;
		case '(':
			name_stack.push_back(name);
			ofs++;
			continue;
		default:
			throw ProblemBadFormat("Bad type " + stype);
		}

		ostringstream prefix;
		copy(name_stack.begin(), name_stack.end(), ostream_iterator<string>(prefix, "."));
		name = prefix.str() + name;
		ans[name] = ProblemAttribute(name, type, values);

		ofs++;
		if (ofs < sz && toks[ofs] == "?") {
			// Optional attribute. We don't need to store this information,
			// because it's part of server-side checking.
			ofs++;
		}
		while (ofs < sz && toks[ofs] == ")") {
			if (name_stack.empty())
				throw ProblemBadFormat("Mismatched )");
			name_stack.pop_back();
			ofs++;
		}
		if (ofs == sz) {
			if (!name_stack.empty())
				throw ProblemBadFormat("Mismatched (");
			return ans;
		}
		if (toks[ofs] != ",")
			throw ProblemBadFormat("Expecting ,");
		ofs++;
	}
}

/* Code shared between addproblem and replaceproblem. The argc and argv start
 * from the problem type. The problem ID is zero to add a new problem,
 * otherwise a problem to update.
 */
static int do_add_or_replace_problem(ServerConnection &con, int prob_id, int argc, char * const *argv) {
	string prob_type = argv[0];
	vector<string> types = con.getProblemTypes();
	if (find(types.begin(), types.end(), prob_type) == types.end()) {
		cerr << "Invalid problem type `" << prob_type << "'. Valid types are:\n";
		for (size_t i = 0; i < types.size(); i++)
			cerr << "    " << types[i] << "\n";
		return 1;
	}

	string desc = con.getProblemDescription(prob_type);
	vector<ProblemInfo> probs = con.getProblems();
	map<string, ProblemAttribute> attribs;
	AttributeMap files, normal;
	ProblemList dependencies;

	try {
		attribs = problem_parse_desc(desc);
	}
	catch (ProblemBadFormat &e) {
		cerr << "Failed to parse problem format: " << e.what() << "\n";
		return 1;
	}

	/* Process the given arguments */
	for (int i = 1; i + 1 < argc; i += 2) {
		if (string(argv[i]) == "dependencies") {
			if (!dependencies.empty()) {
				cerr << "Dependencies listed twice\n";
				return 1;
			}
			/* Fake a list, to simplify the parsing */
			vector<string> toks = problem_tokenize(argv[i + 1]);
			toks.insert(toks.begin(), "[");
			toks.push_back("]");

			vector<string> names;
			try {
				int ofs = 0;
				names = problem_parse_list(toks, ofs);
				if (ofs != (int) toks.size() - 1) {
					throw ProblemBadFormat("Trailing garbage after dependency list");
				}
			}
			catch (ProblemBadFormat &e) {
				cerr << "Failed to parse dependency list: " << e.what() << "\n";
				return 1;
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
					return 1;
				}
				else if (probs_map[names[i]] == 0) {
					cerr << "Problem " << names[i] << " listed twice\n";
					return 1;
				}
				else
					dependencies.push_back(probs_map[names[i]]);
			}
		}
		else if (!attribs.count(argv[i])) {
			cerr << "Invalid attribute `" << argv[i] << "'. Valid attributes are:\n";
			for (map<string, ProblemAttribute>::const_iterator j = attribs.begin(); j != attribs.end(); j++)
				cerr << "    " << j->first << "\n";
			return 1;
		}
		else {
			ProblemAttribute &a = attribs[argv[i]];
			if (a.found) {
				cerr << "Attribute `" << argv[i] << "' specified twice\n";
				return 1;
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
	if (!con.setProblemAttributes(prob_id, prob_type, normal, files, dependencies))
		return 1;

	return 0;
}

static int do_addproblem(ServerConnection &con, int argc, char * const *argv) {
	if (argc < 4 || argc % 2 != 0) {
		cerr << "Usage: addproblem <type> <attrib1> <value1> <attrib2> <value2>...\n";
		return 2;
	}

	return do_add_or_replace_problem(con, 0, argc - 1, argv + 1);
}

static int do_replaceproblem(ServerConnection &con, int argc, char * const *argv) {
	if (argc < 5 || argc % 2 != 1) {
		cerr << "Usage: replaceproblem <name> <type> <attrib1> <value1>...\n";
		return 2;
	}

	vector<ProblemInfo> probs = con.getProblems();
	uint32_t prob_id = 0;
	for (std::size_t i = 0; i < probs.size(); i++)
		if (probs[i].code == argv[1]) {
			prob_id = probs[i].id;
			break;
		}

	if (prob_id == 0) {
		cerr << "Unknown problem `" << argv[1] << "'\n";
		return 2;
	}

	return do_add_or_replace_problem(con, prob_id, argc - 2, argv + 2);
}

static int do_addtime(ServerConnection &con, int argc, char * const *argv) {
	const string startString = "start";
	const string stopString = "stop";
	if (argc < 3 || argc > 4
		|| (argv[1] != startString && argv[1] != stopString)) {
		cerr << "Usage: addtime start|stop <time> [<group>]\n";
		return 2;
	}

	uint32_t group_id = 0;
	if (argc >= 4) {
		group_id = find_group(con, argv[3]);
		if (group_id == 0) {
			cerr << "No such group `" << argv[3] << "'\n";
			return 1;
		}
	}
	time_t time = interpret_date(argv[2]);
	if (time == 0) {
		cerr << "Could not parse time `" << argv[2] << "'\n";
		return 1;
	}
	if (!con.startStop(group_id, argv[1] == startString, time))
		return 1;
	return 0;
}

static int getsource_helper(ServerConnection &con, uint32_t submission_id) {
	uint32_t sourceLength;
	char *source;
    uint32_t languageLength;
    char *language;
	log(LOG_DEBUG, "Retrieving source for submission %u", submission_id);
	if (!con.getSubmissionSource(submission_id, &source, &sourceLength, &language, &languageLength))
		return 1;
	else {
		string lang(language, languageLength);
        delete[] language;
        cerr << "LANGUAGE: " << lang << endl;

		string src(source, sourceLength);
		delete[] source;
		cout << src;

		return 0;
	}
}

static int do_getsource(ServerConnection &con, int argc, char * const *argv) {
	if (argc != 2) {
		cerr << "Usage: getsource <submission_id>\n";
		return 2;
	}

	uint32_t submission_id;
	if (!from_string(argv[1], submission_id)) {
		cerr << "Invalidly formatted submission id\n";
		return 2;
	}

	return getsource_helper(con, submission_id);
}

static int do_getlatestsource(ServerConnection &con, int argc, char * const *argv) {
	if (argc != 3) {
		cerr << "Usage: getlatestsource <user> <problem>\n";
		return 2;
	}
	const char *user = argv[1];
	const char *problem = argv[2];

	uint32_t user_id = find_user(con, user);
	if (user_id == 0) {
		cerr << "No such user `" << user << "'\n";
		return 1;
	}

	uint32_t problem_id = find_problem(con, problem);
	if (problem_id == 0) {
		cerr << "No such problem `" << problem << "'\n";
		return 1;
	}

	uint32_t submission_id = 0;
	time_t time = 0;
	SubmissionList submissions = con.getSubmissionsForUser(user_id);
	for (SubmissionList::iterator it = submissions.begin();
		 it != submissions.end();
		 it++)
	{
		uint32_t s_problem_id = from_string<uint32_t>((*it)["prob_id"]);
		uint32_t s_submission_id = from_string<uint32_t>((*it)["submission_id"]);
		time_t s_time = from_string<time_t>((*it)["time"]);
		int result = from_string<int>((*it)["result"]);

		log(LOG_DEBUG, "Submission %u was for problem %i at time %lu with comment %s and result %d", s_submission_id, s_problem_id, (unsigned long) s_time, (*it)["comment"].c_str(), atoi((*it)["result"].c_str()));

		// Check that the submission is for the right problem
		if (s_problem_id != problem_id)
			continue;

		// If this submission occurred later and is correct, then it
		// supersedes any previous submission we've seen.
		if (s_time > time && result == CORRECT) {
			submission_id = s_submission_id;
			time = s_time;
		}
	}
	if (submission_id == 0) {
		cerr << "No correct submissions for user `" << user << "' on `" << problem << "'\n";
		return 1;
	}
	else
		return getsource_helper(con, submission_id);
}

static int do_setpass(ServerConnection &con, int argc, char * const *argv) {
	if (argc != 3) {
		cerr << "Usage: setpass <user> <newpassword>\n";
		return 2;
	}

	uint32_t user_id = find_user(con, argv[1]);
	if (user_id == 0) {
		cerr << "No such user `" << argv[1] << "'\n";
		return 1;
	}

	if (!con.changePassword(user_id, argv[2]))
		return 1;
	return 0;
}

static int do_getbonus(ServerConnection &con, int argc, char * const *argv) {
	if (argc != 2) {
		cerr << "Usage: getbonus <user>\n";
		return 2;
	}

	uint32_t user_id = find_user(con, argv[1]);
	if (user_id == 0) {
		cerr << "No such user `" << argv[1] << "'\n";
		return 1;
	}

	int32_t points, seconds;
	if (!con.getBonus(user_id, points, seconds))
		return 1;
	cout << points << " points, " << seconds << " seconds\n";
	return 0;
}

static int do_setbonus(ServerConnection &con, int argc, char *const *argv) {
	if (argc != 4) {
		cerr << "Usage: setbonus <user> <points> <seconds>\n";
		return 2;
	}

	int32_t points, seconds;
	if (!from_string(argv[2], points) || !from_string(argv[3], seconds)) {
		cerr << "Could not parse values\n";
		return 2;
	}

	uint32_t user_id = find_user(con, argv[1]);
	if (user_id == 0) {
		cerr << "No such user `" << argv[1] << "'\n";
		return 1;
	}

	if (!con.setBonus(user_id, points, seconds))
		return 1;
	return 0;
}

static int do_submit(ServerConnection &con, int argc, char * const *argv) {
	if (argc != 4) {
		cerr << "Usage: submit <problem> <language> <filename>\n";
		return 2;
	}

	int problem_id = find_problem(con, argv[1]);
	if (problem_id == 0) {
		cerr << "No such problem `" << argv[1] << "'\n";
		return 1;
	}

	const string language = argv[2];
	vector<string> languages = con.getLanguages();
	if (find(languages.begin(), languages.end(), language) == languages.end()) {
		cerr << "Language " << argv[2] << " not supported. Supported languages are:\n";
		for (std::size_t i = 0; i < languages.size(); i++)
			cerr << "  " << languages[i] << "\n";
		return 1;
	}

	int fd = open(argv[3], O_RDONLY);
	if (fd == -1) {
		cerr << "Unable to open `" << argv[3] << "' for reading\n";
		return 1;
	}

	uint32_t submission_id = con.submit(problem_id, fd, language);
	close(fd);
	if (submission_id != 0) {
		// success
		cout << "Submission id is " << submission_id << '\n';
		return 0;
	}
	else
		return 1;
}

static bool closed = false;             // set to true when a server disconnect is detected
static pthread_mutex_t runlock;         // held by main thread except when waiting for something
static pthread_cond_t runcond;          // signalled by worker thread to indicate new data
static Standings balloonBaseline;       // old standings - only used by callback after init
static queue<BalloonEvent> balloonQueue;// queue of unprinted balloon notifications

// Callback used to indicate that the server disconnected
static void server_close(const MessageBlock*, void*) {
	pthread_mutex_lock(&runlock);
	closed = true;
	cout << "Server has disconnected\n" << flush;
	pthread_cond_signal(&runcond);
	pthread_mutex_unlock(&runlock);
}

/* Updates the standings map and the ordered view of it in place, working from
 * an incremental standings update.
 */
static void grid_to_standings(const Grid &grid, Standings &standings) {
	if (grid.empty())
		return;

	Grid::const_iterator row = grid.begin();
	standings.header = vector<string>(row->begin(), row->end());

	for (++row; row != grid.end(); ++row) {
		Standing s(*row);
		if (!s.isContestant())
			continue;

		StandingMap::iterator pos = standings.scores.lower_bound(s.getId());
		if (pos == standings.scores.end() || pos->first != s.getId()) {
			pos = standings.scores.insert(pos, make_pair(s.getId(), s));
			standings.ordered.push_back(pos);
		}
		else {
			pos->second = s;
		}
	}
	sort(standings.ordered.begin(), standings.ordered.end(), CompareStanding());
}

// Returns number of new balloons. Does not lock anything
static int queue_balloons(const Standings &old, const Standings &cur, const char *group) {
	// We take advantage of the fact that the map is ordered by user ID
	StandingMap::const_iterator j = old.scores.begin();
	int ans = 0;
	time_t time = ::time(NULL);
	for (StandingMap::const_iterator i = cur.scores.begin(); i != cur.scores.end(); ++i) {
		if (group != NULL && i->second.getGroup() != group)
			continue; // filtered out

		while (j != old.scores.end() && j->first < i->first)
			++j;
		set<string> solved; // Names of solved problems
		for (size_t k = 0; k < i->second.getSolved().size(); k++)
			if (i->second.getSolved(k) > 0)
				solved.insert(cur.header[STANDING_RAW_SOLVED + k]);
		if (j != old.scores.end() && i->first == j->first) {
			// found a match record in old, cancel out anything already seen
			for (size_t k = 0; k < j->second.getSolved().size(); k++)
				if (j->second.getSolved(k) > 0)
					solved.erase(old.header[STANDING_RAW_SOLVED + k]);
		}
		for (set<string>::const_iterator k = solved.begin(); k != solved.end(); ++k) {
			BalloonEvent event;
			event.time = time;
			event.contestant = i->second.getUsername();
			event.group = "unknown"; // TODO: add to protocol
			event.problem = *k;
			balloonQueue.push(event);
			ans++;
		}
	}
	return ans;
}

static void balloon_update(const MessageBlock *mb, void *data) {
	if (mb) {
		Grid grid = ServerConnection::gridFromMB(*mb);
		Standings old = balloonBaseline;
		grid_to_standings(grid, balloonBaseline);

		pthread_mutex_lock(&runlock);
		if (queue_balloons(old, balloonBaseline, (const char *) data) > 0)
			pthread_cond_signal(&runcond);
		pthread_mutex_unlock(&runlock);
	}
}

static int do_balloon(ServerConnection &con, int argc, char * const *argv) {
	static bool first = true;

	if (argc < 1 || argc > 2) {
		cerr << "Usage: balloon [group]\n";
		return 2;
	}
	char *group = argc >= 2 ? argv[1] : NULL;

	con.registerEventCallback("close", server_close, NULL);
	con.registerEventCallback("updatestandings", balloon_update, group);

	Standings old = balloonBaseline;
	grid_to_standings(con.getStandings(), balloonBaseline);
	if (first)
		first = false;
	else
		queue_balloons(old, balloonBaseline, group);

	while (!closed || !balloonQueue.empty()) {
		if (!balloonQueue.empty()) {
			BalloonEvent event = balloonQueue.front();
			balloonQueue.pop();

			// A message we're actually interested in
			pthread_mutex_unlock(&runlock);

			string dummy;
			struct tm time_tm;
			char time_buffer[64];

			localtime_r(&event.time, &time_tm);
			strftime(time_buffer, sizeof(time_buffer), "%X", &time_tm);
			cout
				<< time_buffer << " " << event.contestant << " solved " << event.problem
				<< " - press enter to acknowledge ..." << flush;
			getline(cin, dummy);  // read any old string

			pthread_mutex_lock(&runlock);
		}
		else {
			cout << "\nWaiting for balloon events ...\n\n" << flush;
			pthread_cond_wait(&runcond, &runlock);
		}
	}
	return 0;
}

static void standings_update(const MessageBlock *mb, void *data) {
	pthread_mutex_lock(&runlock);
	if (mb) {
		Grid grid = ServerConnection::gridFromMB(*mb);
		grid_to_standings(grid, *static_cast<Standings *>(data));
		pthread_cond_signal(&runcond);
	}
	pthread_mutex_unlock(&runlock);
}

static string safe_string(string s) {
	// There shouldn't be any newlines or tabs, but turn them into
	// spaces just to be safe.
	replace(s.begin(), s.end(), '\t', ' ');
	replace(s.begin(), s.end(), '\n', ' ');
	return s;
}

static void update_standings(const string &fname, Standings &standings) {
	string tmpname = fname + ".tmp";

	ofstream out(tmpname.c_str());
	if (!out)
	{
		cerr << "Warning: failed to open " << tmpname << "!\n";
		return;
	}
	for (vector<string>::const_iterator i = standings.header.begin(); i != standings.header.end(); ++i) {
		if (i != standings.header.begin())
			out << "\t";
		out << safe_string(*i);
	}
	out << "\n";
	vector<StandingMap::const_iterator>::const_iterator i;
	for (i = standings.ordered.begin(); i != standings.ordered.end(); ++i)
	{
		const vector<string> &raw = (*i)->second.getRaw();
		for (vector<string>::const_iterator c = raw.begin(); c != raw.end(); c++)
		{
			if (c != raw.begin()) out << "\t";
			out << safe_string(*c);
		}
		out << "\n";
	}
	if (!out)
	{
		cerr << "Warning: error writing " << tmpname << "!\n";
		out.close();
		unlink(tmpname.c_str());
	}
	else
	{
		out.close();
		if (rename(tmpname.c_str(), fname.c_str()) != 0)
		{
			cerr << "Error: could not rename " << tmpname << " to " << fname << "!\n";
			unlink(tmpname.c_str());
		}
		else
			cout << "Successfully wrote standings to " << fname << "\n";
	}
}

static int do_standings(ServerConnection &con, int argc, char * const *argv) {
	if (argc != 2) {
		cerr << "Usage: standings <filename>\n";
		return 2;
	}
	string fname = argv[1];

	Standings standings;
	con.registerEventCallback("close", server_close, NULL);
	con.registerEventCallback("updatestandings", standings_update, &standings);

	grid_to_standings(con.getStandings(), standings);
	update_standings(fname, standings);
	while (true) {
		pthread_cond_wait(&runcond, &runlock);
		if (closed)
			break;
		update_standings(fname, standings);
	}
	return 0;
}

// Returns an exit status
static int process(ServerConnection &con, int argc, char * const *argv) {
	if (argv[0] == string("adduser"))
		return do_adduser(con, argc, argv);
	else if (argv[0] == string("addgroup"))
		return do_addgroup(con, argc, argv);
	else if (argv[0] == string("addproblem"))
		return do_addproblem(con, argc, argv);
	else if (argv[0] == string("replaceproblem"))
		return do_replaceproblem(con, argc, argv);
	else if (argv[0] == string("addtime"))
		return do_addtime(con, argc, argv);
	else if (argv[0] == string("setpass"))
		return do_setpass(con, argc, argv);
	else if (argv[0] == string("getbonus"))
		return do_getbonus(con, argc, argv);
	else if (argv[0] == string("setbonus"))
		return do_setbonus(con, argc, argv);
	else if (argv[0] == string("getsource"))
		return do_getsource(con, argc, argv);
	else if (argv[0] == string("getlatestsource"))
		return do_getlatestsource(con, argc, argv);
	else if (argv[0] == string("submit"))
		return do_submit(con, argc, argv);
	else if (argv[0] == string("balloon"))
		return do_balloon(con, argc, argv);
	else if (argv[0] == string("standings"))
		return do_standings(con, argc, argv);
	else {
		cerr << "Unknown command `" << argv[0] << "'\n";
		return 2;
	}
}

static void log_listener(int prio, const char *format, va_list ap) {
	// Avoid printing debug messages - they're just clutter
	if (prio < LOG_DEBUG) {
		flockfile(stderr);
		vfprintf(stderr, format, ap);
		fputs("\n", stderr);
		funlockfile(stderr);
	}
}

int main(int argc, char * const *argv) {
	int status;

	// This overrides any listener set in the client config file,
	// to ensure that error messages are printed to stderr
	register_log_listener(log_listener);

	ServerConnection::init();

	string login_user, login_pass;
	process_options(argc, argv, login_user, login_pass);

	if (argv[optind] == string("balloon") || argv[optind] == string("standings"))
	{
		int status = 0;
		pthread_mutex_init(&runlock, NULL);
		pthread_cond_init(&runcond, NULL);
		while (true) // indicates usage error, should exit
		{
			pthread_mutex_lock(&runlock);
			ServerConnection con;
			closed = false;
			bool connected = false;
			if (connect(con, login_user, login_pass)) {
				status = process(con, argc - optind, argv + optind);
				connected = true;
			}
			pthread_mutex_unlock(&runlock);
			if (connected)
				con.disconnect();
			if (status == 2)
				break;
			cerr << "Sleeping for 10 seconds\n";
			sleep(10);
		}
		pthread_mutex_destroy(&runlock);
		pthread_cond_destroy(&runcond);
		return status;
	}
	else
	{
		ServerConnection con;
		if (!connect(con, login_user, login_pass))
			return 1;
		status = process(con, argc - optind, argv + optind);
		con.disconnect();
		return status;
	}
}

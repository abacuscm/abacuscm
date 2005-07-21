#include "dbcon.h"
#include "logger.h"
#include "config.h"

#include <mysql/mysql.h>
#include <sstream>

#define log_mysql_error()	log(LOG_ERR,"%s: %s", __PRETTY_FUNCTION__, mysql_error(&_mysql))

using namespace std;

class MySQL : public DbCon {
private:
	MYSQL _mysql;

	string escape_string(const string& str);
public:
	MySQL();
	virtual ~MySQL();

	virtual bool ok();
	virtual uint32_t name2server_id(const string& name);
	virtual string getServerAttribute(uint32_t server_id, const string& attribute);
	virtual bool setServerAttribute(uint32_t server_id, const string& attribute, const string& value);
	
	bool init();
};

MySQL::MySQL() {
}

MySQL::~MySQL() {
	NOT_IMPLEMENTED();
}

string MySQL::escape_string(const string& str) {
	if(str.length() > 512) {
		char *tmp_buffer = new char[str.length() * 2 + 1];
		mysql_real_escape_string(&_mysql, tmp_buffer, str.c_str(), str.length());
		string tmp_string(tmp_buffer);
		delete []tmp_buffer;
		return tmp_string;
	} else {
		char tmp_buffer[1025];
		mysql_real_escape_string(&_mysql, tmp_buffer, str.c_str(), str.length());
		return string(tmp_buffer);
	}
}

bool MySQL::ok() {
	if(mysql_ping(&_mysql)) {
		log(LOG_INFO, "MySQL: %s", mysql_error(&_mysql));
		return false;
	} else
		return true;
}
	
uint32_t MySQL::name2server_id(const string& name) {
	uint32_t server_id = 0;
	ostringstream query;
	query << "SELECT server_id FROM Server WHERE server_name='" << escape_string(name) << "'";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return ~0U;
	}

	MYSQL_RES *res = mysql_use_result(&_mysql);
	if(!res) {
		log_mysql_error();
		return ~0U;
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	
	if(row)
		server_id = atol(row[0]);
	
	mysql_free_result(res);
	
	return server_id;
}
	
string MySQL::getServerAttribute(uint32_t server_id, const string& attribute) {
	NOT_IMPLEMENTED();
	return "";
}

bool MySQL::setServerAttribute(uint32_t server_id, const string& attribute, const string& value) {
	NOT_IMPLEMENTED();
	return false;
}

bool MySQL::init() {
	Config &config = Config::getConfig();
	
	string host = config["mysql"]["host"];
	string user = config["mysql"]["user"];
	string pass = config["mysql"]["password"];
	string db   = config["mysql"]["db"];
	unsigned int port = atol(config["mysql"]["port"].c_str());
	
	mysql_init(&_mysql);

	if(!mysql_real_connect(&_mysql, host.c_str(), user.c_str(), pass.c_str(), db.c_str(), port, NULL, 0)) {
		log(LOG_ERR, "MySQL Error: %s", mysql_error(&_mysql));
		return false;
	}

	log(LOG_INFO, "Established connection to MySQL Server.");
	return true;
}

/////////////////////////////////////////////////////////////
static DbCon* MySQLFunctor() {
	MySQL *db = new MySQL();
	if(!db->init()) {
		delete db;
		db = NULL;
	}
	return db;
}

static void init() __attribute__((constructor));
static void init() {
	DbCon::registerFunctor(MySQLFunctor);
}

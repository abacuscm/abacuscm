#include "dbcon.h"
#include "logger.h"
#include "config.h"

#include <mysql/mysql.h>

using namespace std;

class MySQL : public DbCon {
private:
	MYSQL _mysql;
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

bool MySQL::ok() {
	if(mysql_ping(&_mysql)) {
		log(LOG_INFO, "MySQL: %s", mysql_error(&_mysql));
		return false;
	} else
		return true;
}
	
uint32_t MySQL::name2server_id(const string& name) {
	NOT_IMPLEMENTED();
	return ~0;
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

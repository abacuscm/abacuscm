#include "dbcon.h"
#include "logger.h"

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
	NOT_IMPLEMENTED();
	return false;
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
	NOT_IMPLEMENTED();
	return false;
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

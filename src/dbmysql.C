#include "dbcon.h"
#include "logger.h"
#include "config.h"
#include "message.h"
#include "server.h"

#include <mysql/mysql.h>
#include <sstream>

#define log_mysql_error()	log(LOG_ERR,"%s: %s", __PRETTY_FUNCTION__, mysql_error(&_mysql))

using namespace std;

class MySQL : public DbCon {
private:
	MYSQL _mysql;

	string escape_string(const string& str);
	string escape_buffer(const uint8_t* bfr, uint32_t size);
public:
	MySQL();
	virtual ~MySQL();

	virtual bool ok();
	virtual uint32_t name2server_id(const string& name);
	virtual string getServerAttribute(uint32_t server_id, const string& attribute);
	virtual bool setServerAttribute(uint32_t server_id, const string& attribute, const string& value);
	virtual bool putLocalMessage(Message*);
	virtual bool putRemoteMessage(const Message*);
	virtual bool markProcessed(uint32_t server_id, uint32_t message_id);
	virtual bool addServer(const string& name, uint32_t id);
	virtual bool addUser(const std::string& name, const std::string& pass, uint32_t id, uint32_t type);
	virtual int authenticate(const std::string& uname, const std::string& pass, uint32_t *user_id, uint32_t *user_type);
	virtual bool setPassword(uint32_t user_id, const std::string& newpass);
	virtual uint32_t maxServerId();
	virtual uint32_t maxUserId();

	bool init();
};

MySQL::MySQL() {
}

MySQL::~MySQL() {
	mysql_close(&_mysql);
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

string MySQL::escape_buffer(const uint8_t* bfr, uint32_t size) {
	char *tmp_bfr = new char[size * 2 + 1];
	mysql_real_escape_string(&_mysql, tmp_bfr, (char*)bfr, size);
	string tmp_str(tmp_bfr);
	delete []tmp_bfr;
	return tmp_str;
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
	string value = "";
	ostringstream query;
	query << "SELECT value FROM ServerAttributes WHERE server_id=" <<
		server_id << ", '" << escape_string(attribute) << "'";
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return "";
	}

	MYSQL_RES *res = mysql_store_result(&_mysql);
	if(res) {
		MYSQL_ROW row = mysql_fetch_row(res);
		if(row && row[0])
			value = row[0];
		mysql_free_result(res);
	}
	return value;
}

bool MySQL::setServerAttribute(uint32_t server_id, const string& attribute, const string& value) {
	string esc_attribute = escape_string(attribute);
	string esc_value = escape_string(value);
	ostringstream query;
	query << "INSERT INTO ServerAttributes (server_id, attribute, value) "
		"VALUES(" << server_id << ", '" << esc_attribute << 
		"', '" << esc_value << "')";
	if(mysql_query(&_mysql, query.str().c_str())) {
		// most likely cause is that the attribute already exists, update it.
		query.str("");
		query << "UPDATE ServerAttributes SET value='" << esc_value <<
			"' WHERE server_id=" << server_id << " AND attribute='" <<
			esc_attribute << "'";
		if(mysql_query(&_mysql, query.str().c_str())) {
			log_mysql_error();
			return false;
		}
	}
	return true;
}

bool MySQL::putLocalMessage(Message* message) {
	const uint8_t *blob;
	uint32_t blobsize;
	uint32_t message_id = 0;

	if(!message->getData(&blob, &blobsize))
		return false;

	ostringstream query;
	query << "SELECT MAX(message_id) FROM PeerMessage WHERE server_id="
		<< Server::getId();

	if(mysql_query(&_mysql, "LOCK TABLES PeerMessage WRITE")) {
		log_mysql_error();
		return false;
	}
	
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		mysql_query(&_mysql, "UNLOCK TABLES");
		return false;
	}

	MYSQL_RES *res = mysql_use_result(&_mysql);
	if(res) {
		MYSQL_ROW row = mysql_fetch_row(res);
		if(row && row[0])
			message_id = atol(row[0]);
	}
	mysql_free_result(res);

	message_id++;
	message->setMessageId(message_id);

	query.str("");

	query << "INSERT INTO PeerMessage(server_id, message_id, message_type_id,"
		" time, signature, data, processed) VALUES(" << message->server_id()
		<< ", " << message->message_id() << ", " << message->message_type_id()
		<< ", " << message->time() << ", '"
		<< escape_buffer(message->getSignature(), MESSAGE_SIGNATURE_SIZE)
		<< "', '" << escape_buffer(blob, blobsize) << "', 0)";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		mysql_query(&_mysql, "UNLOCK TABLES");
		return false;
	}
		
	mysql_query(&_mysql, "UNLOCK TABLES");

	return false;
}

bool MySQL::putRemoteMessage(const Message*) {
	NOT_IMPLEMENTED();
	return false;
}

bool MySQL::markProcessed(uint32_t server_id, uint32_t message_id) {
	ostringstream query;
	query << "UPDATE PeerMessage SET processed=1 WHERE server_id=" <<
		server_id << " AND message_id=" << message_id;
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	} else
		return true;
}

bool MySQL::addServer(const string& name, uint32_t id) {
	ostringstream query;

	query << "INSERT INTO Server (server_id, server_name) VALUES (" << id << ", '" << escape_string(name) << "')";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}

	return true;
}
	
bool MySQL::addUser(const std::string& name, const std::string& pass, uint32_t id, uint32_t type) {
	ostringstream query;

	query << "INSERT INTO User (user_id, username, password, type) VALUES (" << id << ", '" << escape_string(name) << "', MD5('" << escape_string(name) << escape_string(pass) << "'), " << type << ")";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}

	return true;
}
	
int MySQL::authenticate(const std::string& uname, const std::string& pass, uint32_t *user_id, uint32_t *user_type) {
	ostringstream query;

	query << "SELECT user_id, type FROM User WHERE username='" << escape_string(uname) << "' AND password=MD5('" << escape_string(uname) << escape_string(pass) << "')";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return -1;
	}
	
	int ret = -1;
	MYSQL_RES *res = mysql_store_result(&_mysql);
	if(res) {
		ret = 0;
		MYSQL_ROW row = mysql_fetch_row(res);
		if(row) {
			ret = 1;

			if(row[0]) {
				if(user_id)
					*user_id = atol(row[0]);
			} else
				ret = -1;
			
			if(row[1]) {
				if(user_type)
					*user_type = atol(row[1]);
			} else
				ret = -1;
		}	
		mysql_free_result(res);
	}
	return ret;
}

bool MySQL::setPassword(uint32_t user_id, const std::string& newpass) {
	ostringstream query;
	query << "UPDATE User SET password=MD5(CONCAT(username, '" <<
		escape_string(newpass) << "')) WHERE user_id=" << user_id;
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}
	return true;
}

uint32_t MySQL::maxServerId() {
	uint32_t max_server_id = ~0U;
	if(mysql_query(&_mysql, "SELECT MAX(server_id) FROM Server")) {
		log_mysql_error();
		return ~0U;
	}

	MYSQL_RES *res = mysql_use_result(&_mysql);
	if(res) {
		MYSQL_ROW row = mysql_fetch_row(res);
		if(row && row[0])
			max_server_id = atol(row[0]);
		mysql_free_result(res);	
	}
	
	return max_server_id;
}

uint32_t MySQL::maxUserId() {
	uint32_t max_user_id = ~0U;
	ostringstream query;
	query << "SELECT MAX(user_id) FROM User WHERE user_id & " << (ID_GRANULARITY - 1) << " = " << Server::getId();
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return ~0U;
	}

	MYSQL_RES *res = mysql_use_result(&_mysql);
	if(res) {
		MYSQL_ROW row = mysql_fetch_row(res);
		if(row) {
			if(row[0])
				max_user_id = atol(row[0]);
			else
				max_user_id = 0;
		}
		mysql_free_result(res);	
	}

	return max_user_id;
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

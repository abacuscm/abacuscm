#include "dbcon.h"
#include "logger.h"
#include "config.h"
#include "message.h"
#include "server.h"

#include <mysql/mysql.h>
#include <sstream>

#define log_mysql_error()	log(LOG_ERR,"%s line %d: %s", __PRETTY_FUNCTION__, __LINE__, mysql_error(&_mysql))

using namespace std;

class MySQL : public DbCon {
private:
	MYSQL _mysql;

	string escape_string(const string& str) __attribute__((pure));
	string escape_buffer(const uint8_t* bfr, uint32_t size) __attribute__((pure));
	
	list<Message*> getMessages(std::string query);
public:
	MySQL();
	virtual ~MySQL();

	virtual bool ok();
	virtual uint32_t name2server_id(const string& name);
	virtual string getServerAttribute(uint32_t server_id,
			const string& attribute);
	virtual bool setServerAttribute(uint32_t server_id, const string& attribute,
			const string& value);
	virtual bool putLocalMessage(Message*);
	virtual bool putRemoteMessage(const Message*);
	virtual std::vector<uint32_t> getRemoteServers();
	virtual bool markProcessed(uint32_t server_id, uint32_t message_id);
	virtual bool addServer(const string& name, uint32_t id);
	virtual bool addUser(const std::string& name, const std::string& pass,
			uint32_t id, uint32_t type);
	virtual int authenticate(const std::string& uname, const std::string& pass,
			uint32_t *user_id, uint32_t *user_type);
	virtual bool setPassword(uint32_t user_id, const std::string& newpass);
	virtual MessageList getUnprocessedMessages();
	virtual MessageList getUnacked(uint32_t server_id, uint32_t message_type_id,
			uint32_t limit = 0);
	virtual void ackMessage(uint32_t server_id, uint32_t message_id,
			uint32_t ack_server);
	virtual bool hasMessage(uint32_t server_id, uint32_t message_id);
	virtual uint32_t maxServerId();
	virtual uint32_t maxUserId();
	virtual ProblemList getProblems();
	virtual time_t getProblemUpdateTime(uint32_t problem_id);
	virtual bool setProblemUpdateTime(uint32_t problem_id, time_t time);
	virtual string getProblemType(uint32_t problem_id);
	virtual bool setProblemType(uint32_t problem_id, string type);
	virtual AttributeList getProblemAttributes(uint32_t problem_id);
	virtual bool setProblemAttribute(uint32_t problem_id, std::string attr,
			int32_t value);
	virtual bool setProblemAttribute(uint32_t problem_id, std::string attr,
			std::string value);
	virtual bool setProblemAttribute(uint32_t problem_id, std::string attr,
			std::string fname, const uint8_t *data, uint32_t datalen);
	virtual bool delProblemAttribute(uint32_t problem_id, std::string attr);
	virtual bool getProblemFileData(uint32_t problem_id, std::string attr,
			uint8_t **dataptr, uint32_t *lenptr);
	
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
		server_id << " AND attribute='" << escape_string(attribute) << "'";
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

	if(mysql_query(&_mysql, "LOCK TABLES PeerMessage WRITE, PeerMessageNoAck WRITE, Server READ")) {
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

	query.str(""); 
	query << "INSERT INTO PeerMessageNoAck(server_id, message_id, "
		"ack_server_id, lastsent) SELECT " << message->server_id() <<
		", " << message->message_id() << ", server_id, 0 FROM Server WHERE "
		"server_id != " << Server::getId();
	
	if(mysql_query(&_mysql, query.str().c_str()))
		log_mysql_error();
	
	mysql_query(&_mysql, "UNLOCK TABLES");

	return true;
}

bool MySQL::putRemoteMessage(const Message* message) {
	const uint8_t *blob;
	uint32_t blobsize;
	ostringstream query;
	
	if(!message->getData(&blob, &blobsize))
		return false;

	query << "INSERT INTO PeerMessage(server_id, message_id, message_type_id,"
		" time, signature, data, processed) VALUES(" << message->server_id()
		<< ", " << message->message_id() << ", " << message->message_type_id()
		<< ", " << message->time() << ", '"
		<< escape_buffer(message->getSignature(), MESSAGE_SIGNATURE_SIZE)
		<< "', '" << escape_buffer(blob, blobsize) << "', 0)";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}

	query.str("");
	query << "INSERT INTO PeerMessageNoAck(server_id, message_id, "
		"ack_server_id, lastsent) SELECT " << message->server_id() <<
		", " << message->message_id() << ", server_id, 0 FROM Server WHERE "
		"server_id != " << Server::getId() << " AND server_id != " <<
		message->server_id();

	if(mysql_query(&_mysql, query.str().c_str()))
		log_mysql_error();

	return true;
}
	
std::vector<uint32_t> MySQL::getRemoteServers() {
	ostringstream query;
	query << "SELECT server_id FROM Server WHERE server_id != "
		<< Server::getId();

	vector<uint32_t> remservers;
	MYSQL_RES *res;
	
	if(mysql_query(&_mysql, query.str().c_str())) {
		// this is extremely bad!
		log_mysql_error();
	} else if(!(res = mysql_store_result(&_mysql))) {
		log_mysql_error();
	} else {
		MYSQL_ROW row;
		while((row = mysql_fetch_row(res)) != 0) {
			remservers.push_back(atol(row[0]));
		}
	}

	return remservers;
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

	if(id != Server::getId()) {
		query.str("");
		query << "INSERT INTO PeerMessageNoAck(server_id, message_id, ack_server_id, lastsent) SELECT server_id, message_id, " << id << ", 0 FROM PeerMessage";

		if(mysql_query(&_mysql, query.str().c_str())) {
			log_mysql_error();
			// return false;
		}
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

MessageList MySQL::getMessages(std::string query) {
	MessageList msglist;
	MYSQL_RES *res;
	if(mysql_query(&_mysql, query.c_str())) {
		log_mysql_error();
	} else if(!(res = mysql_use_result(&_mysql))) {
		log_mysql_error();
	} else {
		MYSQL_ROW row;
		while((row = mysql_fetch_row(res)) != NULL) {
			long datsize = atol(row[6]);
			uint8_t* sig = new uint8_t[MESSAGE_SIGNATURE_SIZE];
			uint8_t* data = new uint8_t[datsize];
			memcpy(sig, row[4], MESSAGE_SIGNATURE_SIZE);
			memcpy(data, row[5], datsize);
			Message* tmp = Message::buildMessage(
					atol(row[0]),
					atol(row[1]),
					atol(row[2]),
					atol(row[3]),
					sig,
					data,
					datsize
				);

			if(!tmp) {
				delete []sig;
				delete []data;
				log(LOG_CRIT, "This should not be happening!  Unable to construct message from DB");
			} else {
				msglist.push_back(tmp);
			}
		}
		mysql_free_result(res);
	}
	
	return msglist;
}

MessageList MySQL::getUnprocessedMessages() {
	return getMessages("SELECT server_id, message_id, message_type_id, "
		"time, signature, data, length(data) FROM PeerMessage WHERE "
		"processed = 0 ORDER BY time");
}

MessageList MySQL::getUnacked(uint32_t server_id, uint32_t message_type_id, uint32_t limit) {
	ostringstream query;
	query << "SELECT PeerMessage.server_id, PeerMessage.message_id, "
		"message_type_id, time, signature, data, length(data) FROM "
		"PeerMessage, PeerMessageNoAck WHERE "
		"PeerMessage.server_id = PeerMessageNoAck.server_id AND "
		"PeerMessage.message_id = PeerMessageNoAck.message_id AND "
		"PeerMessageNoAck.ack_server_id = " << server_id;

	if(message_type_id)
		query << " AND PeerMessage.message_type_id = " << message_type_id;
	
	query << " ORDER BY time";

	if(limit)
		query << " LIMIT " << limit;
	
	return getMessages(query.str());
}

void MySQL::ackMessage(uint32_t server_id, uint32_t message_id,
		uint32_t ack_server) {
	ostringstream query;
	query << "DELETE FROM PeerMessageNoAck WHERE server_id=" << server_id <<
		" AND message_id=" << message_id << " AND ack_server_id=" <<
		ack_server;
	
	if(mysql_query(&_mysql, query.str().c_str()))
		log_mysql_error();
}
	
bool MySQL::hasMessage(uint32_t server_id, uint32_t message_id) {
	ostringstream query;
	query << "SELECT processed FROM PeerMessage WHERE server_id=" <<
		server_id << " AND message_id=" << message_id;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}

	MYSQL_RES *res = mysql_store_result(&_mysql);
	if(res) {
		MYSQL_ROW row = mysql_fetch_row(res);
		bool result = row != NULL;
		mysql_free_result(res);
		return result;
	} else {
		log_mysql_error();
		return false;
	}
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

ProblemList MySQL::getProblems() {
	ProblemList result;
	if(mysql_query(&_mysql, "SELECT * FROM Problem"))
		log_mysql_error();
	else {
		MYSQL_RES *res = mysql_use_result(&_mysql);
		if(res) {
			MYSQL_ROW row;
			while((row = mysql_fetch_row(res)) != 0) {
				result.push_back(atol(row[0]));
			}
		}
		mysql_free_result(res);
	}
	return result;
}

time_t MySQL::getProblemUpdateTime(uint32_t problem_id) {
	ostringstream query;
	query << "SELECT updated FROM Problem WHERE problem_id=" << problem_id;
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return ~0U;
	} else {
		time_t result = ~0U;
		MYSQL_RES *res = mysql_use_result(&_mysql);
		if(res) {
			MYSQL_ROW row = mysql_fetch_row(res);
			if(row)
				result = atoll(row[0]);
			mysql_free_result(res);
		}
		return result;
	}
}

bool MySQL::setProblemUpdateTime(uint32_t problem_id, time_t time) {
	ostringstream query;
	query << "UPDATE Problem SET updated=" << time << " WHERE problem_id=" <<
		problem_id;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	} else
		return true;
}
	
string MySQL::getProblemType(uint32_t problem_id) {
	ostringstream query;
	query << "SELECT type FROM Problem WHERE problem_id=" << problem_id;
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return "";
	}

	MYSQL_RES *res = mysql_use_result(&_mysql);
	if(!res)
		return "";

	string result;
	
	MYSQL_ROW row = mysql_fetch_row(res);
	if(row)
		result = row[0];
	mysql_free_result(res);

	return result;
}

bool MySQL::setProblemType(uint32_t problem_id, string type) {
	ostringstream query;
	type = escape_string(type);
	query << "INSERT INTO Problem (problem_id, type) VALUES(" <<
		problem_id << ", '" << type << "') ON DUPLICATE KEY UPDATE "
		"type='" << type << "'";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}

	return true;
}

AttributeList MySQL::getProblemAttributes(uint32_t problem_id) {
	ostringstream query;
	query << "SELECT attribute, value FROM ProblemAttributes WHERE problem_id="
		<< problem_id;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return AttributeList();
	}

	AttributeList attrs;
	MYSQL_RES *res = mysql_use_result(&_mysql);
	MYSQL_ROW row;
	while(0 != (row = mysql_fetch_row(res))) {
		attrs[row[0]] = row[1];
	}
	mysql_free_result(res);

	return attrs;
}

bool MySQL::setProblemAttribute(uint32_t problem_id, std::string attr, int32_t value) {
	ostringstream tmp;
	tmp << value;
	return setProblemAttribute(problem_id, attr, tmp.str());
}

bool MySQL::setProblemAttribute(uint32_t problem_id, std::string attr, std::string value) {
	if(!delProblemAttribute(problem_id, attr))
		return false;
	
	ostringstream query;
	query << "INSERT INTO ProblemAttributes (problem_id, attribute, value)"
		" VALUES(" << problem_id << ", '" << escape_string(attr) << "', '"
		<< escape_string(value) << "')";
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}

	return true;
}

bool MySQL::setProblemAttribute(uint32_t problem_id, std::string attr, std::string fname, const uint8_t *data, uint32_t datalen) {
	if(!setProblemAttribute(problem_id, attr, fname))
		return false;

	ostringstream query;
	query << "INSERT INTO ProblemFileData (problem_id, attribute, data)"
		" VALUES(" << problem_id << ", '" << escape_string(attr) << "', '" <<
		escape_buffer(data, datalen) << "')";
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}

	return true;
}

bool MySQL::delProblemAttribute(uint32_t problem_id, std::string attr) {
	ostringstream query;
	query << "DELETE FROM ProblemAttributes WHERE problem_id=" << problem_id <<
		" AND attribute='" << escape_string(attr) << "'";
	
	int r1 = mysql_query(&_mysql, query.str().c_str());
	if(r1)
		log_mysql_error();
	
	query.str("");
	
	query << "DELETE FROM ProblemFileData WHERE problem_id=" << problem_id <<
		" AND attribute='" << escape_string(attr) << "'";
	int r2 = mysql_query(&_mysql, query.str().c_str());
	if(r2)
		log_mysql_error();

	return r1 == 0 && r2 == 0;
}

bool MySQL::getProblemFileData(uint32_t problem_id, std::string attr, uint8_t **dataptr, uint32_t *lenptr) {
	*dataptr = NULL;
	*lenptr = 0;

	ostringstream query;
	query << "SELECT data, length(data) FROM ProblemFileData WHERE attribute='"
		<< escape_string(attr) << "' AND problem_id=" << problem_id;
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	} else {
		MYSQL_RES *res = mysql_use_result(&_mysql);
		if(res) {
			MYSQL_ROW row = mysql_fetch_row(res);
			if(row) {
				*lenptr = atol(row[1]);
				*dataptr = new uint8_t[*lenptr];
				memcpy(*dataptr, row[0], *lenptr);
			}
			mysql_free_result(res);
		}
	}
	return *dataptr != NULL;
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

/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "dbcon.h"
#include "logger.h"
#include "config.h"
#include "message.h"
#include "server.h"

#include <mysql/mysql.h>
#include <sstream>
#include <time.h>

#define log_mysql_error()	log(LOG_ERR,"%s line %d: %s", __PRETTY_FUNCTION__, __LINE__, mysql_error(&_mysql))

using namespace std;

class MySQL : public DbCon {
private:
	MYSQL _mysql;

	list<Message*> getMessages(std::string query);
public:
	MySQL();
	virtual ~MySQL();

	virtual bool ok();
	virtual QueryResult multiRowQuery(std::string query);
	virtual QueryResultRow singleRowQuery(std::string query);
	virtual bool executeQuery(std::string query);
	virtual string escape_string(const string& str);
	virtual string escape_buffer(const uint8_t* bfr, uint32_t size);

	// from here will eventually go bye bye.
	virtual uint32_t name2server_id(const string& name);
	virtual std::string server_id2name(uint32_t user_id);
	virtual uint32_t name2user_id(const string& name);
	virtual std::string user_id2name(uint32_t user_id);
	//virtual uint32_t user_id2type(uint32_t user_id);
	virtual UserList getUsers();
	virtual ServerList getServers();
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
	virtual uint32_t maxSubmissionId();
	virtual uint32_t maxClarificationReqId();
	virtual uint32_t maxClarificationId();
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
	virtual bool putSubmission(uint32_t submission_id, uint32_t user_id, uint32_t prob_id,
			uint32_t time, uint32_t server_id, char* content,
			uint32_t content_size, std::string language);
	virtual SubmissionList getSubmissions(uint32_t uid);
	virtual ClarificationList getClarifications(uint32_t uid);
	virtual ClarificationRequestList getClarificationRequests(uint32_t uid);
        virtual AttributeList getClarificationRequest(uint32_t req_id);
	virtual bool putClarificationRequest(uint32_t cr_id, uint32_t user_id, uint32_t prob_id,
					     uint32_t time, uint32_t server_id,
					     const std::string& question);
	virtual bool putClarification(uint32_t cr_id, uint32_t c_id,
				      uint32_t user_id, uint32_t time,
				      uint32_t server_id, uint32_t pub,
				      const std::string& answer);
	virtual bool retrieveSubmission(uint32_t sub_id, char** buffer, int *length,
			string& language, uint32_t* prob_id);
	virtual IdList getUnmarked(uint32_t server_id);
	virtual bool putMark(uint32_t submission_id, uint32_t marker_id,
			uint32_t time, uint32_t result, std::string comment, uint32_t server_id);
	virtual bool putMarkFile(uint32_t submission_id, uint32_t marker_id,
			std::string name, const void* data, uint32_t len);
	virtual uint32_t countMarkFiles(uint32_t submission_id);
	virtual bool getMarkFile(uint32_t submission_id, uint32_t file_index, std::string &name, char **data, uint32_t &length);
	virtual bool getSubmissionState(uint32_t submission_id, RunResult& state, uint32_t& utype, string& comment);
	virtual uint32_t submission2user_id(uint32_t submission_id);
	virtual uint32_t submission2server_id(uint32_t submission_id);
	virtual std::string submission2problem(uint32_t submission_id);
	virtual bool hasSolved(uint32_t user_id, uint32_t prob_id);
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

QueryResult MySQL::multiRowQuery(std::string query) {
	if(!executeQuery(query))
		return QueryResult();

	MYSQL_RES *res = mysql_use_result(&_mysql);
	if(!res) {
		log_mysql_error();
		return QueryResult();
	}

	unsigned int fieldcount = mysql_field_count(&_mysql);
	if(!fieldcount)
		return QueryResult();

	MYSQL_ROW row = mysql_fetch_row(res);

	QueryResult queryresult;
	while(row) {
		QueryResultRow resrow;
		resrow.reserve(fieldcount);
		for(unsigned int i = 0; i < fieldcount; ++i)
			resrow.push_back(row[i]);
		queryresult.push_back(resrow);

		row = mysql_fetch_row(res);
	}

	mysql_free_result(res);

	return queryresult;
}

QueryResultRow MySQL::singleRowQuery(std::string query) {
	if(!executeQuery(query))
		return QueryResultRow();

	MYSQL_RES *res = mysql_use_result(&_mysql);
	if(!res) {
		log_mysql_error();
		return QueryResultRow();
	}

	unsigned int fieldcount = mysql_field_count(&_mysql);
	if(!fieldcount)
		return QueryResultRow();

	MYSQL_ROW row = mysql_fetch_row(res);

	QueryResultRow resrow;

	if(row) {
		resrow.reserve(fieldcount);
		for(unsigned int i = 0; i < fieldcount; ++i)
			resrow.push_back(row[i]);

		// have to flush the entire result.
		while(mysql_fetch_row(res));
	}

	mysql_free_result(res);

	return resrow;
}

bool MySQL::executeQuery(std::string query) {
	if(mysql_query(&_mysql, query.c_str())) {
		log_mysql_error();
		return false;
	}
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

std::string MySQL::server_id2name(uint32_t server_id) {
	string servername;
	ostringstream query;
	query << "SELECT server_name FROM Server WHERE server_id=" << server_id;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return "";
	}

	MYSQL_RES *res = mysql_use_result(&_mysql);
	if(!res) {
		log_mysql_error();
		return "";
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	if(row)
		servername = row[0];

	mysql_free_result(res);

	return servername;
}

uint32_t MySQL::name2user_id(const string& name) {
	uint32_t user_id = 0;
	ostringstream query;
	query << "SELECT user_id FROM User WHERE username='" << escape_string(name) << "'";

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
		user_id = atol(row[0]);

	mysql_free_result(res);

	return user_id;
}

std::string MySQL::user_id2name(uint32_t user_id) {
	string username;
	ostringstream query;
	query << "SELECT username FROM User WHERE user_id=" << user_id;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return "";
	}

	MYSQL_RES *res = mysql_use_result(&_mysql);
	if(!res) {
		log_mysql_error();
		return "";
	}

	MYSQL_ROW row = mysql_fetch_row(res);
	if(row)
		username = row[0];

	mysql_free_result(res);

	return username;
}

UserList MySQL::getUsers() {
	ostringstream query;
	query << "SELECT user_id, username FROM User";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return ClarificationList();
	}

	UserList lst;
	MYSQL_RES *res = mysql_use_result(&_mysql);
	if (!res) {
		log_mysql_error();
		return UserList();
	}
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(res)) != 0) {
		AttributeList attrs;
		attrs["id"] = row[0];
		attrs["username"] = row[1];

		lst.push_back(attrs);
	}
	mysql_free_result(res);

	return lst;
}

ServerList MySQL::getServers() {
	ServerList list;

	if(mysql_query(&_mysql, "SELECT server_id, server_name FROM Server")) {
		log_mysql_error();
	} else {
		MYSQL_RES *res = mysql_use_result(&_mysql);
		if(res) {
			MYSQL_ROW row;
			while(NULL != (row = mysql_fetch_row(res))) {
				list[atol(row[0])] = row[1];
			}
			mysql_free_result(res);
		}
	}

	return list;
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
		mysql_free_result(res);
	}

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
        mysql_free_result(res);
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

uint32_t MySQL::maxSubmissionId() {
	uint32_t max_user_id = ~0U;
	ostringstream query;
	query << "SELECT MAX(submission_id) FROM Submission WHERE submission_id & " << (ID_GRANULARITY - 1) << " = " << Server::getId();
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

uint32_t MySQL::maxClarificationReqId() {
	uint32_t max_user_id = ~0U;
	ostringstream query;
	query << "SELECT MAX(clarification_req_id) FROM ClarificationRequest WHERE clarification_req_id & " << (ID_GRANULARITY - 1) << " = " << Server::getId();
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

uint32_t MySQL::maxClarificationId() {
	uint32_t max_user_id = ~0U;
	ostringstream query;
	query << "SELECT MAX(clarification_id) FROM Clarification WHERE clarification_id & " << (ID_GRANULARITY - 1) << " = " << Server::getId();
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
            		mysql_free_result(res);
		}
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

bool MySQL::putSubmission(uint32_t submission_id, uint32_t user_id, uint32_t prob_id, uint32_t time, uint32_t server_id, char* content, uint32_t content_size, std::string language) {
	ostringstream query;
	query << "INSERT INTO Submission (submission_id, user_id, prob_id, time, server_id, content, language) VALUES(" << submission_id << ", " << user_id << ", " << prob_id << ", " << time << ", " << server_id << ", '" << escape_buffer((uint8_t*)content, content_size) << "', '" << escape_string(language) << "')";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}

	return true;
}

SubmissionList MySQL::getSubmissions(uint32_t uid) {
	ostringstream query;
	query << "SELECT submission_id, time, value, Submission.prob_id FROM User, ProblemAttributes, Submission WHERE User.user_id = Submission.user_id AND Submission.prob_id = ProblemAttributes.problem_id AND ProblemAttributes.attribute = 'shortname'";

	if(uid)
		query << " AND User.user_id = " << uid;

	query << " ORDER BY time";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return SubmissionList();
	}

	SubmissionList lst;

    MYSQL_RES *res = mysql_use_result(&_mysql);
    if (!res) {
        log_mysql_error();
        return SubmissionList();
    }
	MYSQL_ROW row;
	while((row = mysql_fetch_row(res)) != 0) {
		AttributeList attrs;
		attrs["submission_id"] = row[0];
		attrs["time"] = row[1];
		attrs["problem"] = row[2];
		attrs["prob_id"] = row[3];

		lst.push_back(attrs);
	}
	mysql_free_result(res);

	return lst;
}

ClarificationList MySQL::getClarifications(uint32_t uid) {
	ostringstream query;
	query << "SELECT Clarification.clarification_id, username, value, Clarification.time, ClarificationRequest.text, Clarification.text FROM Clarification LEFT JOIN ClarificationRequest USING(clarification_req_id) LEFT JOIN User ON Clarification.user_id = User.user_id LEFT OUTER JOIN ProblemAttributes ON ProblemAttributes.problem_id = ClarificationRequest.problem_id AND ProblemAttributes.attribute = 'shortname'";
	if (uid)
		query << " WHERE Clarification.public != 0 OR ClarificationRequest.user_id = " << uid;
	query << " ORDER BY TIME";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return ClarificationList();
	}

	ClarificationList lst;
	MYSQL_RES *res = mysql_use_result(&_mysql);
	if (!res) {
		log_mysql_error();
		return ClarificationList();
	}
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(res)) != 0) {
		AttributeList attrs;
		attrs["id"] = row[0];
		/* Leave out Judge for now; can be added as row[1] later */
		attrs["problem"] = row[2] ? row[2] : "General";
		attrs["time"] = row[3];
		attrs["question"] = row[4];
		attrs["answer"] = row[5];

		lst.push_back(attrs);
	}
	mysql_free_result(res);

	return lst;
}

AttributeList MySQL::getClarificationRequest(uint32_t req_id) {
	ostringstream query;
	query << "SELECT ClarificationRequest.clarification_req_id, ClarificationRequest.user_id, username, value, ClarificationRequest.time, ClarificationRequest.text AS question, COUNT(clarification_id) AS answers FROM ClarificationRequest LEFT OUTER JOIN Clarification USING(clarification_req_id) LEFT JOIN User ON ClarificationRequest.user_id = User.user_id LEFT OUTER JOIN ProblemAttributes ON ClarificationRequest.problem_id = ProblemAttributes.problem_id AND ProblemAttributes.attribute='shortname'";
	query << " WHERE ClarificationRequest.clarification_req_id = " << req_id;
	query << " GROUP BY ClarificationRequest.clarification_req_id";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return AttributeList();
	}

        AttributeList attrs;
	MYSQL_RES *res = mysql_use_result(&_mysql);
	if (!res) {
		log_mysql_error();
		return AttributeList();
	}
	MYSQL_ROW row = mysql_fetch_row(res);
	if (row)
	{
		attrs["id"] = row[0];
		attrs["user_id"] = row[1];
		/* Leave out contestant for now; can be used later */
		attrs["problem"] = row[3] ? row[3] : "General";
		attrs["time"] = row[4];
		attrs["question"] = row[5];
		attrs["status"] = atoi(row[6]) ? "answered" : "pending";
		while (mysql_fetch_row(res));
	}
	mysql_free_result(res);

	return attrs;
}

ClarificationRequestList MySQL::getClarificationRequests(uint32_t uid) {
	ostringstream query;

	query << "SELECT ClarificationRequest.clarification_req_id, ClarificationRequest.user_id, username, value, ClarificationRequest.time, ClarificationRequest.text AS question, COUNT(clarification_id) AS answers FROM ClarificationRequest LEFT OUTER JOIN Clarification USING(clarification_req_id) LEFT JOIN User ON ClarificationRequest.user_id = User.user_id LEFT OUTER JOIN ProblemAttributes ON ClarificationRequest.problem_id = ProblemAttributes.problem_id AND ProblemAttributes.attribute='shortname'";
	if (uid) query << " WHERE ClarificationRequest.user_id = " << uid;
	query << " GROUP BY ClarificationRequest.clarification_req_id ORDER BY ClarificationRequest.time";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return ClarificationRequestList();
	}

	ClarificationRequestList lst;
	MYSQL_RES *res = mysql_use_result(&_mysql);
	if (!res) {
		log_mysql_error();
		return ClarificationRequestList();
	}
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(res)) != 0) {
		AttributeList attrs;
		attrs["id"] = row[0];
		attrs["user_id"] = row[1];
		/* Leave out contestant for now; can be used later */
		attrs["problem"] = row[3] ? row[3] : "General";
		attrs["time"] = row[4];
		attrs["question"] = row[5];
		attrs["status"] = atoi(row[6]) ? "answered" : "pending";

		lst.push_back(attrs);
	}
	mysql_free_result(res);

	return lst;
}

bool MySQL::putClarificationRequest(uint32_t cr_id, uint32_t user_id, uint32_t prob_id,
				    uint32_t time, uint32_t /* server_id */,
				    const std::string& question) {
	ostringstream query;
	query << "INSERT INTO ClarificationRequest (clarification_req_id, user_id, problem_id, time, text)";
	query << " VALUES (" << cr_id << ", " << user_id << ", " << prob_id << ", " << time << ", '" << escape_string(question) << "')";

	if (mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}

	return true;
}

bool MySQL::putClarification(uint32_t cr_id, uint32_t c_id,
			     uint32_t user_id, uint32_t time,
			     uint32_t /* server_id */, uint32_t pub,
			     const std::string& answer) {
	ostringstream query;
	query << "INSERT INTO Clarification (clarification_id, clarification_req_id, user_id, time, public, text)";
	query << " VALUES (" << c_id << ", " << cr_id << ", " << user_id << ", " << time << ", " << (pub ? 1 : 0) << ", '" << escape_string(answer) << "')";

	if (mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	}

	return true;
}

bool MySQL::retrieveSubmission(uint32_t sub_id, char** buffer, int *length, string& language, uint32_t* prob_id) {
	ostringstream query;
	query << "SELECT content, LENGTH(content), language, prob_id FROM Submission WHERE "
		"submission_id = " << sub_id;

	*buffer = NULL;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
	} else {
		MYSQL_RES *res = mysql_use_result(&_mysql);
		if(!res) {
			log_mysql_error();
		} else {
			MYSQL_ROW row;
			if((row = mysql_fetch_row(res)) != NULL) {
				*length = strtoll(row[1], NULL, 10);
				*buffer = new char[*length];
				memcpy(*buffer, row[0], *length);
				language = row[2];
				if(prob_id)
					*prob_id = strtoll(row[3], NULL, 0);
			}
			mysql_free_result(res);
		}
	}

	return *buffer != NULL;
}

IdList MySQL::getUnmarked(uint32_t server_id) {
	ostringstream query;
	query << "SELECT submission_id FROM Submission WHERE (SELECT COUNT(*) FROM SubmissionMark WHERE SubmissionMark.submission_id=Submission.submission_id AND server_id=" << server_id << ") = 0 ORDER BY time";

	MYSQL_RES *res;
	IdList result;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
	} else if(!(res = mysql_use_result(&_mysql))) {
		log_mysql_error();
	} else {
		MYSQL_ROW row;
		while((row = mysql_fetch_row(res)) != NULL) {
			result.push_back(strtoll(row[0], NULL, 0));
		}
		mysql_free_result(res);
	}

	return result;
}
// SELECT user_id, prob_id, time, (SELECT correct FROM SubmissionMark WHERE Submission.user_id = Submission.user_id AND Submission.prob_id = SubmissionMark.prob_id AND Submission.time = SubmissionMark.time AND server_id = 1) AS mark FROM Submission HAVING IsNull(mark);

bool MySQL::putMark(uint32_t submission_id, uint32_t marker_id, uint32_t time, uint32_t result, std::string comment, uint32_t server_id) {
	ostringstream query;
	query << "INSERT INTO SubmissionMark (submission_id, marker_id, mark_time, result, remark, server_id) VALUES("
        << submission_id << ", " << marker_id << ", " << time << ", " << result << ", '" << escape_string(comment) << "', " << server_id << ")";
    if (true)
        query << " ON DUPLICATE KEY UPDATE mark_time=" << time << ",result=" << result << ",remark='" << escape_string(comment) << "',server_id=" << server_id;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	} else
		return true;
}

bool MySQL::putMarkFile(uint32_t submission_id, uint32_t marker_id,
			std::string name, const void* data, uint32_t len) {
	ostringstream query;
	query << "INSERT INTO SubmissionMarkFile (submission_id, marker_id, "
		"name, content) VALUES(" << submission_id << ", " << marker_id <<
		", '" << escape_string(name) << "', '" <<
		escape_buffer((const uint8_t*)data, len) << "')";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	} else
		return true;
}

uint32_t MySQL::countMarkFiles(uint32_t submission_id) {
    ostringstream query;
    query << "SELECT COUNT(*) FROM SubmissionMarkFile where submission_id=" << submission_id;

    if (mysql_query(&_mysql, query.str().c_str())) {
        log_mysql_error();
        return 0;
    } else {
        MYSQL_RES *res = mysql_use_result(&_mysql);
        if (!res) {
            log_mysql_error();
            return 0;
        }

        MYSQL_ROW row = mysql_fetch_row(res);

        uint32_t result;

        if (row)
            result = strtoll(row[0], NULL, 0);
        else
            result = 0;
        mysql_free_result(res);
        return result;
    }
}

bool MySQL::getMarkFile(uint32_t submission_id, uint32_t file_index, std::string &name, char **data, uint32_t &length) {
    ostringstream query;
    query << "SELECT name, content FROM SubmissionMarkFile where submission_id=" << submission_id;

    if (mysql_query(&_mysql, query.str().c_str())) {
        log_mysql_error();
        return 0;
    } else {
        MYSQL_RES *res = mysql_use_result(&_mysql);
        if (!res) {
            log_mysql_error();
            return false;
        }

        MYSQL_ROW row;
        uint32_t cur = 0;
        while ((row = mysql_fetch_row(res)) != NULL && cur < file_index)
            cur++;

        if (row == NULL) {
            log_mysql_error();
            mysql_free_result(res);
            return false;
        }

        name = row[0];
        unsigned long *lengths = mysql_fetch_lengths(res);
        *data = new char[lengths[1]];
        memcpy(*data, row[1], lengths[1]);
        length = lengths[1];

        if (row != NULL)
            while (mysql_fetch_row(res) != NULL)
                ;

        mysql_free_result(res);
        return true;
    }
}

bool MySQL::getSubmissionState(uint32_t submission_id, RunResult& state, uint32_t &utype, string& comment) {
	ostringstream query;
	query << "SELECT result, remark, type FROM SubmissionMark, User WHERE marker_id = user_id AND submission_id=" << submission_id << " ORDER BY type LIMIT 1";

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return false;
	} else {
		MYSQL_RES* res = mysql_use_result(&_mysql);
		if(!res) {
			log_mysql_error();
			return false;
		}

		MYSQL_ROW row = mysql_fetch_row(res);
		bool result = false;

		if(row) {
			state = (RunResult)strtoll(row[0], NULL, 0);
			comment = row[1];
			utype = strtoll(row[2], NULL, 0);
			result = true;
		}

		mysql_free_result(res);
		return result;
	}
}

uint32_t MySQL::submission2user_id(uint32_t submission_id) {
	ostringstream query;
	query << "SELECT user_id FROM Submission WHERE submission_id = " << submission_id;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return ~0U;
	} else {
		MYSQL_RES* res = mysql_use_result(&_mysql);
		if(!res) {
			log_mysql_error();
			return ~0U;
		}

		uint32_t uid = ~0U;
		MYSQL_ROW row = mysql_fetch_row(res);
		if(row)
			uid = strtoll(row[0], NULL, 0);
		mysql_free_result(res);
		return uid;
	}
}

uint32_t MySQL::submission2server_id(uint32_t submission_id) {
	ostringstream query;
	query << "SELECT server_id FROM Submission WHERE submission_id = " << submission_id;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return ~0U;
	} else {
		MYSQL_RES* res = mysql_use_result(&_mysql);
		if(!res) {
			log_mysql_error();
			return ~0U;
		}

		uint32_t sid = ~0U;
		MYSQL_ROW row = mysql_fetch_row(res);
		if(row)
			sid = strtoll(row[0], NULL, 0);
		mysql_free_result(res);
		return sid;
	}
}

std::string MySQL::submission2problem(uint32_t submission_id) {
	ostringstream query;
	query << "SELECT value FROM Submission, ProblemAttributes WHERE prob_id = problem_id AND attribute='shortname' AND submission_id=" << submission_id;

	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
		return "";
	} else {
		MYSQL_RES* res = mysql_use_result(&_mysql);
		if(!res) {
			log_mysql_error();
			return "";
		}

		string problem = "";
		MYSQL_ROW row = mysql_fetch_row(res);
		if(row)
			problem = row[0];
		mysql_free_result(res);
		return problem;
	}
}

bool MySQL::hasSolved(uint32_t user_id, uint32_t prob_id) {
	ostringstream query;
	query << "SELECT user_id, prob_id FROM Submission, SubmissionMark WHERE "
		"Submission.submission_id = SubmissionMark.submission_id AND result = 0 AND user_id = "
		<< user_id << " AND prob_id = " << prob_id;

	bool solved = false;
	if(mysql_query(&_mysql, query.str().c_str())) {
		log_mysql_error();
	} else {
		MYSQL_RES *res = mysql_use_result(&_mysql);
		if(!res)
			log_mysql_error();
		else {
			MYSQL_ROW row = mysql_fetch_row(res);
			if(row)
				solved = atoll(row[0]) != 0;
			mysql_free_result(res);
		}
	}

	return solved;
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
	if(!res) {
		log_mysql_error();
		return "";
	}

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

	MYSQL_RES *res = mysql_use_result(&_mysql);

	if(!res) {
		log_mysql_error();
		return AttributeList();
	}

	AttributeList attrs;

	if (!res) {
		log_mysql_error();
		return AttributeList();
	}
	MYSQL_ROW row;
	while(NULL != (row = mysql_fetch_row(res))) {
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
	mysql_server_init(0, NULL, NULL);
	DbCon::registerFunctor(MySQLFunctor);
}

static void deinit() __attribute__((destructor));
static void deinit() {
	mysql_server_end();
}

/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __DBCON_H__
#define __DBCON_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <string>
#include <vector>
#include <list>
#include <map>
#include <stdint.h>

#include "misc.h"

class Message;

typedef std::list<Message*> MessageList;
typedef std::map<std::string, std::string> StringMap;
typedef StringMap AttributeList;
typedef std::list<uint32_t> IdList;
typedef IdList ProblemList;
typedef std::map<uint32_t, std::string> ServerList;
typedef std::list<AttributeList> SubmissionList;
typedef std::list<AttributeList> ClarificationList;
typedef std::list<AttributeList> ClarificationRequestList;

typedef std::vector<std::string> QueryResultRow;
typedef std::list<QueryResultRow> QueryResult;

typedef bool (*QueryCallback)(void* ctx, QueryResultRow&);

class DbCon {
public:
	DbCon();
	virtual ~DbCon();

	/**
	 * Checks that the connection is ok.  IE: in a working condition.
	 */
	virtual bool ok() = 0;

	/**
	 * Executes a SQL query that returns multiple rows as a list of string
	 * vectors.  Any conversion needs to other datatypes should be made by the
	 * caller.
	 */
	virtual QueryResult multiRowQuery(const std::string& query) = 0;

	/**
	 * Executes a SQL query and returns the first row of the result from that
	 * query.  Please note that most database engines needs to go through the
	 * entire result anyway, so you should probably try to make sure that the
	 * query only returns a single row.
	 */
	virtual QueryResultRow singleRowQuery(const std::string& query) = 0;

	/**
	 * Execute a query that does _not_ generate a result.  This is typically
	 * used to insert now data into the database.
	 */
	virtual bool executeQuery(const std::string& query) = 0;

	/**
	 * Escape a string.
	 */
	virtual std::string escape_string(const std::string& str) = 0;

	/**
	 * Escape a buffer.
	 */
	virtual std::string escape_buffer(const uint8_t* bfr, uint32_t size) = 0;

	/**
	 * Specify a query which will call a callback function for every row in the
	 * result.  the function takes a void*, which is the parameter ctx as
	 * passed to queryCallback(), and a QueryResultRow, which is a vector of
	 * strings.  The return value of the callback is a bool, indicating whether
	 * the callback is interrested in further results.
	 */
//	virtual void queryCallback(std::string query, QueryCallback cb, void* ctx = NULL) = 0;

	/**
	 * From here downwards should be eliminated as time goes by.  Having all the
	 * SQL in (almost) one place seemed a good idea at the time.  The reality is
	 * that it's not.  No matter what they teach you at varsity, it's better to
	 * create modules with SQL code in them rather than have all the SQL in one
	 * place.  Also, seperate modules for DB connectivity should _only_ contain
	 * semantics for different database engines (DAO is not such a bad idea after
	 * all).
	 */


	/**
	 * Retrieves a list of all servers, mapped with id -> name
	 */
	virtual ServerList getServers() = 0;

	/**
	 * Retrieves an attribute value for a server.
	 */
	virtual std::string getServerAttribute(uint32_t server_id, const std::string& attribute) = 0;

	/**
	 * Sets an attribute (setting an attribute to "" should delete it).
	 */
	virtual bool setServerAttribute(uint32_t server_id, const std::string& attribute, const std::string& value) = 0;

	/**
	 * Function to insert locally generated messages, ie, this _must_ generate
	 * a message_id, call Message::setMessageId() _and_ then call
	 * Message::getSignature() and insert that into the DB too.
	 */
	virtual bool putLocalMessage(Message *message) = 0;

	/**
	 * This function gets called to insert remotely received messages only.
	 * This means the DB needs to insert all fields in one go.
	 */
	virtual bool putRemoteMessage(const Message* message) = 0;

	/**
	 * This function is used to mark a message as processed.  This means
	 * that it will not get loaded from the database for processing upon
	 * restart!
	 */
	virtual bool markProcessed(uint32_t server_id, uint32_t message_id) = 0;

	/**
	 * Used to add a new server to the system.
	 */
	virtual bool addServer(const std::string& name, uint32_t id) = 0;

	/**
	 * Retrieve a list of remote servers.  This must be a complete list
	 * of remote servers, excluding the local machine as indicated by
	 * Server::getId()
	 */
	virtual std::vector<uint32_t> getRemoteServers() = 0;

	/**
	 * Used to retrieve the current maximum server_id.
	 */
	virtual uint32_t maxServerId() = 0;

	/**
	 * Used to retrieve the max local user_id that is in use.
	 * This id needs to be either 0 or fall into
	 * Server::getId() + n * ID_GRANULARITY, with n a positive integer.
	 */
	virtual uint32_t maxUserId() = 0;

	/**
	 * Used to retrieve the max Submission Id.  See maxUserId.
	 */
	virtual uint32_t maxSubmissionId() = 0;

	/**
	 * Used to retrieve the max Clarification Request Id.  See maxUserId.
	 */
	virtual uint32_t maxClarificationReqId() = 0;

	/**
	 * Used to retrieve the max Clarification Id.  See maxUserId.
	 */
	virtual uint32_t maxClarificationId() = 0;

	/**
	 * Used to authenticate a user.  Return values should be as follows:
	 * <0	Error
	 * 0	Authentication failed
	 * >0	Authentication succeeded.
	 * On success the user_id and user_type should be stored in the
	 * appropriate variables, iff the pointers are non-null.
	 */
	virtual int authenticate(const std::string& uname, const std::string& pass,
			uint32_t *user_id, uint32_t *user_type) = 0;

	/**
	 * Used to retrieve all unprocessed messages.
	 */
	virtual MessageList getUnprocessedMessages() = 0;

	/**
	 * Function to retrieve a list of (all) unacked messages (for a particular
	 * server).  You can also specify a limit on the number of messages to
	 * receive.  The message_type_id parameter can be 0 to indicate any type
	 * of message.
	 */
	virtual MessageList getUnacked(uint32_t server_id, uint32_t message_type_id, uint32_t limit = 0) = 0;

	/**
	 * Call this function to ACK a message in the database.  This effectively
	 * dequeues it from being re-sent.
	 */
	virtual void ackMessage(uint32_t server_id, uint32_t message_id,
			uint32_t ack_server) = 0;

	/**
	 * This function queries whether we actually have a certain message
	 * committed to DB.
	 */
	virtual bool hasMessage(uint32_t server_id, uint32_t message_id) = 0;

	/**
	 * Retrieve a list of problem Ids.
	 */
	virtual ProblemList getProblems() = 0;

	/**
	 * Determine when last a problem got "updated".
	 */
	virtual time_t getProblemUpdateTime(uint32_t problem_id) = 0;

	/**
	 * Set the time of last problem update.  This can almost be considered
	 * a type of "version" number.
	 */
	virtual bool setProblemUpdateTime(uint32_t problem_id, time_t newtime) = 0;

	/**
	 * Retrieve/Set problem type.  If problem type cannot be updated, a
	 * new problem should be created.  In the case of newly created problems
	 * the last update time should be set to 0.
	 */
	virtual std::string getProblemType(uint32_t problem_id) = 0;
	virtual bool setProblemType(uint32_t problem_id, std::string type) = 0;

	/**
	 * Retrieve a list of attributes and their values (as strings) of all
	 * attributes.  In the case of "files" only the filename should be retrieved.
	 */
	virtual AttributeList getProblemAttributes(uint32_t problem_id) = 0;

	/**
	 * Update/Delete functions for problem attributes.
	 */
	virtual bool setProblemAttribute(uint32_t problem_id, std::string attr,
			int32_t value) = 0;
	virtual bool setProblemAttribute(uint32_t problem_id, std::string attr,
			std::string value) = 0;
	virtual bool setProblemAttribute(uint32_t problem_id, std::string attr,
			std::string fname, const uint8_t *data, uint32_t datalen) = 0;
	virtual bool delProblemAttribute(uint32_t problem_id, std::string attr) = 0;

	/**
	 * A way to retrieve the file-data.
	 */
	virtual bool getProblemFileData(uint32_t problem_id, std::string attr,
			uint8_t **dataptr, uint32_t *lenptr) = 0;

	/**
	 * Retrieve all submissions for a specific user.
	 */
	virtual SubmissionList getSubmissions(uint32_t user_id = 0) = 0;

	/**
	 * Retrieves all clarifications relevant to a specific user.
	 */
	virtual ClarificationList getClarifications(uint32_t user_id = 0) = 0;

        /**
         * Retrieves clarification with given ID. Returns empty list on
         * failure.
         */
        virtual AttributeList getClarificationRequest(uint32_t req_id) = 0;

	/**
	 * Retrieves all clarification requests relevant to a specific user.
	 */
	virtual ClarificationRequestList getClarificationRequests(uint32_t user_id = 0) = 0;

	/**
	 * Add a clarification request to the database
	 */
	virtual bool putClarificationRequest(uint32_t cr_id, uint32_t user_id, uint32_t prob_id,
					     uint32_t time, uint32_t server_id,
					     const std::string& question) = 0;

	/**
	 * Add a clarification to the database
	 */
	virtual bool putClarification(uint32_t cr_id, uint32_t c_id,
				      uint32_t user_id, uint32_t time,
				      uint32_t server_id, uint32_t pub,
				      const std::string& answer) = 0;

	/**
	 * Retrieve Submission content (and language).
	 */
	virtual bool retrieveSubmission(uint32_t submission_id, char** buffer, int *length, std::string& language, uint32_t* prob_id) = 0;

	virtual IdList getUnmarked(uint32_t server_id = 0) = 0;

	virtual bool putMark(uint32_t submission_id, uint32_t marker_id,
			uint32_t time, uint32_t result, std::string comment, uint32_t server_id) = 0;

	virtual bool putMarkFile(uint32_t submission_id, uint32_t marker_id,
			std::string name, const void* data, uint32_t len) = 0;

	virtual uint32_t countMarkFiles(uint32_t submission_id) = 0;

	virtual bool getMarkFile(uint32_t submission_id, uint32_t file_index, std::string &name, char **data, uint32_t &length) = 0;

	virtual bool getSubmissionState(uint32_t submission_id, RunResult& state, uint32_t& utype, std::string& comment) = 0;
	virtual uint32_t submission2user_id(uint32_t submission_id) = 0;
	virtual uint32_t submission2server_id(uint32_t submission_id) = 0;
	virtual std::string submission2problem(uint32_t submission_id) = 0;
	virtual bool hasSolved(uint32_t user_id, uint32_t prob_id) = 0;

	/**
	 * Functions to register a DbCon functor (function to create DbCons),
	 * get a DbCon and to release one.  This allows for connection pooling
	 * and re-use.  I don't think I'm going to kill them, there should only
	 * ever be three or four ...
	 */
	static DbCon *getInstance();
	void release();
	static bool registerFunctor(DbCon* (*functor)());
	static void cleanup();
};

#endif

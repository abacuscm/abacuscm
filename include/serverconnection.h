/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __SERVERCONNECTION_H__
#define __SERVERCONNECTION_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "dbcon.h"
#include "misc.h"
#include "threadssl.h"

#include <vector>
#include <string>
#include <list>
#include <map>
#include <openssl/ssl.h>
#include <stdint.h>

class MessageBlock;

typedef void (*EventCallback)(const MessageBlock*, void *);

typedef std::map<std::string, std::string> AttributeMap;
typedef std::list<AttributeMap> MultiValuedList;
typedef MultiValuedList SubmissionList;
typedef MultiValuedList ClarificationList;
typedef MultiValuedList ClarificationRequestList;
typedef std::list<std::list<std::string> > Grid;

typedef struct {
	uint32_t id;
	std::string code;
	std::string name;
} ProblemInfo;

typedef struct {
	uint32_t id;
	std::string username;
} UserInfo;

class ServerConnection {
	// In batch.C, to allow it to call sendMB directly
	friend void batch_sendMB(ServerConnection &_server_con, MessageBlock *&mb, std::vector<std::string> &content);
private:
	struct CallbackData {
		EventCallback func;
		void *p;
	};
	typedef std::list<CallbackData> CallbackList;
	typedef std::map<std::string, CallbackList> EventMap;

	SSL_CTX *_ctx;

	/* Single-entry queue for passing responses back from the receiver to the
	 * control thread.
	 */
	pthread_mutex_t _lock_response;
	pthread_cond_t _cond_response;
	MessageBlock* _response;

	pthread_t _keepalive_thread;
	pthread_mutex_t _lock_keepalive;
	pthread_cond_t _cond_keepalive;

	/* While the receiver thread is live, this is also an indicator of whether
	 * a keepalive timeout occurred.
	 */
	bool _kill_keepalive_thread;

	pthread_mutex_t _lock_eventmap;
	EventMap _eventmap;

	/* These do not need locks, since they are synchronised through the
	 * internal lock in ThreadSSL. The control thread may not change their
	 * values while the receiver thread is alive.
	 */
	pthread_t _receiver_thread;
	int _sock;
	ThreadSSL *_ssl;

	void processMB(MessageBlock *mb);
	MessageBlock *sendMB(MessageBlock *mb);
	void* receive_thread();

	void *keepalive_thread();

	bool simpleAction(MessageBlock &mb);
	std::vector<std::string> vectorAction(MessageBlock &mb, std::string prefix);
	MultiValuedList multiVectorAction(MessageBlock &mb, std::list<std::string> attrs);
	std::vector<uint32_t> uintVectorAction(MessageBlock &mb, std::string prefix);
	std::string stringAction(MessageBlock &mb, std::string fieldname);
	Grid gridAction(MessageBlock &mb);

	static std::vector<std::string> vectorFromMB(MessageBlock &mb, std::string prefix);
	static MultiValuedList multiListFromMB(MessageBlock &mb, std::list<std::string> attrlst);

	static void* receiver_spawner(void*);
	static void* keepalive_spawner(void*);

	/* Begin shutting down the connection. This should only be called by the
	 * receiver thread on connection termination or the keepalive thread on a
	 * timeout. It is safe to call it more than once.
	 *
	 * It will ensure that all threads waiting on SSL reads or writes are
	 * interrupted, and will tell the main thread that something went wrong if
	 * it is still expecting a response.
	 */
	void startShutdown();

public:
	ServerConnection();
	~ServerConnection();

	bool connect(std::string servername, std::string service);
	bool disconnect();

	static Grid gridFromMB(const MessageBlock &mb);

	bool auth(std::string username, std::string password);
	std::string whatAmI();
	bool createuser(std::string username, std::string friendlyname, std::string password, std::string type);
	bool changePassword(std::string password);
	bool changePassword(uint32_t id, std::string password);

	bool startStop(bool global, bool start, time_t time);

	std::vector<std::string> getProblemTypes();
	std::vector<ProblemInfo> getProblems();
	std::vector<ProblemInfo> getSubmissibleProblems();
	std::string getProblemDescription(std::string problemtype);

	std::vector<UserInfo> getUsers();

	std::vector<std::string> getServerList();

	bool setProblemAttributes(uint32_t prob_id, std::string type,
	                const AttributeMap& normal, const AttributeMap& file,
		            ProblemList dependencies);
	bool getProblemAttributes(uint32_t prob_id, AttributeMap& attrs);
	bool getProblemFile(uint32_t prob_id, std::string attrib, char **bufferptr, uint32_t *bufferlen);
	bool getSubmissionSource(uint32_t submission_id, char **bufferptr, uint32_t *bufferlen);
	bool submit(uint32_t prob_id, int fd, const std::string& language);
	bool clarificationRequest(uint32_t prob_id, const std::string& question);
	bool clarificationReply(uint32_t clarification_req_id, bool pub, const std::string& answer);
	SubmissionList getSubmissions();
	ClarificationList getClarifications();
	ClarificationRequestList getClarificationRequests();
	Grid getStandings();

	uint32_t countMarkFiles(uint32_t submission_id);
	bool getMarkFile(uint32_t submission_id, uint32_t file_index, std::string &name, void **data, uint32_t &length);

	uint32_t contestTime();
	uint32_t contestRemain();
	bool contestRunning();
	bool subscribeTime();

	bool watchBalloons(bool yesno);

	bool becomeMarker();
	bool mark(uint32_t submission_id, RunResult result, std::string comment, const AttributeMap &files);

	bool registerEventCallback(std::string event, EventCallback func, void *custom);
	bool deregisterEventCallback(std::string event, EventCallback func);

private:
	std::vector<ProblemInfo> _getProblems(std::string query);
};

#endif

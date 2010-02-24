/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "serverconnection.h"
#include "logger.h"
#include "acmconfig.h"
#include "messageblock.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <netdb.h>
#include <openssl/rand.h>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <algorithm>
#include <time.h>

// 16K is the max size of an SSL block, thus
// optimal in terms of speed, but memory intensive.
#define BFR_SIZE		(16 * 1024)

// Character used to escape newlines
#define NEWLINE_ESCAPE_CHAR ('\001')

using namespace std;

ServerConnection::ServerConnection() {
	_sock = -1;
	_ssl = NULL;
	_ctx = NULL;
	_response = NULL;

	pthread_mutex_init(&_lock_response, NULL);
	pthread_mutex_init(&_lock_eventmap, NULL);
	pthread_mutex_init(&_lock_keepalive, NULL);

	pthread_cond_init(&_cond_response, NULL);

	// When initialising the _cond_keepalive condition, we set it to use CLOCK_MONOTONIC for
	// timed waits. This avoids any problems happening if the system clock changes.
	pthread_condattr_t keepalive_attr;
	pthread_condattr_init(&keepalive_attr);
	pthread_condattr_setclock(&keepalive_attr, CLOCK_MONOTONIC);
	pthread_cond_init(&_cond_keepalive, &keepalive_attr);
	pthread_condattr_destroy(&keepalive_attr);
}

ServerConnection::~ServerConnection() {
	if(_sock >= 0)
		disconnect();
	if(_ctx)
		SSL_CTX_free(_ctx);

	pthread_mutex_destroy(&_lock_response);
	pthread_mutex_destroy(&_lock_eventmap);
	pthread_mutex_destroy(&_lock_keepalive);

	pthread_cond_destroy(&_cond_response);
	pthread_cond_destroy(&_cond_keepalive);
}

bool ServerConnection::connect(string server, string service) {
	Config &config = Config::getConfig();

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	addrinfo * adinf = NULL;
	addrinfo * i;
	SSL *ssl = NULL;

	if(_ssl != NULL) {
		log(LOG_ERR, "Already connected!");
		return false;
	}

	if(!_ctx) {
		_ctx = SSL_CTX_new(TLSv1_client_method());
		if(!_ctx) {
			log_ssl_errors("SSL_CTX_new");
			goto err;
		}

		if(!SSL_CTX_load_verify_locations(_ctx,
					config["server"]["cacert"].c_str(), NULL)) {
			log(LOG_ERR, "Failed to load certificate. Please check that your configuration file has the correct path in the 'cacert' line.");
			SSL_CTX_free(_ctx);
			_ctx = NULL;
			goto err;
		}

		SSL_CTX_set_verify(_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
		SSL_CTX_set_mode(_ctx, SSL_MODE_AUTO_RETRY);
	}

	int error_code;
	if((error_code = getaddrinfo(server.c_str(), service.c_str(), &hints, &adinf)) != 0) {
		log(LOG_ERR, "getaddrinfo (%s:%d) %s", __FILE__, __LINE__, gai_strerror(error_code));
		goto err;
	}

	for(i = adinf; i; i = i->ai_next) {
		_sock = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if(_sock < 0)
			lerror("socket");
		else // yes, this can go in the condition above but then we lose i.
			break;
	}

	if(_sock < 0)
		goto err;

	if(::connect(_sock, i->ai_addr, i->ai_addrlen) < 0) {
		lerror("connect");
		goto err;
	}
	freeaddrinfo(adinf);
	adinf = NULL;

	ssl = SSL_new(_ctx);
	if(!ssl) {
		log_ssl_errors("SSL_new");
		goto err;
	}

	if (fcntl(_sock, F_SETFL, O_NONBLOCK) != 0) {
		lerror("fcntl");
		goto err;
	}

	if(!SSL_set_fd(ssl, _sock)) {
		log_ssl_errors("SSL_set_fd");
		goto err;
	}

	_ssl = new(nothrow) ThreadSSL(ssl);
	if (_ssl == NULL) {
		log(LOG_ERR, "OOM allocating ThreadSSL");
		goto err;
	}
	ssl = NULL;    // _ssl takes over ownership

	if(_ssl->connect(ThreadSSL::BLOCK_FULL).processed < 1) {
		log_ssl_errors("SSL_connect");
		goto err;
	}

	log(LOG_DEBUG, "TODO:  Check servername against cn of certificate");

	pthread_mutex_lock(&_lock_keepalive);
	_kill_keepalive_thread = false;
	pthread_mutex_unlock(&_lock_keepalive);
	if (pthread_create(&_keepalive_thread, NULL, keepalive_spawner, this) != 0)
	{
		log(LOG_ERR, "failed to create keepalive thread");
		goto err;
	}

	if (pthread_create(&_receiver_thread, NULL, receiver_spawner, this) != 0)
	{
		log(LOG_ERR, "failed to create receiver thread");

		// Kill off the keepalive thread which we spawned earlier
		pthread_mutex_lock(&_lock_keepalive);
		_kill_keepalive_thread = true;
		pthread_cond_signal(&_cond_keepalive);
		pthread_mutex_unlock(&_lock_keepalive);
		pthread_join(_keepalive_thread, NULL);
		goto err;
	}

	return true;
err:
	if(ssl) {
		SSL_free(ssl);
		ssl = NULL;
	}
	if(_ssl) {
		delete _ssl;
		_ssl = NULL;
	}
	if(_sock) {
		close(_sock);
		_sock = -1;
	}
	if(adinf)
		freeaddrinfo(adinf);
	return false;
}

bool ServerConnection::disconnect() {
	if (_ssl == NULL) {
		log(LOG_WARNING, "Attempting to disconnect non-connected socket.");
		return false;
	}

	// This will internally wake up the receiver and get it to shut down
	_ssl->startShutdown();
	pthread_join(_receiver_thread, NULL);

	// Terminate the keepalive thread
	pthread_mutex_lock(&_lock_keepalive);
	_kill_keepalive_thread = true;
	pthread_cond_signal(&_cond_keepalive);
	pthread_mutex_unlock(&_lock_keepalive);
	pthread_join(_keepalive_thread, NULL);

	delete _ssl;
	_ssl = NULL;
	close(_sock);
	_sock = -1;

	return true;
}

void ServerConnection::startShutdown() {
	// If we are closing the connection or timing out, then make sure to
	// wake up the GUI event if it's waiting for a response.
	pthread_mutex_lock(&_lock_response);
	if (_response == NULL) {
		_response = new MessageBlock("err");
		(*_response)["msg"] = "Connection terminated while processing request";
	}
	// In addition, we ensure that the connection is closed. This avoids a case where
	// the GUI makes multiple requests to the server, and we timeout the connection
	// in the middle of the first one.
	_ssl->startShutdown();

	pthread_cond_signal(&_cond_response);
	pthread_mutex_unlock(&_lock_response);
}

void ServerConnection::processMB(MessageBlock *mb) {
	if(mb->action() == "ok" || mb->action() == "err") {
		pthread_mutex_lock(&_lock_response);
		if(_response) {
			log(LOG_WARNING, "Losing result - this could potentially cause problems");
			delete mb;
		} else {
			_response = mb;
		}
		pthread_cond_signal(&_cond_response);
		pthread_mutex_unlock(&_lock_response);
	} else {
		// If we are closing the connection or timing out, then make sure to
		// wake up the GUI event if it's waiting for a response.
		if (mb->action() == "close")
			startShutdown();

		if (mb->action() == "keepalive") {
			// If we receive a keepalive, we let our keepalive thread know about it.
			// We also pass the keepalive on to any event listeners, in case clients
			// want to do something when keepalive messages are received.
			pthread_mutex_lock(&_lock_keepalive);
			pthread_cond_signal(&_cond_keepalive);
			pthread_mutex_unlock(&_lock_keepalive);
		}

		log(LOG_DEBUG, "Received event '%s'", mb->action().c_str());
		// Handle clarification reply events that encode newlines
		for (MessageHeaders::iterator i = mb->begin(); i != mb->end(); i++)
			replace(i->second.begin(), i->second.end(), NEWLINE_ESCAPE_CHAR, '\n');
		pthread_mutex_lock(&_lock_eventmap);
		EventMap::iterator emi = _eventmap.find(mb->action());
		if(emi != _eventmap.end()) {
			CallbackList::iterator i;
			for(i = emi->second.begin(); i != emi->second.end(); ++i)
				i->func(mb, i->p);
		}
		pthread_mutex_unlock(&_lock_eventmap);
		delete mb;
	}
}

MessageBlock* ServerConnection::sendMB(MessageBlock *mb) {
	MessageBlock *res = NULL;

	if(!_ssl) {
		log(LOG_ERR, "Attempting to send data to a NULL _ssl socket");
		return NULL;
	}

	pthread_mutex_lock(&_lock_response);
	delete _response;
	_response = NULL;
	pthread_mutex_unlock(&_lock_response);
	if(mb->writeToSSL(_ssl)) {
		pthread_mutex_lock(&_lock_response);
		while(!_response)
			pthread_cond_wait(&_cond_response, &_lock_response);
		res = _response;
		_response = NULL;
		pthread_mutex_unlock(&_lock_response);
	}

	return res;
}

bool ServerConnection::simpleAction(MessageBlock &mb) {
	MessageBlock *ret = sendMB(&mb);
	if(!ret)
		return false;

	bool response = ret->action() == "ok";
	if(!response)
		log(LOG_ERR, "%s", (*ret)["msg"].c_str());

	delete ret;

	return response;
}

vector<string> ServerConnection::vectorAction(MessageBlock &mb, string prefix) {
	MessageBlock *ret = sendMB(&mb);
	if(!ret)
		return vector<string>();

	vector<string> resp(vectorFromMB(*ret, prefix));
	delete ret;
	return resp;
}

MultiValuedList ServerConnection::multiVectorAction(MessageBlock &mb, list<string> attrs) {
	MessageBlock *ret = sendMB(&mb);
	if(!ret)
		return MultiValuedList();

	MultiValuedList resp(multiListFromMB(*ret, attrs));
	delete ret;
	return resp;
}

string ServerConnection::stringAction(MessageBlock &mb, string fieldname) {
	MessageBlock *ret = sendMB(&mb);
	if(!ret)
		return "";

	bool response = ret->action() == "ok";
	string result;
	if(!response)
		log(LOG_ERR, "%s", (*ret)["msg"].c_str());
	else
		result = (*ret)[fieldname];

	delete ret;

	return result;
}

Grid ServerConnection::gridAction(MessageBlock &mb) {
	Grid grid;
	MessageBlock *ret = sendMB(&mb);

	if(!ret || ret->action() != "ok") {
		if(ret) {
			log(LOG_ERR, "%s", (*ret)["msg"].c_str());
			delete ret;
		}
		return Grid();
	}

	Grid resp(gridFromMB(*ret));
	delete ret;
	return resp;
}

Grid ServerConnection::gridFromMB(const MessageBlock &mb) {
	Grid grid;

	int ncols = strtoll(mb["ncols"].c_str(), NULL, 0);
	int nrows = strtoll(mb["nrows"].c_str(), NULL, 0);

	for(int r = 0; r < nrows; ++r) {
		list<string> row;
		for(int c = 0; c < ncols; ++c) {
			ostringstream hdrname;
			hdrname << "row_" << r << "_" << c;
			row.push_back(mb[hdrname.str()]);
		}
		grid.push_back(row);
	}
	return grid;
}

vector<string> ServerConnection::vectorFromMB(MessageBlock &mb, string prefix) {
	unsigned i = 0;
	ostringstream nattr;
	nattr << prefix << i;
	vector<string> result;
	while(mb.hasAttribute(nattr.str())) {
		result.push_back(mb[nattr.str()]);
		nattr.str("");
		nattr << prefix << ++i;
	}
	return result;
}

MultiValuedList ServerConnection::multiListFromMB(MessageBlock &mb, list<string> attrlst) {
	MultiValuedList result;
	unsigned i = 0;
	while(true) {
		ostringstream tmp;
		tmp << i++;
		string ival = tmp.str();
		bool foundone = false;
		AttributeMap attrs;
		list<string>::iterator j;
		for(j = attrlst.begin(); j != attrlst.end(); ++j) {
			if(mb.hasAttribute(*j + ival)) {
				attrs[*j] = mb[*j + ival];
				foundone = true;
			}
		}
		if(foundone)
			result.push_back(attrs);
		else
			break;
	}
	return result;
}

bool ServerConnection::auth(string username, string password) {
	MessageBlock mb("auth");

	mb["user"] = username;
	mb["pass"] = password;

	return simpleAction(mb);
}

string ServerConnection::whatAmI() {
	MessageBlock mb("whatami");

	return stringAction(mb, "type");
}

bool ServerConnection::createuser(string username, string friendlyname, string password, string type) {
	MessageBlock mb("adduser");

	mb["username"] = username;
	mb["friendlyname"] = friendlyname;
	mb["passwd"] = password;
	mb["type"] = type;

	return simpleAction(mb);
}

bool ServerConnection::changePassword(string password) {
	MessageBlock mb("passwd");

	mb["newpass"] = password;

	return simpleAction(mb);
}

bool ServerConnection::changePassword(uint32_t id, string password) {
	MessageBlock mb("id_passwd");
	ostringstream id_str;
	id_str << id;

	mb["user_id"] = id_str.str();
	mb["newpass"] = password;

	return simpleAction(mb);
}

bool ServerConnection::startStop(bool global, bool start, time_t time) {
	MessageBlock mb("startstop");
	ostringstream t;

	t << time;
	mb["action"] = start ? "start" : "stop";
	mb["time"] = t.str();
	mb["server_id"] = global ? "all" : "self";

	return simpleAction(mb);
}

vector<string> ServerConnection::getProblemTypes() {
	MessageBlock mb("getprobtypes");

	return vectorAction(mb, "type");
}

vector<string> ServerConnection::getServerList() {
	MessageBlock mb("getserverlist");

	return vectorAction(mb, "server");
}

string ServerConnection::getProblemDescription(string type) {
	MessageBlock mb("getprobdescript");
	mb["type"] = type;

	return stringAction(mb, "descript");
}

bool ServerConnection::setProblemAttributes(uint32_t prob_id, std::string type,
                       const AttributeMap& normal, const AttributeMap& file,
		               ProblemList dependencies) {
	ostringstream ostrstrm;
	AttributeMap::const_iterator i;
	MessageBlock mb("setprobattrs");
	list<int> fds;
	uint32_t file_offset = 0;

	ostrstrm << prob_id;

	mb["prob_id"] = ostrstrm.str();
	mb["prob_type"] = type;

	for(i = file.begin(); i != file.end(); ++i) {
		int fd = open(i->second.c_str(), O_RDONLY | O_EXCL);
		if(fd < 0) {
			lerror(("open(" + i->second + ")").c_str());
			for(list<int>::iterator j = fds.begin(); j != fds.end(); ++j)
				close(*j);

			return false;
		}
		struct stat st_stat;
		if(fstat(fd, &st_stat) < 0) {
			lerror(("fstat(" + i->second + ")").c_str());
			for(list<int>::iterator j = fds.begin(); j != fds.end(); ++j)
				close(*j);

			return false;
		}

		ostrstrm.str("");
		char *tmp = strdup(i->second.c_str());
		ostrstrm << file_offset << " " << st_stat.st_size << " " << basename(tmp);
		free(tmp);

		mb[i->first] = ostrstrm.str();
		file_offset += st_stat.st_size;
		fds.push_back(fd);
	}

	log(LOG_DEBUG, "Loading %u bytes ...", file_offset);
	char *filedata = new char[file_offset];
	char *pos = filedata;
	for(list<int>::iterator fd_ptr = fds.begin(); fd_ptr != fds.end(); ++fd_ptr) {
		struct stat st_stat;
		if(fstat(*fd_ptr, &st_stat) < 0) {
			lerror(("fstat(" + i->second + ")").c_str());
			for( ; fd_ptr != fds.end(); ++fd_ptr)
				close(*fd_ptr);
			delete []filedata;
			return false;
		}
		while(st_stat.st_size) {
			int res = read(*fd_ptr, pos, st_stat.st_size);
			log(LOG_DEBUG, "read returned %d", res);
			if(res < 0) {
				lerror("read");
				for( ; fd_ptr != fds.end(); ++fd_ptr)
					close(*fd_ptr);
				delete []filedata;
		    	return false;
			}
			st_stat.st_size -= res;
			pos += res;
		}
	}
	if(filedata + file_offset != pos)
		log(LOG_ERR, "Potential problem, the amount of data read doesn't match up with the calculated amount of data required (Loaded %ld, expected %u)", (long)(pos - filedata), file_offset);
	mb.setContent(filedata, file_offset);
	delete []filedata;

	for(i = normal.begin(); i != normal.end(); ++i) {
		mb[i->first] = i->second;
	}

	std::ostringstream deps;
	bool done_one = false;
	for (ProblemList::iterator i = dependencies.begin(); i != dependencies.end(); i++) {
		if (done_one)
			deps << ",";
		deps << *i;
		done_one = true;
	}

	mb["prob_dependencies"] = deps.str();

//	mb.dump();

	return simpleAction(mb);
}

bool ServerConnection::getProblemAttributes(uint32_t prob_id, AttributeMap& attrs) {
	ostringstream strm;
	strm << prob_id;

	MessageBlock mb("getprobattrs");
	mb["prob_id"] = strm.str();

	MessageBlock *ret = sendMB(&mb);
	if(!ret)
		return false;

	bool response = ret->action() == "ok";
	if(!response) {
		log(LOG_ERR, "%s", (*ret)["msg"].c_str());
		delete ret;
		return false;
	}

	MessageHeaders::const_iterator i;
	for(i = ret->begin(); i != ret->end(); ++i) {
		if(i->first != "content-length")
			attrs[i->first] = i->second;
	}

	delete ret;
	return true;
}

bool ServerConnection::getProblemFile(uint32_t prob_id, string attrib, char **bufferptr, uint32_t *bufferlen) {
	ostringstream strm;
	strm << prob_id;

	MessageBlock mb("getprobfile");
	mb["prob_id"] = strm.str();
	mb["file"] = attrib;

	MessageBlock *ret = sendMB(&mb);
	if(!ret)
		return false;

	bool response = ret->action() == "ok";
	if(!response) {
		log(LOG_ERR, "%s", (*ret)["msg"].c_str());
		delete ret;
		return false;
	}

	*bufferlen = ret->content_size();
	*bufferptr = new char[*bufferlen];
	memcpy(*bufferptr, ret->content(), *bufferlen);

	delete ret;
	return true;
}

bool ServerConnection::getSubmissionSource(uint32_t submission_id, char **bufferptr, uint32_t *bufferlen) {
	ostringstream str;

	MessageBlock mb("getsubmissionsource");
	str << submission_id;
	mb["submission_id"] = str.str();

	MessageBlock *ret = sendMB(&mb);
	if (!ret)
		return false;

	*bufferlen = ret->content_size();
	*bufferptr = new char[(*bufferlen)];
	memcpy(*bufferptr, ret->content(), *bufferlen);

	delete ret;
	return true;
}

vector<ProblemInfo> ServerConnection::getProblems() {
	return _getProblems("getproblems");
}

vector<ProblemInfo> ServerConnection::getSubmissibleProblems() {
	return _getProblems("getsubmissibleproblems");
}

vector<ProblemInfo> ServerConnection::_getProblems(std::string query) {
	vector<ProblemInfo> response;
	MessageBlock mb(query);

	MessageBlock *res = sendMB(&mb);
	if(!res)
		return response;

	if(res->action() != "ok") {
		log(LOG_ERR, "%s", (*res)["msg"].c_str());
		delete res;
		return response;
	}

	unsigned i = 0;
	while(true) {
		ostringstream strstrm;
		strstrm << "id" << i;
		if(!res->hasAttribute(strstrm.str()))
			break;

		ProblemInfo tmp;
		tmp.id = strtoll((*res)[strstrm.str()].c_str(), NULL, 0);

		strstrm.str(""); strstrm << "code" << i;
		tmp.code = (*res)[strstrm.str()];

		strstrm.str(""); strstrm << "name" << i;
		tmp.name = (*res)[strstrm.str()];

		log(LOG_DEBUG, "Added problem '%s' (%s)", tmp.code.c_str(), tmp.name.c_str());
		response.push_back(tmp);
		i++;
	}

	delete res;
	return response;
}

vector<UserInfo> ServerConnection::getUsers() {
	vector<UserInfo> response;
	MessageBlock mb("getusers");

	MessageBlock *res = sendMB(&mb);
	if (!res)
		return response;

	if (res->action() != "ok") {
		log(LOG_ERR, "%s", (*res)["msg"].c_str());
		delete res;
		return response;
	}

	unsigned i = 0;
	while(true) {
		ostringstream strstrm;
		strstrm << "id" << i;
		if(!res->hasAttribute(strstrm.str()))
			break;

		UserInfo tmp;
		tmp.id = strtoll((*res)[strstrm.str()].c_str(), NULL, 0);

		strstrm.str(""); strstrm << "username" << i;
		tmp.username = (*res)[strstrm.str()];

		log(LOG_DEBUG, "Added user '%u' (%s)", (unsigned int) tmp.id, tmp.username.c_str());
		response.push_back(tmp);
		i++;
	}

	delete res;
	return response;
}

bool ServerConnection::submit(uint32_t prob_id, int fd, const string& lang) {
	ostringstream str_prob_id;
	str_prob_id << prob_id;

	MessageBlock mb("submit");
	mb["prob_id"] = str_prob_id.str();
	mb["lang"] = lang;

	struct stat statbuf;
	if(fstat(fd, &statbuf) < 0)
		return false;

	void *ptr = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if(!ptr)
		return false;
	mb.setContent((char*)ptr, statbuf.st_size);
	munmap(ptr, statbuf.st_size);

	return simpleAction(mb);
}

bool ServerConnection::clarificationRequest(uint32_t prob_id, const string& question) {
	if (!_ssl)
		return false;

	ostringstream str_prob_id;
	str_prob_id << prob_id;

	string flatquestion = question;
	replace(flatquestion.begin(), flatquestion.end(), '\n', NEWLINE_ESCAPE_CHAR);

	MessageBlock mb("clarificationrequest");
	mb["prob_id"] = str_prob_id.str();
	mb["question"] = flatquestion;

	return simpleAction(mb);
}

bool ServerConnection::clarificationReply(uint32_t clarification_req_id,
					  bool pub,
					  const string& answer) {
	if (!_ssl)
		return false;

	ostringstream str_cr_id;
	str_cr_id << clarification_req_id;

	string flatanswer = answer;
	replace(flatanswer.begin(), flatanswer.end(), '\n', NEWLINE_ESCAPE_CHAR);

	MessageBlock mb("clarification");
	mb["clarification_request_id"] = str_cr_id.str();
	mb["answer"] = flatanswer;
	mb["public"] = pub ? "1" : "0";

	return simpleAction(mb);
}

SubmissionList ServerConnection::getSubmissions() {
	if(!_ssl)
		return SubmissionList();

	MessageBlock mb("getsubmissions");
	list<string> attrs;
	attrs.push_back("submission_id");
	attrs.push_back("contestant");
	attrs.push_back("time");
	attrs.push_back("contesttime");
	attrs.push_back("problem");
	attrs.push_back("result");
	attrs.push_back("comment");
	attrs.push_back("prob_id");

	return multiVectorAction(mb, attrs);
}

ClarificationList ServerConnection::getClarifications() {
	if (!_ssl)
		return ClarificationList();

	MessageBlock mb("getclarifications");
	list<string> attrs;
	attrs.push_back("id");
	attrs.push_back("req_id");
	attrs.push_back("time");
	attrs.push_back("problem");
	attrs.push_back("question");
	attrs.push_back("answer");

	ClarificationList l = multiVectorAction(mb, attrs);
	for (ClarificationList::iterator i = l.begin(); i != l.end(); i++)
		for (AttributeMap::iterator j = i->begin(); j != i->end(); j++)
			replace(j->second.begin(), j->second.end(), NEWLINE_ESCAPE_CHAR, '\n');
	return l;
}

ClarificationRequestList ServerConnection::getClarificationRequests() {
	if (!_ssl)
		return ClarificationRequestList();

	MessageBlock mb("getclarificationrequests");
	list<string> attrs;
	attrs.push_back("id");
	attrs.push_back("time");
	attrs.push_back("problem");
	attrs.push_back("question");

	ClarificationRequestList l = multiVectorAction(mb, attrs);
	for (ClarificationRequestList::iterator i = l.begin(); i != l.end(); i++)
		for (AttributeMap::iterator j = i->begin(); j != i->end(); j++)
			replace(j->second.begin(), j->second.end(), NEWLINE_ESCAPE_CHAR, '\n');
	return l;
}

Grid ServerConnection::getStandings() {
	if(!_ssl)
		return Grid();

	MessageBlock mb("standings");

	return gridAction(mb);
}

bool ServerConnection::watchBalloons(bool yesno) {
	MessageBlock mb("balloonnotify");

	if(yesno)
		mb["action"] = "subscribe";
	else
		mb["action"] = "unsubscribe";

	return simpleAction(mb);
}

bool ServerConnection::becomeMarker() {
	MessageBlock mb("subscribemark");

	return simpleAction(mb);
}

bool ServerConnection::mark(uint32_t submission_id, RunResult result, std::string comment, const AttributeMap& file) {
	ostringstream ostrstrm;
	AttributeMap::const_iterator i;
	MessageBlock mb("mark");
	list<int> fds;
	uint32_t file_offset = 0;

	ostrstrm << submission_id;

	mb["submission_id"] = ostrstrm.str();

	ostrstrm.str("");
	ostrstrm << result;
	mb["result"] = ostrstrm.str();

	mb["comment"] = comment;

	int c = 0;
	for(i = file.begin(); i != file.end(); ++i, ++c) {
		int fd = open(i->second.c_str(), O_RDONLY | O_EXCL);
		if(fd < 0) {
			lerror(("open(" + i->second + ")").c_str());
			for(list<int>::iterator j = fds.begin(); j != fds.end(); ++j)
				close(*j);

			return false;
		}
		struct stat st_stat;
		if(fstat(fd, &st_stat) < 0) {
			lerror(("fstat(" + i->second + ")").c_str());
			for(list<int>::iterator j = fds.begin(); j != fds.end(); ++j)
				close(*j);

			return false;
		}

		ostrstrm.str("");
		ostrstrm << c;

		string fileoption = "file" + ostrstrm.str();

		ostrstrm.str("");
		ostrstrm << file_offset << " " << st_stat.st_size << " " << i->first;

		mb[fileoption] = ostrstrm.str();
		file_offset += st_stat.st_size;
		fds.push_back(fd);
	}

	log(LOG_DEBUG, "Loading %u bytes ...", file_offset);
	char *filedata = new char[file_offset];
	char *pos = filedata;
	for(list<int>::iterator fd_ptr = fds.begin(); fd_ptr != fds.end(); ++fd_ptr) {
		struct stat st_stat;
		if(fstat(*fd_ptr, &st_stat) < 0) {
			lerror(("fstat(" + i->second + ")").c_str());
			for( ; fd_ptr != fds.end(); ++fd_ptr)
				close(*fd_ptr);
			delete []filedata;
			return false;
		}
		while(st_stat.st_size) {
			int res = read(*fd_ptr, pos, st_stat.st_size);
			log(LOG_DEBUG, "read returned %d", res);
			if(res < 0) {
				lerror("read");
				for( ; fd_ptr != fds.end(); ++fd_ptr)
					close(*fd_ptr);
				delete []filedata;
				return false;
			}
			st_stat.st_size -= res;
			pos += res;
		}
	}
	if(filedata + file_offset != pos)
		log(LOG_ERR, "Potential problem, the amount of data read doesn't match up with the calculated amount of data required (Loaded %ld, expected %u)", (long)(pos - filedata), file_offset);
	mb.setContent(filedata, file_offset);
	delete []filedata;

	mb.dump();

	return simpleAction(mb);
}

uint32_t ServerConnection::countMarkFiles(uint32_t submission_id) {
	MessageBlock mb("fetchfile");
	mb["request"] = "count";
	ostringstream str("");
	str << submission_id;
	mb["submission_id"] = str.str();

	MessageBlock *result = sendMB(&mb);

	uint32_t count = strtoll((*result)["count"].c_str(), NULL, 0);
	delete result;
	return count;
}

bool ServerConnection::getMarkFile(uint32_t submission_id, uint32_t file_index, std::string &name, void **data, uint32_t &length) {
	MessageBlock mb("fetchfile");
	mb["request"] = "data";
	ostringstream str("");
	str << submission_id;
	mb["submission_id"] = str.str();
	str.str("");
	str << file_index;
	mb["index"] = str.str();

	MessageBlock *result = sendMB(&mb);

	name = (*result)["name"];
	length = strtoll((*result)["length"].c_str(), NULL, 0);
	*data = (void *) new char[length + 1];
	memcpy(*data, result->content(), length);
	((char *) *data) [length] = 0;
	return true;
}

uint32_t ServerConnection::contestTime() {
	MessageBlock mb("contesttime");

	MessageBlock *res = sendMB(&mb);

	if(res && res->action() == "ok") {
		uint32_t time = strtoll((*res)["time"].c_str(), NULL, 0);
		delete res;
		return time;
	} else {
		if(res) {
			log(LOG_ERR, "%s", (*res)["msg"].c_str());
			delete res;
		}
		return ~0U;
	}
}

uint32_t ServerConnection::contestRemain() {
	MessageBlock mb("contesttime");

	MessageBlock *res = sendMB(&mb);

	if(res && res->action() == "ok") {
		uint32_t time = strtoll((*res)["remain"].c_str(), NULL, 0);
		delete res;
		return time;
	} else {
		if(res) {
			log(LOG_ERR, "%s", (*res)["msg"].c_str());
			delete res;
		}
		return ~0U;
	}
}

bool ServerConnection::contestRunning() {
	MessageBlock mb("contesttime");

	MessageBlock *res = sendMB(&mb);

	if(res && res->action() == "ok") {
		bool running = (*res)["running"] == "yes";
		delete res;
		return running;
	} else {
		if(res) {
			log(LOG_ERR, "%s", (*res)["msg"].c_str());
			delete res;
		}
		return false;
	}
}

bool ServerConnection::subscribeTime() {
	MessageBlock mb("subscribetime");
	return simpleAction(mb);
}

bool ServerConnection::registerEventCallback(string event, EventCallback func, void *custom) {
	pthread_mutex_lock(&_lock_eventmap);

	CallbackList &list = _eventmap[event];
	CallbackList::iterator i;
	for(i = list.begin(); i != list.end(); ++i) {
		if(i->func == func) {
			i->func = func;
			i->p = custom;
			break;
		}
	}
	if(i == list.end()) {
		CallbackData t;
		t.func = func;
		t.p = custom;
		list.push_back(t);
	}

	pthread_mutex_unlock(&_lock_eventmap);
	return true;
}

bool ServerConnection::deregisterEventCallback(string event, EventCallback func) {
	pthread_mutex_lock(&_lock_eventmap);

	CallbackList &list = _eventmap[event];
	CallbackList::iterator i;
	for(i = list.begin(); i != list.end();) {
		if(i->func == func)
			list.erase(i++);
		else
			++i;
	}

	pthread_mutex_unlock(&_lock_eventmap);
	return true;
}

void* ServerConnection::receive_thread() {
	char buffer[BFR_SIZE];
	MessageBlock *mb = NULL;
	bool timed_out;

	log(LOG_DEBUG, "Starting ServerConnection receive_thread");
	while(true) {
		if(!_ssl) {
			log(LOG_CRIT, "This is bad!  Null _ssl pointer!");
			break;
		}
		ThreadSSL::Status res = _ssl->read(buffer, BFR_SIZE, ThreadSSL::BLOCK_PARTIAL);

		char *pos = buffer;
		if(res.err == SSL_ERROR_ZERO_RETURN) {
			break;
		}
		else while(res.processed > 0) {
			if(!mb)
				mb = new MessageBlock();
			int bytes_used = mb->addBytes(pos, res.processed);

			if(bytes_used < 0) {
				log(LOG_ERR, "Sync error!  This is extremely serious and probably indicates a bug!");
				delete mb;
				mb = NULL;
			} else if(bytes_used == 0) {
				res.processed = 0;
			} else {
				// processMB either deletes mb or puts it in a queue.
				processMB(mb);
				mb = NULL;
				res.processed -= bytes_used;
				pos += bytes_used;
			}
		}
	}

	pthread_mutex_lock(&_lock_keepalive);
	timed_out = _kill_keepalive_thread;
	pthread_mutex_unlock(&_lock_keepalive);

	mb = new MessageBlock("close");
	(*mb)["timedout"] = (timed_out ? "1" : "0");
	if (timed_out) {
		log(LOG_DEBUG, "Connection timed out, terminating receive_thread");
	}
	else {
		log(LOG_DEBUG, "Connection got shut down, terminating receive_thread");
	}
	processMB(mb);

	return NULL;
}

void *ServerConnection::keepalive_thread() {
	struct timespec triggerTime;

	pthread_mutex_lock(&_lock_keepalive);
	while (!_kill_keepalive_thread) {
		clock_gettime(CLOCK_MONOTONIC, &triggerTime);
		triggerTime.tv_sec  += KEEPALIVE_TIMEOUT;
		// If the condition is triggered, then this means that we either received
		// a keepalive, or are terminating. If there is an error then either
		// something went horribly wrong or we timed out, in which case we should
		// terminate.
		if (pthread_cond_timedwait(&_cond_keepalive, &_lock_keepalive, &triggerTime) != 0) {
			_kill_keepalive_thread = true;
			pthread_mutex_unlock(&_lock_keepalive);
			startShutdown();
			pthread_mutex_lock(&_lock_keepalive);
		}
	}
	pthread_mutex_unlock(&_lock_keepalive);

	return NULL;
}

void* ServerConnection::receiver_spawner(void *p) {
	return ((ServerConnection*)p)->receive_thread();
}

void* ServerConnection::keepalive_spawner(void *p) {
	return ((ServerConnection*)p)->keepalive_thread();
}

static void init() __attribute__((constructor));
static void init() {
	SSL_library_init();
	SSL_load_error_strings();
	RAND_load_file("/dev/urandom", 4096);
}

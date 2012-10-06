/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *	with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <algorithm>
#include <memory>
#include <cassert>

#include "acmconfig.h"
#include "logger.h"
#include "moduleloader.h"
#include "peermessenger.h"
#include "queue.h"
#include "message.h"
#include "message_createserver.h"
#include "message_createuser.h"
#include "message_creategroup.h"
#include "server.h"
#include "dbcon.h"
#include "threadssl.h"
#include "hashpw.h"
#include "waitable.h"
#include "clientlistener.h"
#include "clientconnection.h"
#include "clientaction.h"
#include "message_type_ids.h"
#include "timedaction.h"
#include "markers.h"
#include "clienteventregistry.h"
#include "usersupportmodule.h"

#define DEFAULT_MIN_IDLE_WORKERS		1
#define DEFAULT_MAX_IDLE_WORKERS		2
#define MIN_MAX_TOTAL_WORKERS			2

using namespace std;

class ThreadedWaitableSet : public WaitableSet
{
public:
	struct Worker {
		Queue<Waitable *> work_queue;
		pthread_t thread_id;
		ThreadedWaitableSet *owner;
	};
private:
	int _initial_workers;
	int _min_idle_workers;
	int _max_idle_workers;
	int _max_total_workers;
	Queue<Worker *> _idle_workers;

	// Manipulated only from the main thread
	int _total_workers;

	// Loads the tuning parameters from the configuration file
	void configure();

	void create_worker();
	void destroy_worker();

	static void *worker_thread(void *);
public:
	// Callback in main thread to process work
	virtual void process(Waitable *w);

	// Wraps the underlying run() method to spawn and clean up workers
	virtual void run();

	// Called by worker thread to notify it that the worker is now idle
	void worker_idle(Worker *w);
};

// some globals, the static keyword keeps them inside _this_
// source file.
static pthread_t thread_peer_listener;
static pthread_t thread_message_handler;
static pthread_t thread_socket_selector;
static pthread_t thread_message_resender;
static pthread_t thread_timed_actions;

// This variable indicates that we are still running.
static volatile sig_atomic_t abacusd_running = true;

// All the various Queue<>s
static Queue<Message*> message_queue;
static Queue<uint32_t> ack_queue;
static Queue<TimedAction*> timed_queue;
static ThreadedWaitableSet socket_pool;

/**
 * This signal handler sets abacusd_running to false
 * to make things terminate.  The only "thread" that
 * is directly affected is the peer_listener.  Once it
 * terminates all the others will follow as soon as their
 * queue's empty out.
 */
static void signal_die(int) {
	if(abacusd_running)
		abacusd_running = false;
}

/**
 * Set up the signals we need to catch...
 */
static bool setup_signals() {
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = signal_die;

	if(sigaction(SIGTERM, &action, NULL) < 0)
		goto err;
	if(sigaction(SIGINT, &action, NULL) < 0)
		goto err;

	return true;
err:
	lerror("sigaction");
	return false;
}

/**
 * Load all the required modules...
 */
static bool load_modules(ModuleLoader &module_loader) {
	Config &config = Config::getConfig();

	if(!module_loader.loadModule(config["modules"]["dbconnector"]))
		return false;

	DbCon *dbtest = DbCon::getInstance();
	if(!dbtest) {
		log(LOG_ERR, "Unable to obtain database instance, bailing initialization.");
		return false;
	}

	dbtest->release();

	if(!module_loader.loadModuleSet("support", config["modules"]["support"], true))
		return false;

	if(!module_loader.loadModule(config["modules"]["peermessenger"]))
		return false;

	if(!module_loader.loadModuleSet("act", config["modules"]["actions"]))
		return false;

	if(!module_loader.loadModuleSet("prob", config["modules"]["problemtypes"]))
		return false;

	return true;
}

/**
 * These are here to prevent segfaulting due to undefined initialization order.
 * Whilst loading modules happens after initialization of global/static this
 * cannot be quaranteed for ((constructor))s called as part of the basic loading
 * process.
 */
static Message* create_msg_createserver() {
	return new Message_CreateServer;
}


static Message* create_msg_createuser() {
	return new Message_CreateUser;
}

/**
 * initialise the system.  This will check whether we are recovering
 * from an existing competition (ie, are we already connected or not).
 */
static bool initialise() {
	Config& config = Config::getConfig();
	string localname = config["initialisation"]["name"];
	if(localname == string("")) {
		log(LOG_ERR, "Unable to determine local node name.");
		return false;
	}

	SSL_load_error_strings();
	SSL_library_init();
	ThreadSSL::initialise();

	// it would be nicer to have these in their appropriate source files - but they
	// insist on segfaulting there.
	// TODO: revisit now that module init is done with a specific function
	// instead of a constructor.
	Message::registerMessageFunctor(TYPE_ID_CREATESERVER, create_msg_createserver);
	Message::registerMessageFunctor(TYPE_ID_CREATEUSER, create_msg_createuser);

	log(LOG_INFO, "Local node name: %s", localname.c_str());

	DbCon *db = DbCon::getInstance();
	if(!db)
		return false;

	uint32_t local_id = Server::server_id(localname);
	MessageList msglist = db->getUnprocessedMessages();
	IdList sublist;
	if(local_id && db->getServerAttribute(local_id, "marker") == "yes")
		sublist = db->getUnmarked(local_id);
	db->release();db=NULL;

	if(local_id == ~0U) {
		return false;
	} else if(!local_id && config["initialisation"]["type"] == "master") {
		Message_CreateServer *init = new Message_CreateServer(localname, 1);
		ConfigSection::const_iterator i;
		for(i = config["init_attribs"].begin(); i != config["init_attribs"].end(); ++i)
			init->addAttribute(i->first, i->second);

		string initial_pw = config["initialisation"]["admin_password"];
		if (initial_pw == "") {
			log(LOG_DEBUG, "No initial password set, using default");
			initial_pw = "admin";
		}
		string initial_hashpw = hashpw("admin", initial_pw);
		Message_CreateGroup *default_group = new Message_CreateGroup("default", 1);
		Message_CreateUser *admin = new Message_CreateUser("admin", "Administrator", initial_hashpw, 1, USER_TYPE_ADMIN);

		if(init->makeMessage() && default_group->makeMessage() && admin->makeMessage()) {
			message_queue.enqueue(init);
			message_queue.enqueue(default_group);
			message_queue.enqueue(admin);
		} else {
			delete init;
			delete default_group;
			delete admin;
			return false;
		}
	} else {
		log(LOG_INFO, "Contest already initialised, resuming.");
	}

	if(!ClientConnection::init())
		return false;

	ClientListener *cl = new ClientListener();
	if(!cl->init(&socket_pool)) {
		delete cl;
		return false;
	}

	socket_pool.add(cl);

	ClientAction::setMessageQueue(&message_queue);

	message_queue.enqueue(msglist.begin(), msglist.end());
	for(IdList::iterator i = sublist.begin(); i != sublist.end(); ++i)
		Markers::getInstance().enqueueSubmission(*i);

	return true;
}

/**
 * This thread is responsible for listening for messages, pushing them
 * onto the message queue.
 */
static void* peer_listener(void *) {
	PeerMessenger* messenger = PeerMessenger::getMessenger();
	log(LOG_INFO, "Starting PeerListener thread.");
	while(abacusd_running) {
		Message * message = messenger->getMessage();
		if(message) {
			DbCon *db = DbCon::getInstance();
			if(db) {
				if(db->putRemoteMessage(message)) {
					message_queue.enqueue(message);
					messenger->sendAck(message->server_id(), message->message_id());
				} else
					delete message;
				db->release(); db = NULL;
			} else
				delete message;
		} else if(abacusd_running)
			log(LOG_WARNING, "Failed to get message!");
	}
	return NULL;
}

/**
 * This thread is responsible for handling messages that goes
 * onto the queue.	It will receive a quit message when it needs
 * to terminate, which will call thread_quit() for us.
 */
static void* message_handler(void *) {
	log(LOG_INFO, "message_handler() ready to process messages.");
	for(Message *m = message_queue.dequeue(); m; m = message_queue.dequeue()) {
		m->process();
		delete m;
	}
	log(LOG_INFO, "Shutting down message_handler().");
	return NULL;
}

/**
 * A worker thread.  These threads handles clients as well as the client
 * listener.
 */
void* ThreadedWaitableSet::worker_thread(void *data) {
	Worker *self = (Worker *) data;
	while(true) {
		Waitable *s = self->work_queue.dequeue();
		if(!s)
			break;

		s->process();
		socket_pool.process_finish(s);
		self->owner->worker_idle(self);
	}
	log(LOG_DEBUG, "Worker thread dying.");
	return NULL;
}

void ThreadedWaitableSet::configure() {
	Config& config = Config::getConfig();
	_initial_workers = atol(config["workers"]["init"].c_str());
	_max_total_workers = atol(config["workers"]["max"].c_str());
	_max_idle_workers = atol(config["workers"]["max_idle"].c_str());
	_min_idle_workers = atol(config["workers"]["min_idle"].c_str());

	if(_max_total_workers < MIN_MAX_TOTAL_WORKERS) {
		log(LOG_NOTICE, "Increasing the maximum number of total workers "
				"from %u to %u.", _max_total_workers, MIN_MAX_TOTAL_WORKERS);
		_max_total_workers = MIN_MAX_TOTAL_WORKERS;
	}

	if(_min_idle_workers == 0) {
		log(LOG_NOTICE, "Defaulting to %u minimum number of idle workers.",
				DEFAULT_MIN_IDLE_WORKERS);
		_min_idle_workers = DEFAULT_MIN_IDLE_WORKERS;
	}

	if(_max_idle_workers <= _min_idle_workers) {
		_max_idle_workers = _min_idle_workers +
			(DEFAULT_MAX_IDLE_WORKERS - DEFAULT_MIN_IDLE_WORKERS);
		log(LOG_NOTICE, "Defaulting to %u maximum number of idle workers.",
				_max_idle_workers);
	}

	if(_initial_workers <= _min_idle_workers ||
	   _initial_workers > _max_idle_workers) {
		// +1 causes the division to be rounded up!
		_initial_workers = (_min_idle_workers + _max_idle_workers + 1) / 2;
		log(LOG_NOTICE, "Defaulting to %u initial worker threads.",
				_initial_workers);
	}

	if(_max_total_workers < 2 * _max_idle_workers) {
		log(LOG_NOTICE, "Increasing the maximum number of workers threads "
				"to %u from %u.", 2 * _max_idle_workers, _max_total_workers);
		_max_total_workers = 2 * _max_idle_workers;
	}
}

void ThreadedWaitableSet::create_worker() {
	Worker *worker = new Worker();
	worker->owner = this;
	if (pthread_create(&worker->thread_id, NULL, worker_thread, worker) == 0) {
		_total_workers++;
		_idle_workers.enqueue(worker);
		log(LOG_DEBUG, "Created worker thread (now %d)", _total_workers);
	} else {
		log(LOG_ERR, "Failed to create worker thread (currently %d)", _total_workers);
		delete worker;
	}
}

void ThreadedWaitableSet::destroy_worker() {
	assert(_total_workers > 0);
	Worker *worker = _idle_workers.dequeue();
	assert(worker != NULL);
	worker->work_queue.enqueue(NULL); // requests shutdown
	pthread_join(worker->thread_id, NULL);
	delete worker;
	_total_workers--;
	log(LOG_DEBUG, "Destroyed worker thread (now %d)", _total_workers);
}

void ThreadedWaitableSet::run() {
	configure();
	_total_workers = 0;
	for (int i = 0; i < _initial_workers; i++) {
		create_worker();
	}
	WaitableSet::run();
	while (_total_workers > 0) {
		destroy_worker();
	}
}

void ThreadedWaitableSet::process(Waitable *w) {
	w->preprocess();

	int idle_workers = _idle_workers.size();
	/* At this point, worker threads can add workers to _idle_queue, but not
	 * remove from it - so we may end up with more idle workers than we really
	 * want, but never less (unless we've hit the total limit).
	 */
	while (idle_workers - 1 < _min_idle_workers && _total_workers < _max_total_workers) {
		// - 1 above is because we're about to give one thread work to do
		create_worker();
		idle_workers++;
	}
	while (idle_workers > _max_idle_workers) {
		destroy_worker();
		idle_workers--;
	}

	Worker *worker = _idle_workers.dequeue();
	worker->work_queue.enqueue(w);
}

void ThreadedWaitableSet::worker_idle(Worker *worker) {
	_idle_workers.enqueue(worker);
}

void* socket_selector(void*) {
	socket_pool.run();
	return NULL;
}

// TODO:  Make the "limit" variable configurable
// instead of simply 1.
void resend_message(uint32_t server_id) {
	PeerMessenger *messenger = PeerMessenger::getMessenger();
	if(!messenger)
		return;

	DbCon *db = DbCon::getInstance();
	if(!db)
		return;

	MessageList msg = db->getUnacked(server_id, TYPE_ID_CREATESERVER);
	if(msg.empty())
		msg = db->getUnacked(server_id, 0, 1);
	db->release();db=NULL;

	MessageList::iterator i;
	for(i = msg.begin(); i != msg.end(); ++i) {
		messenger->sendMessage(server_id, *i);
		delete *i;
	}
}

// TODO: Make the waiting period configurable
// instead of a fixed 5.  Possibly also make this
// a "hard" time, ie, every 5 seconds instead of
// a complete 5 seconds every time.
void* message_resender(void*) {
	while(true) {
		uint32_t s_ack = ack_queue.timed_dequeue(5, 0);
		if(!abacusd_running)
			break;
		if(s_ack) {
			resend_message(s_ack);
		} else {
			DbCon *db = DbCon::getInstance();
			if(db) {
				vector<uint32_t> servers = db->getRemoteServers();
				db->release();db=NULL;

				vector<uint32_t>::iterator i;
				for(i = servers.begin(); i != servers.end(); ++i) {
					resend_message(*i);
				}
			}
		}
	}

	return NULL;
}

bool TimedActionPtrLessThan(const TimedAction* v1, const TimedAction* v2) {
	// We want smallest first, the *_heap functions implement a max-heap, thus
	// reverse the direction of the comparison.
	return v1->processingTime() > v2->processingTime();
}

void* timed_actions(void*) {
	vector<TimedAction*> heap;
	TimedAction *next = NULL;

	while(abacusd_running) {
		time_t now = time(NULL);
		while(next && next->processingTime() <= now) {
			next->perform();
			delete next;
			if(heap.empty())
				next = NULL;
			else {
				next = *heap.begin();
				pop_heap(heap.begin(), heap.end(), TimedActionPtrLessThan);
				heap.pop_back();
			}
		}

		if(next) {
			time_t diff = next->processingTime() - now;
			TimedAction *tmp = timed_queue.timed_dequeue(diff, 0);
			if(tmp) {
				if(tmp->processingTime() < next->processingTime())
					swap(next, tmp);
				heap.push_back(tmp);
				push_heap(heap.begin(), heap.end(), TimedActionPtrLessThan);
			}
		} else
			next = timed_queue.dequeue();
	}

	delete next;
	timed_queue.enqueue(NULL);  // ensures that the loop below won't block
	while((next = timed_queue.dequeue()) != NULL) {
		delete next;
	}
	while (!heap.empty()) {
		delete heap.back();
		heap.pop_back();
	}

	return NULL;
}

/**
 * Set everything up to do what it's supposed to do and then wait for
 * it all to tear down again...
 */
int main(int argc, char ** argv) {
	const char * config_file = DEFAULT_SERVER_CONFIG;
	Config &config = Config::getConfig();

	if(argc > 1)
		config_file = argv[1];

	if(!config.load(config_file))
		return -1;

	if(!setup_signals())
		return -1;

	Server::setAckQueue(&ack_queue);
	Server::setTimedQueue(&timed_queue);
	Server::setWaitableSet(&socket_pool);

	Markers::getInstance().startTimeoutCheckingThread();

	auto_ptr<ModuleLoader> module_loader(new ModuleLoader());
	/* The ModuleLoader object uses scope to clean up on destruction. It
	 * also grabs config during initialisation, so the above line should not
	 * be moved above the configuration loading.
	 */
	if(!load_modules(*module_loader))
		return -1;

	if(!PeerMessenger::getMessenger())
		return -1;

	log(LOG_INFO, "Starting abacusd.");

	if(!initialise())
		return -1;

	pthread_create(&thread_peer_listener, NULL, peer_listener, NULL);
	pthread_create(&thread_message_handler, NULL, message_handler, NULL);
	pthread_create(&thread_socket_selector, NULL, socket_selector, NULL);
	pthread_create(&thread_message_resender, NULL, message_resender, NULL);
	pthread_create(&thread_timed_actions, NULL, timed_actions, NULL);

	log(LOG_DEBUG, "abacusd is up and running.");

	// TODO: This probably introduces a race condition,
	// but frankly, having to hit ^C twice in this case
	// is easier right now than figuring out sigprocmask and sigwait.
	while (abacusd_running)
		pause();

	Markers::getInstance().shutdown();

	PeerMessenger::getMessenger()->shutdown();

	socket_pool.shutdown();
	pthread_join(thread_peer_listener, NULL);
	message_queue.enqueue(NULL);
	ack_queue.enqueue(0);
	timed_queue.enqueue(NULL);

	// Now we can join() them too since they should die shortly.
	pthread_join(thread_message_handler, NULL);
	pthread_join(thread_socket_selector, NULL);
	pthread_join(thread_message_resender, NULL);
	pthread_join(thread_timed_actions, NULL);

	socket_pool.delete_all();
	DbCon::cleanup();
	ThreadSSL::cleanup();

	/* Close down all the modules */
	module_loader.reset();
	log(LOG_INFO, "abacusd is shut down.");
	return 0;
}

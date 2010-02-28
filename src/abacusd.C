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

#include "acmconfig.h"
#include "logger.h"
#include "moduleloader.h"
#include "peermessenger.h"
#include "queue.h"
#include "message.h"
#include "message_createserver.h"
#include "message_createuser.h"
#include "server.h"
#include "dbcon.h"
#include "threadssl.h"
#include "hashpw.h"
#include "socket.h"
#include "clientlistener.h"
#include "clientconnection.h"
#include "clientaction.h"
#include "message_type_ids.h"
#include "timedaction.h"
#include "markers.h"
#include "clienteventregistry.h"
#include "usersupportmodule.h"

#define DEFAULT_MIN_IDLE_WORKERS		5
#define DEFAULT_MAX_IDLE_WORKERS		10
#define MIN_MAX_TOTAL_WORKERS			50

using namespace std;

// some globals, the static keyword keeps them inside _this_
// source file.
static pthread_t thread_peer_listener;
static pthread_t thread_message_handler;
static pthread_t thread_socket_selector;
static pthread_t thread_worker_spawner;
static pthread_t thread_message_resender;
static pthread_t thread_timed_actions;

// This variable indicates that we are still running.
static volatile sig_atomic_t abacusd_running = true;

// All the various Queue<>s
static Queue<Message*> message_queue;
static Queue<Socket*> wait_queue;
static Queue<uint32_t> ack_queue;
static Queue<TimedAction*> timed_queue;

static SocketPool socket_pool;

// num_idle_workers counts the number of currently blocking
// worker threads, lock_numworkers protects it.
static volatile uint32_t num_idle_workers = 0;
static volatile uint32_t total_num_workers = 0;
static pthread_mutex_t lock_numworkers;
static pthread_cond_t cond_numworkers;
static uint32_t min_idle_workers;
static uint32_t max_idle_workers;

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
 * Need this because we want to signal the socket_selector
 * with SIGUSR1 to cause it to reload it's state, but at the
 * same time we don't want to get nuked from existance, and
 * setting to SIG_IGN causes the signal to be discarded and
 * doesn't cause select(2) to return EINTR.
 */
static void signal_nothing(int) {
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

	action.sa_handler = signal_nothing;
	if(sigaction(SIGUSR1, &action, NULL) < 0)
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
		Message_CreateUser *admin = new Message_CreateUser("admin", "Administrator", initial_hashpw, 1, USER_TYPE_ADMIN);

		if(init->makeMessage() && admin->makeMessage()) {
			message_queue.enqueue(init);
			message_queue.enqueue(admin);
		} else {
			delete init;
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

	socket_pool.locked_insert(cl);

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
		if(m->process()) {
			DbCon *db = DbCon::getInstance();
			if(db) {
				db->markProcessed(m->server_id(), m->message_id());
				db->release();db=NULL;
			} else {
				log(LOG_ERR, "Unable to mark message (%d, %d) as processed!",
						m->server_id(), m->message_id());
			}
		} else {
			log(LOG_CRIT, "Failure to process message (%d, %d)!!!  "
					"This is A HUGE PROBLEM!!!	Fix it and restart abacusd!!!",
					m->server_id(), m->message_id());
		}
		delete m;
	}
	log(LOG_INFO, "Shutting down message_handler().");
	return NULL;
}

/**
 * A worker thread.  These threads handles clients and run in detached mode.
 * They need to terminate either when dequeueing a NULL Socket* or if the
 * max block count is reached.
 */
void* worker_thread(void *) {
	pthread_mutex_lock(&lock_numworkers);
	total_num_workers++;
	log(LOG_DEBUG, "Creating worker thread (now has %d).", total_num_workers);
	while(true) {
		if(num_idle_workers == max_idle_workers)
			break;
		++num_idle_workers;
		pthread_mutex_unlock(&lock_numworkers);

		Socket *s = wait_queue.dequeue();
		pthread_mutex_lock(&lock_numworkers);
		if(--num_idle_workers < min_idle_workers)
			pthread_cond_signal(&cond_numworkers);
		if(!s)
			break;
		pthread_mutex_unlock(&lock_numworkers);

		if(s->process()) {
			socket_pool.locked_insert(s);
			pthread_kill(thread_socket_selector, SIGUSR1);
		} else
			delete s;
		pthread_mutex_lock(&lock_numworkers);
	}

	total_num_workers--;
	log(LOG_DEBUG, "Worker thread dying, %d left.", total_num_workers);
	if(!total_num_workers)
		pthread_cond_signal(&cond_numworkers);
	pthread_mutex_unlock(&lock_numworkers);
	return NULL;
}

/**
 * Worker spawner.	This thread spawns more workers as required.
 * It expects to get sent a SIGUSR1 whenever the number of idle
 * workers reaches the minimum, at which point it will spawn a
 * few.
 */
void* worker_spawner(void*) {
	Config& config = Config::getConfig();
	uint32_t initial_workers = atol(config["workers"]["init"].c_str());
	uint32_t max_num_workers = atol(config["workers"]["max"].c_str());
	max_idle_workers = atol(config["workers"]["max_idle"].c_str());
	min_idle_workers = atol(config["workers"]["min_idle"].c_str());

	if(max_num_workers < MIN_MAX_TOTAL_WORKERS) {
		log(LOG_NOTICE, "Increasing the maximum number of total workers "
				"from %u to %u.", max_num_workers, MIN_MAX_TOTAL_WORKERS);
		max_num_workers = MIN_MAX_TOTAL_WORKERS;
	}

	if(min_idle_workers == 0) {
		log(LOG_NOTICE, "Defaulting to %u minimum number of idle workers.",
				DEFAULT_MIN_IDLE_WORKERS);
		min_idle_workers = DEFAULT_MIN_IDLE_WORKERS;
	}

	if(max_idle_workers <= min_idle_workers) {
		max_idle_workers = min_idle_workers +
			(DEFAULT_MAX_IDLE_WORKERS - DEFAULT_MIN_IDLE_WORKERS);
		log(LOG_NOTICE, "Defaulting to %u maximum number of idle workers.",
				max_idle_workers);
	}

	if(initial_workers <= min_idle_workers ||
			initial_workers >= max_idle_workers) {
		// +1 couses the division to be rounded up!
		initial_workers = (min_idle_workers + max_idle_workers + 1) / 2;
		log(LOG_NOTICE, "Defaulting to %u initial woker threads.",
				initial_workers);
	}

	if(max_num_workers < 2 * max_idle_workers) {
		log(LOG_NOTICE, "Increasing the maximum number of workers threads "
				"to %u from %u.", 2 * max_idle_workers, max_num_workers);
		max_num_workers = 2 * max_idle_workers;
	}

	pthread_mutex_init(&lock_numworkers, NULL);
	pthread_cond_init(&cond_numworkers, NULL);

	while(initial_workers--) {
		pthread_t tmp;
		if(pthread_create(&tmp, NULL, worker_thread, NULL) == 0)
			pthread_detach(tmp);
	}

	sigset_t sigset;
	if(sigemptyset(&sigset))
		lerror("sigemptyset");
	else if(sigaddset(&sigset, SIGUSR1))
		lerror("sigaddset");
	else if(pthread_sigmask(SIG_BLOCK, &sigset, NULL))
		lerror("pthread_sigmask");
	else while(true) {
		pthread_mutex_lock(&lock_numworkers);
		pthread_cond_wait(&cond_numworkers, &lock_numworkers);

		if(!abacusd_running)
			break;

		uint32_t tospawn = min_idle_workers - num_idle_workers;
		uint32_t max_create = max_num_workers - total_num_workers;
		pthread_mutex_unlock(&lock_numworkers);
		if(max_create < tospawn)
			tospawn = max_create;

		while(tospawn--) {
			pthread_t tmp;
			if(pthread_create(&tmp, NULL, worker_thread, NULL) == 0)
				pthread_detach(tmp);
		}
	}

	log(LOG_INFO, "Waiting for workers to die ...");
	for(uint32_t i = 0; i < total_num_workers; i++)
		wait_queue.enqueue(NULL);

	while(total_num_workers)
		pthread_cond_wait(&cond_numworkers, &lock_numworkers);
	pthread_mutex_unlock(&lock_numworkers);

	pthread_cond_destroy(&cond_numworkers);
	pthread_mutex_destroy(&lock_numworkers);

	return NULL;
}

void* socket_selector(void*) {
	while(abacusd_running) {
		fd_set sockets;
		int n = 0;
		FD_ZERO(&sockets);

		SocketPool::iterator i;
		socket_pool.lock();
		for(i = socket_pool.begin(); i != socket_pool.end(); ++i)
			(*i)->addToSet(&n, &sockets);
		socket_pool.unlock();

		if(select(n, &sockets, NULL, NULL, NULL) < 0) {
			if(errno != EINTR)
				lerror("select");
		} else {
			socket_pool.lock();
			for(i = socket_pool.begin(); i != socket_pool.end();)
			if((*i)->isInSet(&sockets)) {
				Socket *s = *i;
				socket_pool.erase(i++);
				wait_queue.enqueue(s);
			} else
				++i;
			socket_pool.unlock();
			/*
			 * Note that the above loop is very messy.	This is due to the
			 * erase operation invalidating the iterator, so essentially we
			 * now increment the iterator before nuking what it pointed to.
			 * this gets extremely nasty and probably took the better part
			 * of a day to figure out.
			 */
		}
	}

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
// instead of a fixed 5.  Posibly also make this
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
	Server::setSocketQueue(&wait_queue);
	Server::setSocketPool(&socket_pool);

	Markers::getInstance().startTimeoutCheckingThread();

	ModuleLoader module_loader;
	/* The ModuleLoader object uses scope to clean up on destruction. It
	 * also grabs config during initialisation, so the above line should not
	 * be moved above the configuration loading.
	 */
	if(!load_modules(module_loader))
		return -1;

	if(!PeerMessenger::getMessenger())
		return -1;

	log(LOG_INFO, "Starting abacusd.");

	if(!initialise())
		return -1;

	pthread_create(&thread_peer_listener, NULL, peer_listener, NULL);
	pthread_create(&thread_message_handler, NULL, message_handler, NULL);
	pthread_create(&thread_worker_spawner, NULL, worker_spawner, NULL);
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

	pthread_join(thread_peer_listener, NULL);
	pthread_kill(thread_socket_selector, SIGTERM);
	message_queue.enqueue(NULL);
	ack_queue.enqueue(0);
	timed_queue.enqueue(NULL);

	pthread_mutex_lock(&lock_numworkers);
	pthread_cond_signal(&cond_numworkers);
	pthread_mutex_unlock(&lock_numworkers);

	// Now we can join() them too since they should die shortly.
	pthread_join(thread_message_handler, NULL);
	pthread_join(thread_worker_spawner, NULL);
	pthread_join(thread_socket_selector, NULL);
	pthread_join(thread_message_resender, NULL);
	pthread_join(thread_timed_actions, NULL);

	SocketPool::iterator i;
	socket_pool.lock();
	for(i = socket_pool.begin(); i != socket_pool.end(); ++i)
		delete (*i);
	socket_pool.unlock();

	DbCon::cleanup();
	ThreadSSL::cleanup();

	log(LOG_INFO, "abacusd is shut down.");
	return 0;
}

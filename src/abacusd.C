#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>

#include "config.h"
#include "logger.h"
#include "moduleloader.h"
#include "peermessenger.h"
#include "queue.h"
#include "message.h"
#include "message_createserver.h"
#include "sigsegv.h"
#include "dbcon.h"
#include "socket.h"
#include "clientlistener.h"

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

// This variable indicates that we are still running.
static volatile bool abacusd_running = true;

// All the various Queue<>s
static Queue<Message*> message_queue;
static Queue<Socket*> wait_queue;

static SocketPool socket_pool;

// num_idle_workers counts the number of currently blocking
// worker threads, lock_numworkers protects it.
static volatile uint32_t num_idle_workers = 0;
static volatile uint32_t total_num_workers = 0;
static pthread_mutex_t lock_numworkers;
static uint32_t min_idle_workers;
static uint32_t max_idle_workers;

/**
 * This signal handler sets abacusd_running to false
 * to make things terminate.  The only "thread" that
 * is directly affected is the peer_listener.  Once it
 * terminates all the others will follow as soon as their
 * queue's empty out.
 */
void signal_die(int) {
	if(abacusd_running) {
		abacusd_running = false;
		pthread_kill(thread_peer_listener, SIGTERM);
		pthread_kill(thread_worker_spawner, SIGUSR1);
	}
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

	setup_sigsegv();

	return true;
err:
	lerror("sigaction");
	return false;
}

/**
 * Load all the required modules...
 */
static bool load_modules() {
	Config &config = Config::getConfig();
	
	if(!ModuleLoader::loadModule(config["modules"]["peermessenger"]))
		return false;
	
	if(!ModuleLoader::loadModule(config["modules"]["dbconnector"]))
		return false;
	
	return true;
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
	
	log(LOG_INFO, "Local node name: %s", localname.c_str());

	DbCon *db = DbCon::getInstance();	
	if(!db)
		return false;

	uint32_t local_id = db->name2server_id(localname);
	db->release();
	
	if(local_id == ~0U) {
		return false;
	} else if(!local_id) {
		Message_CreateServer *init = new Message_CreateServer(localname, 1);
		ConfigSection::const_iterator i;
		for(i = config["init_attribs"].begin(); i != config["init_attribs"].end(); ++i)
			init->addAttribute(i->first, i->second);
		
		if(init->makeMessage())
			message_queue.enqueue(init);
		else {
			delete init;
			return false;
		}
	} else {
		log(LOG_INFO, "Contest already initialised, resuming.");
	}

	ClientListener *cl = new ClientListener();
	if(!cl->init(&socket_pool)) {
		delete cl;
		return false;
	}
	
	socket_pool.insert(cl);
	
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
		if(message)
			message_queue.enqueue(message);
		else
			log(LOG_WARNING, "Failed to get message!");
	}
	return NULL;
}

/**
 * This thread is responsible for handling messages that goes
 * onto the queue.  It will receive a quit message when it needs
 * to terminate, which will call thread_quit() for us.
 */
static void* message_handler(void *) {
	log(LOG_INFO, "message_handler() ready to process messages.");
	for(Message *m = message_queue.dequeue(); m; m = message_queue.dequeue()) {
		if(m->process()) {
			DbCon *db = DbCon::getInstance();
			if(db) {
				db->markProcessed(m->server_id(), m->message_id());
				db->release();
			} else {
				log(LOG_ERR, "Unable to mark message (%d, %d) as processed!",
						m->server_id(), m->message_id());
			}
		} else {
			log(LOG_CRIT, "Failure to process message (%d, %d)!!!  "
					"This is A HUGE PROBLEM!!!  Fix it and restart abacusd!!!",
					m->server_id(), m->message_id());
		}
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
			pthread_kill(thread_worker_spawner, SIGUSR1);
		if(!s)
			break;
		pthread_mutex_unlock(&lock_numworkers);

		if(s->process()) {
			socket_pool.insert(s);
			pthread_kill(thread_socket_selector, SIGUSR1);
		} else
			delete s;
		pthread_mutex_lock(&lock_numworkers);			
	}

	total_num_workers--;
	log(LOG_DEBUG, "Worker thread dying, %d left.", total_num_workers);
	if(!total_num_workers)
		pthread_kill(thread_worker_spawner, SIGUSR1);
	pthread_mutex_unlock(&lock_numworkers);
	return NULL;
}

/**
 * Worker spawner.  This thread spawns more workers as required.
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
	else while(abacusd_running) {
		int signum;
		sigwait(&sigset, &signum);

		if(!abacusd_running)
			break;

		pthread_mutex_lock(&lock_numworkers);
		uint32_t tospawn = min_idle_workers - num_idle_workers;
		uint32_t max_create = max_num_workers - total_num_workers;
		pthread_mutex_unlock(&lock_numworkers);
		if(max_create < tospawn)
			tospawn = max_create;

		while(tospawn) {
			pthread_t tmp;
			if(pthread_create(&tmp, NULL, worker_thread, NULL) == 0)
				pthread_detach(tmp);
		}
	}

	log(LOG_INFO, "Waiting for workers to die ...");
	for(uint32_t i = 0; i < total_num_workers; i++)
		wait_queue.enqueue(NULL);

	while(total_num_workers) {
		int signum;
		sigwait(&sigset, &signum);
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

	if(!load_modules())
		return -1;
	
	if(!PeerMessenger::getMessenger())
		return -1;

	log(LOG_INFO, "Starting abacusd.");

	if(!initialise())
		return -1;
	
	pthread_create(&thread_peer_listener, NULL, peer_listener, NULL);
	pthread_create(&thread_message_handler, NULL, message_handler, NULL);
	pthread_create(&thread_worker_spawner, NULL, worker_spawner, NULL);

	log(LOG_DEBUG, "abacusd is up and running.");

	pthread_join(thread_peer_listener, NULL);
	
	message_queue.enqueue(NULL);
	pthread_join(thread_message_handler, NULL);
	pthread_join(thread_worker_spawner, NULL);

	log(LOG_INFO, "abacusd is shut down.");
	return 0;
}

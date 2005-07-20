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
#include "sigsegv.h"

using namespace std;

// some globals, the static keyword keeps them inside _this_
// source file.
static pthread_t thread_peer_listener;
static pthread_t thread_message_handler;

// This variable indicates that we are still running.
static volatile bool abacusd_running = true;

// All the various Queue<>s
static Queue<Message*> message_queue;

/**
 * This signal handler sets abacusd_running to false
 * to make things terminate.  The only "thread" that
 * is directly affected is the peer_listener.  Once it
 * terminates all the others will follow as soon as their
 * queue's empty out.
 */
void signal_die(int) {
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
	for(Message *m = message_queue.dequeue(); m; m = message_queue.dequeue())
		m->process();
	log(LOG_INFO, "Shutting down message_handler().");
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

	pthread_create(&thread_peer_listener, NULL, peer_listener, NULL);
	pthread_create(&thread_message_handler, NULL, message_handler, NULL);

	log(LOG_DEBUG, "abacusd is up and running.");


	pthread_join(thread_peer_listener, NULL);
	
	message_queue.enqueue(NULL);
	pthread_join(thread_message_handler, NULL);

	log(LOG_INFO, "abacusd is shut down.");
	return 0;
}

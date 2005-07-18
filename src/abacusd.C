#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <unistd.h>

#include "config.h"
#include "logger.h"
#include "moduleloader.h"
#include "peermessenger.h"

using namespace std;

// some globals, the static keyword keeps them inside _this_
// source file.
static pthread_t thread_peer_listener;
static pthread_t thread_message_handler;

// This variable indicates that we are still running.
static volatile bool abacusd_running = true;

static bool load_modules() {
	Config &config = Config::getConfig();
	
	if(!ModuleLoader::loadModule(config["modules"]["peermessenger"]))
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
		if(message) {
			NOT_IMPLEMENTED();
		} else
			log(LOG_WARNING, "Failed to get message!");
	}
	
	log(LOG_INFO, "Terminating PeerListener thread.");
	return NULL;
}

/**
 * This thread is responsible for handling messages that goes
 * onto the queue.  It will receive a quit message when it needs
 * to terminate, which will call thread_quit() for us.
 */
static void* message_handler(void *) {
	// for now we do nothing ...
	NOT_IMPLEMENTED();
	return NULL;
}

int main(int argc, char ** argv) {
	const char * config_file = DEFAULT_SERVER_CONFIG;
	Config &config = Config::getConfig();
	
	if(argc > 1)
		config_file = argv[1];
	
	if(!config.load(config_file))
		return -1;

	if(!load_modules())
		return -1;
	
	if(!PeerMessenger::getMessenger())
		return -1;

	log(LOG_INFO, "Starting abacusd.");
	log(LOG_DEBUG, "Blocking ..., hit <ENTER>");

	pthread_create(&thread_peer_listener, NULL, peer_listener, NULL);
	pthread_create(&thread_message_handler, NULL, message_handler, NULL);

	wait(NULL);

	pthread_join(thread_peer_listener, NULL);
	pthread_join(thread_message_handler, NULL);

	log(LOG_INFO, "abacusd is shut down.");
	return 0;
}

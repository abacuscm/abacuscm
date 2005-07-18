#include <iostream>
#include <unistd.h>

#include "config.h"
#include "logger.h"
#include "moduleloader.h"
#include "peermessenger.h"

using namespace std;

bool load_modules() {
	Config &config = Config::getConfig();
	
	if(!ModuleLoader::loadModule(config["modules"]["peermessenger"]))
		return false;
	
	return true;
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

	cin.get();

	return 0;
}

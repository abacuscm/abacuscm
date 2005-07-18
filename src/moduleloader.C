#include <dlfcn.h>

#include "moduleloader.h"
#include "logger.h"
#include "config.h"

using namespace std;

ModuleLoader::ModuleLoader() {
	NOT_IMPLEMENTED();
}

ModuleLoader::~ModuleLoader() {
	NOT_IMPLEMENTED();
}

bool ModuleLoader::loadModule(string modname) {
	string moddir = Config::getConfig()["modules"]["moddir"];
	if(moddir == "")
		moddir = DEFAULT_MODULE_DIR;
	string fname = moddir + "/mod_" + modname + ".so";

	log(LOG_INFO, "Loading module %s from %s.", modname.c_str(), fname.c_str());

	void* mod = dlopen(fname.c_str(), RTLD_LAZY | RTLD_LOCAL);
	if(!mod)
		log(LOG_ERR, "Failed to load module %s: %s", modname.c_str(), dlerror());

	return mod != 0;
}

bool ModuleLoader::loadModuleSet(string prefix, string set) {
	log(LOG_INFO, "Loading Module set (%s): %s", prefix.c_str(), set.c_str());
	string::size_type start = 0;

	while(start < set.length()) {
		string::size_type end = set.find_first_of(",", start);
		if(end == string::npos)
			end = set.length();
		string mod = set.substr(start, end - start);
		if(!loadModule(prefix + "_" + mod))
			return false;
		start = end + 1;
	}

	log(LOG_INFO, "Loaded module set (%s).", prefix.c_str());

	return true;
}

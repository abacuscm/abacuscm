/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *	with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include <dlfcn.h>

#include "moduleloader.h"
#include "logger.h"
#include "acmconfig.h"

using namespace std;

ModuleLoader::ModuleLoader() {
	moddir = Config::getConfig()["modules"]["moddir"];
	if(moddir == "")
		moddir = DEFAULT_MODULE_DIR;
}

ModuleLoader::~ModuleLoader() {
}

bool ModuleLoader::loadModule(string modname, bool global __attribute__((unused))) {
	string fname = moddir + "/" + modname + ".so";

	log(LOG_INFO, "Loading module %s", modname.c_str());

	void* mod = dlopen(fname.c_str(), RTLD_NOW | (global ? RTLD_GLOBAL : RTLD_LOCAL));
	if(!mod) {
		log(LOG_ERR, "Failed to load module %s: %s", modname.c_str(), dlerror());
		return false;
	}

	void (*initf)() = (void (*)())dlsym(mod, "abacuscm_mod_init");
	if (initf)
		initf();
	log(LOG_INFO, "Loaded module %s", modname.c_str());

	return true;
}

bool ModuleLoader::loadModuleSet(string prefix, string set, bool global) {
	log(LOG_INFO, "Loading Module set (%s): %s", prefix.c_str(), set.c_str());
	string::size_type start = 0;

	while(start < set.length()) {
		string::size_type end = set.find_first_of(",", start);
		if(end == string::npos)
			end = set.length();
		string mod = set.substr(start, end - start);
		if(!loadModule(prefix + "_" + mod, global))
			return false;
		start = end + 1;
	}

	log(LOG_INFO, "Loaded module set (%s).", prefix.c_str());

	return true;
}

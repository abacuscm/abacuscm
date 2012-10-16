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
#include <vector>

using namespace std;

ModuleLoader::ModuleLoader() {
	moddir = Config::getConfig()["modules"]["moddir"];
	if(moddir == "")
		moddir = DEFAULT_MODULE_DIR;
}

static vector<string> split_set(const std::string &set) {
	vector<string> ans;
	string::size_type start = 0;
	while(start < set.length()) {
		string::size_type end = set.find_first_of(",", start);
		if(end == string::npos)
			end = set.length();
		string mod = set.substr(start, end - start);
		ans.push_back(mod);
		start = end + 1;
	}
	return ans;
}

ModuleLoader::~ModuleLoader() {
	/* First pass: let all modules run cleanup code before the carpet is pulled
	 * out from any of them.
	 */
	for (vector<void *>::const_reverse_iterator mod = modules.rbegin(); mod != modules.rend(); ++mod) {
		void (*donef)() = (void (*)())dlsym(*mod, "abacuscm_mod_done");
		if (donef)
			donef();
	}

	/* Second pass: unload the modules */
	for (vector<void *>::const_reverse_iterator mod = modules.rbegin(); mod != modules.rend(); ++mod) {
		dlclose(*mod);
	}
}

bool ModuleLoader::loadModule(const string &modname, bool global) {
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

	modules.push_back(mod);

	return true;
}

bool ModuleLoader::loadModuleSet(const string &prefix, const string &set, bool global) {
	log(LOG_INFO, "Loading Module set (%s): %s", prefix.c_str(), set.c_str());

	vector<string> mods = split_set(set);
	for (vector<string>::const_iterator mod = mods.begin(); mod != mods.end(); ++mod)
	{
		if(!loadModule(prefix + "_" + *mod, global))
			return false;
	}

	log(LOG_INFO, "Loaded module set (%s).", prefix.c_str());

	return true;
}

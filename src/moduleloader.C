/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *	with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_DLOPEN
# include <dlfcn.h>
#else
# include <ltdl.h>
#endif

#include "moduleloader.h"
#include "logger.h"
#include "acmconfig.h"

using namespace std;

ModuleLoader::ModuleLoader() {
	string moddir = Config::getConfig()["modules"]["moddir"];
	if(moddir == "")
		moddir = DEFAULT_MODULE_DIR;
#ifndef HAVE_DLOPEN
	lt_dlinit();
	lt_dladdsearchdir(moddir.c_str());
#endif /* !HAVE_DLOPEN */
}

ModuleLoader::~ModuleLoader() {
#ifndef HAVE_DLOPEN
	lt_dlexit();
#endif /* !HAVE_DLOPEN */
}

bool ModuleLoader::loadModule(string modname, bool global __attribute__((unused))) {
	string fname = string("mod_") + modname;

	log(LOG_INFO, "Loading module %s from %s.so", modname.c_str(), fname.c_str());

#ifdef HAVE_DLOPEN
	void* mod = dlopen(fname.c_str(), RTLD_NOW | (global ? RTLD_GLOBAL : RTLD_LOCAL));
	if(!mod)
		log(LOG_ERR, "Failed to load module %s: %s", modname.c_str(), dlerror());
#else /* HAVE_DLOPEN */
	lt_dlhandle mod = lt_dlopenext(fname.c_str());
	if(!mod)
	{
		const char *error;
		error = lt_dlerror();
		log(LOG_ERR, "Failed to load module %s: %s", modname.c_str(), error ? error : "unknown error");
		return false;
	}
#endif /* HAVE_DLOPEN */

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

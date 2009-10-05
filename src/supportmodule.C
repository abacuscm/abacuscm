/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "supportmodule.h"
#include "logger.h"

#include <map>

using namespace std;

typedef map<string, SupportModule*> SupportModuleMap;

static SupportModuleMap smm;

SupportModule::SupportModule()
{
}

SupportModule::~SupportModule()
{
}

void SupportModule::init()
{
}

void SupportModule::registerSupportModule(const string& name, SupportModule* sm)
{
	SupportModuleMap::iterator i = smm.find(name);

	if(i != smm.end()) {
		log(LOG_WARNING, "Already have a support module registered by the name of '%s', replacing %p with %p.",
				name.c_str(), i->second, sm);
	}

	smm[name] = sm;
	sm->init();
}

SupportModule* SupportModule::get(const string& name)
{
	SupportModuleMap::iterator i = smm.find(name);

	if(i == smm.end()) {
		log(LOG_ERR, "Error locating support module: '%s'.", name.c_str());
		return NULL;
	}

	return i->second;
}

SupportModuleRegistrar::SupportModuleRegistrar(const string& nm, SupportModule* sm)
{
	SupportModule::registerSupportModule(nm, sm);
}

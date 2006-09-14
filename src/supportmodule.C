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

void SupportModule::registerSupportModule(string name, SupportModule* sm)
{
	SupportModuleMap::iterator i = smm.find(name);

	if(i != smm.end()) {
		log(LOG_WARNING, "Already have a support module registered by the name of '%s', replacing %p with %p.",
				name.c_str(), i->second, sm);
	}

	smm[name] = sm;
}

SupportModule* SupportModule::get(string name)
{
	SupportModuleMap::iterator i = smm.find(name);

	if(i == smm.end())
		return NULL;

	return i->second;
}

/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __MODULELOADER_H__
#define __MODULELOADER_H__

#include <string>

class ModuleLoader {
private:
	ModuleLoader();
	~ModuleLoader();
public:
	static bool loadModule(std::string module_name, bool global = false);
	static bool loadModuleSet(std::string prefix, std::string set, bool global = false);
};

#endif

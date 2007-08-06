/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *	with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __MODULELOADER_H__
#define __MODULELOADER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <string>

class ModuleLoader {
public:
	ModuleLoader();
	~ModuleLoader();
	static bool loadModule(std::string module_name, bool global = false);
	static bool loadModuleSet(std::string prefix, std::string set, bool global = false);
};

#endif

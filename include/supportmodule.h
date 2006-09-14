/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __SUPPORTMODULE_H__
#define __SUPPORTMODULE_H__

#include <string>

class SupportModule {
private:
	virtual ~SupportModule();

protected:
	SupportModule();

public:
	static void registerSupportModule(std::string name, SupportModule* sm);
	static SupportModule* get(std::string);
};

#define DEFINE_SUPPORT_MODULE_GETTER(c) inline c* get##c() { return dynamic_cast<c*>(SupportModule::get(#c)); }
#define DEFINE_SUPPORT_MODULE_REGISTRAR(c) static c __singleton_##c; static void __attribute__((constructor)) __init_##c() { SupportModule::registerSupportModule(#c, &__singleton_##c); }

#endif

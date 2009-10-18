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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <string>

class SupportModule {
protected:
	SupportModule();
	virtual ~SupportModule();

public:
	virtual void init();

	static void registerSupportModule(const std::string& name, SupportModule* sm);
	static SupportModule* get(const std::string& name);
};

class SupportModuleRegistrar {
public:
	SupportModuleRegistrar(const std::string &name, SupportModule* sm);
};

#define DEFINE_SUPPORT_MODULE_GETTER(c) inline c* get##c() { return /*dynamic_cast<c*>*/(c*)(SupportModule::get(#c)); }
#define DECLARE_SUPPORT_MODULE(c)                                       \
	private:                                                            \
		static c __singleton_instance;                                  \
		static SupportModuleRegistrar __registrar;                      \

#define DEFINE_SUPPORT_MODULE(c)                                        \
	c c::__singleton_instance;                                           \
	SupportModuleRegistrar c::__registrar(#c, &c::__singleton_instance); \

#endif

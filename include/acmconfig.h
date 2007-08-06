/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __ACMCONFIG_H__
#define __ACMCONFIG_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <map>
#include <string>
#include <iostream>

#define DEFAULT_SERVER_CONFIG		SYSCONFDIR "/abacus/server.conf"
#define DEFAULT_CLIENT_CONFIG		SYSCONFDIR "/abacus/client.conf"
#define DEFAULT_MARKER_CONFIG		SYSCONFDIR "/abacus/marker.conf"
#define DEFAULT_UDPRECEIVER_PORT	7368
#define DEFAULT_MODULE_DIR			PKGLIBDIR

typedef std::map<std::string, std::string> ConfigSection;
typedef std::map<std::string, ConfigSection> ConfigMap;

class Config {
private:
	static Config _config;

	ConfigMap _sections;

	Config();
	~Config();
public:
	bool load(const std::string& fname);
	void dumpConfig(std::ostream& os) const;

	ConfigSection& operator[] (const std::string& sect) {
		return _sections[sect];
	}

	static Config& getConfig();
};

inline
std::ostream& operator<<(std::ostream& os, const Config&c) {
	c.dumpConfig(os);
	return os;
}
#endif

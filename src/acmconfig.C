/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include <config.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "acmconfig.h"

using namespace std;

Config Config::_config;

Config& Config::getConfig() {
	return _config;
}

Config::Config() {

}

Config::~Config() {

}

bool Config::load(const string& fname) {
	ifstream conf_in(fname.c_str());
	if(!conf_in.good()) {
		cerr << "Failed to open config file " << fname << ": " << strerror(errno) << "\n";
		return false;
	}

	string cur_section("");
	int line_num = 1;

	while(conf_in) {
		ostringstream line_strm;

		while(conf_in.peek()=='\n') {
			conf_in.get();
			line_num++;
		}

		if(!conf_in.get(*line_strm.rdbuf()))
			break;

		string conf_line = line_strm.str();

		int len = conf_line.length();
		int p = 0;

		while(p < len && isspace(conf_line[p]))
			p++;

		if(p == len) // blank line.
			continue;

		if(conf_line[p] == '#') {
		} else if(conf_line[p] == '[') {
			p++;
			int p2 = p;
			while(p2 < len && conf_line[p2] != ']')
				p2++;
			cur_section = conf_line.substr(p, p2 - p);
		} else {
			int p2 = p;
			while(p2 < len && conf_line[p2] != '=')
				p2++;
			int p3 = p2;
			while(p3 && isspace(conf_line[--p3]));
			if(p3 < 0) {
				cerr << "Ignoring invalid config on line " << line_num << ": " << conf_line << '\n';
				continue;
			}

			string opt_name = conf_line.substr(p, p3 - p + 1);

			p2++;
			while(p2 < len && isspace(conf_line[p2]))
				p2++;

			p3 = len - 1;
			while(p3 > p2 && isspace(conf_line[p3]))
				p3--;

			string opt_value = conf_line.substr(p2, p3 - p2 + 1);

			if(cur_section == "")
				cerr << "config option on line " << line_num << " not in a config section\n";
			else {
				if (opt_value.substr(0, 2) == "~/") {
					const char *home = getenv("HOME");
					if (home) {
						opt_value = string(home) + opt_value.substr(1);
					}
				}
				_sections[cur_section][opt_name] = opt_value;
			}
		}
	}

	return true;
}

void Config::dumpConfig(ostream& os) const {
	ConfigMap::const_iterator i;
	for(i = _sections.begin(); i != _sections.end(); i++) {
		os << "[" << i->first << "]\n";

		ConfigSection::const_iterator j;
		for(j = i->second.begin(); j != i->second.end(); j++)
			os << j->first << " = " << j->second << '\n';
	}
}

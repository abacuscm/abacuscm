#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <map>
#include <string>
#include <iostream>

#define DEFAULT_SERVER_CONFIG		"/etc/abacus/server.conf"
#define DEFAULT_CLIENT_CONFIG		"/etc/abacus/client.conf"
#define DEFAULT_MARKER_CONFIG		"/etc/abacus/marker.conf"
#define DEFAULT_UDPRECEIVER_PORT	7368
#define DEFAULT_MODULE_DIR			"/usr/lib/abacus"

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

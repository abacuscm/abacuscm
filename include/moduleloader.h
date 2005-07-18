#ifndef __MODULELOADER_H__
#define __MODULELOADER_H__

#include <string>

class ModuleLoader {
private:
	ModuleLoader();
	~ModuleLoader();
public:
	static bool loadModule(std::string module_name);
	static bool loadModuleSet(std::string prefix, std::string set);
};

#endif

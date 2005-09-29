#ifndef __PROBLEMMARKER_H__
#define __PROBLEMMARKER_H__

#include <string>
#include <map>

class Buffer;

class ProblemMarker;
typedef ProblemMarker* (*ProblemMarkerFunctor)();

typedef std::map<std::string, std::string> ResultMap;

class ProblemMarker {
private:
	typedef std::map<std::string, ProblemMarkerFunctor> FunctorMap;
	static FunctorMap _functors;

	std::map<std::string, std::string> _attribs;
	uint32_t _prob_id;
	
protected:
	Buffer getProblemFile(std::string attrib);
	std::string attrib(std::string);
public:
	ProblemMarker();
	virtual ~ProblemMarker();

	virtual void mark(const Buffer& submission, const std::string& lang) = 0;

	static ProblemMarker* createMarker(std::string problemtype, uint32_t prob_id);
	static void registerMarker(std::string problemtype, ProblemMarkerFunctor);
};

#endif

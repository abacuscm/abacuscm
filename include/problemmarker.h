#ifndef __PROBLEMMARKER_H__
#define __PROBLEMMARKER_H__

#include <string>
#include <map>
#include <stdint.h>

#include "serverconnection.h"
#include "buffer.h"
#include "runinfo.h"

class ProblemMarker;
typedef ProblemMarker* (*ProblemMarkerFunctor)();

typedef std::map<std::string, std::string> ResultMap;

typedef struct {
	uint32_t prob_id;
	uint32_t hash;
	std::string lang;
	Buffer submission;
} MarkRequest;

class ProblemMarker {
private:
	typedef std::map<std::string, ProblemMarkerFunctor> FunctorMap;
	static FunctorMap _functors;

	AttributeMap _attribs;
	MarkRequest* _mr;
	ServerConnection* _server_con;
	std::string _workdir;

protected:
	Buffer* getProblemFile(std::string attrib);
	std::string attrib(std::string);
	std::string workdir();

	const Buffer& submission();
	std::string language();

    RunInfo *_run_info;

public:
	ProblemMarker();
	virtual ~ProblemMarker();

	virtual void mark() = 0;

	void setServerCon(ServerConnection* server_con);
	void setMarkRequest(MarkRequest* mr);
	void setProblemAttributes(AttributeMap &attrs);

	static ProblemMarker* createMarker(std::string problemtype);
	static void registerMarker(std::string problemtype, ProblemMarkerFunctor);
};

#endif

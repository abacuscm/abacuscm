/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __PROBLEMMARKER_H__
#define __PROBLEMMARKER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <string>
#include <map>
#include <stdint.h>
#include <sys/types.h>

#include "serverconnection.h"
#include "buffer.h"
#include "runinfo.h"
#include "misc.h"

class ProblemMarker;
typedef ProblemMarker* (*ProblemMarkerFunctor)();

typedef std::map<std::string, std::string> ResultMap;

typedef struct {
	uint32_t prob_id;
	uint32_t submission_id;
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

    bool _have_result;
    RunResult _result;
    std::map<std::string, std::string> _result_files;

    void setResult(RunResult result);
    void addResultFile(std::string filetype, std::string filename, off_t max_size = 0);

public:
	ProblemMarker();
	virtual ~ProblemMarker();

	virtual void mark() = 0;
	bool submitResult();

	void setServerCon(ServerConnection* server_con);
	void setMarkRequest(MarkRequest* mr);
	void setProblemAttributes(AttributeMap &attrs);

	static ProblemMarker* createMarker(std::string problemtype);
	static void registerMarker(std::string problemtype, ProblemMarkerFunctor);
};

#endif

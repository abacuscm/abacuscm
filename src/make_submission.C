/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "logger.h"
#include "acmconfig.h"
#include "serverconnection.h"
#include "messageblock.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>

using namespace std;

/**
 * Submits a solution to a problem from the command-line.
 */
int main(int argc, char **argv) {
	if(argc != 7) {
		cerr << "USAGE: " << *argv << " config username password problem language filename\n";
		return -1;
	}

	string username = argv[2];
        string password = argv[3];
        string problem  = argv[4];
        string language = argv[5];
        string filename = argv[6];

	ServerConnection::init();
	ServerConnection _server_con;

	Config &conf = Config::getConfig();
	conf.load(DEFAULT_CLIENT_CONFIG);

	conf.load(argv[1]);

	log(LOG_DEBUG, "Connecting ...");

	if(!_server_con.connect(conf["server"]["address"], conf["server"]["service"]))
		return -1;

	log(LOG_DEBUG, "Authenticating ...");

	if(!_server_con.auth(username, password))
		return -1;

        uint32_t problem_id = 0;
        vector<ProblemInfo> problems = _server_con.getProblems();
        for (unsigned int i = 0; i < problems.size(); i++)
            if (problems[i].code == problem)
            {
                problem_id = problems[i].id;
                break;
            }
        if (problem_id == 0)
        {
            log(LOG_ERR, "Unable to find problem with shortname '%s'", problem.c_str());
            _server_con.disconnect();
            return 1;
        }

        // Validate the language choice
        vector<string> languages = _server_con.getLanguages();
        bool foundLanguage = false;
        for (unsigned int i = 0; i < languages.size(); i++)
        {
            if (languages[i] == language)
            {
                foundLanguage = true;
                break;
            }
        }
        if (!foundLanguage)
        {
            log(LOG_ERR, "Invalid language '%s' specified! The valid languages are:", language.c_str());
            for (unsigned int i = 0; i < languages.size(); i++)
                log(LOG_ERR, "%s", languages[i].c_str());
            _server_con.disconnect();
            return 1;
        }

        ifstream in(filename.c_str());
        if (!in)
        {
            log(LOG_ERR, "Unable to open file '%s'", filename.c_str());
            _server_con.disconnect();
            return 1;
        }
        ostringstream is;
        is << in.rdbuf();
        in.close();
        string src = is.str();

        int fd = open(filename.c_str(), O_RDONLY);
        if (fd == -1)
        {
            log(LOG_ERR, "Unable to open file '%s' for reading", filename.c_str());
            lerror("open failed");
            _server_con.disconnect();
            return 1;
        }

        if (!_server_con.submit(problem_id, fd, language))
        {
            log(LOG_ERR, "Submission failed");
            _server_con.disconnect();
            return 1;
        }

        close(fd);

        // Let's try to work out the submission ID for this submission. This is
        // a little flaky right now since it ignores race conditions, but if we
        // assume that submissions for a single team are happening from a single
        // client connection (which we currently hold) then this is safe.
        //
        // We have 5 attempts at this before giving up (to cater for any really
        // big delays in getting our submission into the database).
        uint32_t submission_id = 0;
        for (int tries = 0; tries < 5; tries++)
        {
            // We need to sleep after submitting to give the Abacus server
            // a chance to add our submission to the database.
            sleep(1);

            SubmissionList submissions = _server_con.getSubmissions();
            uint32_t time = 0;
            for (SubmissionList::iterator it = submissions.begin();
                 it != submissions.end();
                 it++)
            {
                uint32_t s_problem_id = strtoul((*it)["prob_id"].c_str(), NULL, 10);
                uint32_t s_submission_id = strtoul((*it)["submission_id"].c_str(), NULL, 10);
                uint32_t s_time = strtoul((*it)["time"].c_str(), NULL, 10);

                if (s_problem_id != problem_id)
                    continue;

                if (s_time > time)
                {
                    time = s_time;
                    submission_id = s_submission_id;
                }
            }

            // We think we have the correct submission. Let's compare it to
            // what we actually have in the file.
            uint32_t sourceLength;
            char *source;
            log(LOG_DEBUG, "Retrieving source for submission %u", submission_id);
            if (!_server_con.getSubmissionSource(submission_id, &source, &sourceLength))
            {
                log(LOG_ERR, "Failed to retrieve source for submission %u", submission_id);
                _server_con.disconnect();
                return 1;
            }

            // Check that the source matches
            if (sourceLength == src.length() &&
                memcmp(source, src.c_str(), sourceLength) == 0)
            {
                // We've found it!
                log(LOG_DEBUG, "Submission id is %u", submission_id);
                break;
            }
            else
                submission_id = 0;
            delete[] source;
        }

        if (submission_id == 0)
            log(LOG_ERR, "Submitted, but unable to determine submission id");

	_server_con.disconnect();
	return 0;
}

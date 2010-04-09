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
#include <string>
#include <pthread.h>
#include <time.h>

using namespace std;

int main(int argc, char **argv) {
	if(argc < 5 || argc > 6) {
		cerr << "USAGE: " << *argv << " config username password (submission_id | submission_username problem_shortname)\n";
		return -1;
	}

	string username = argv[2];
        string password = argv[3];

        string submission_username;
	string problem_shortname;
        uint32_t submission_id = 0;

        if (argc == 5)
        {
            submission_id = strtoul(argv[4], NULL, 10);
            if (submission_id == 0) {
                log(LOG_ERR, "Invalid submission id %u", submission_id);
                return 1;
            }
        }
        else {
            submission_username = argv[4];
            problem_shortname = argv[5];
        }

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
        uint32_t user_id = 0;
        if (submission_id == 0)
        {
            // Running in fetch-latest-submission-for-user-on-problem mode,
            // so check the problem shortname and username for validity,
            // and obtain the relevant user ids.
            vector<ProblemInfo> problems = _server_con.getProblems();
            for (unsigned int i = 0; i < problems.size(); i++)
                if (problems[i].code == problem_shortname)
                {
                    problem_id = problems[i].id;
                    break;
                }
            if (problem_id == 0)
            {
                log(LOG_ERR, "Unable to find problem with shortname '%s'", problem_shortname.c_str());
                _server_con.disconnect();
                return 1;
            }

            vector<UserInfo> users = _server_con.getUsers();
            for (unsigned int i = 0; i < users.size(); i++)
                if (users[i].username == submission_username)
                {
                    user_id = users[i].id;
                    break;
                }
            if (user_id == 0)
            {
                log(LOG_ERR, "Unable to find user with username '%s'", submission_username.c_str());
                _server_con.disconnect();
                return 1;
            }

            uint32_t time = 0;
            SubmissionList submissions = _server_con.getSubmissionsForUser(user_id);
            for (typeof(submissions.begin()) it = submissions.begin();
                 it != submissions.end();
                 it++)
            {
                uint32_t s_problem_id = strtoul((*it)["prob_id"].c_str(), NULL, 10);
                uint32_t s_submission_id = strtoul((*it)["submission_id"].c_str(), NULL, 10);
                uint32_t s_time = strtoul((*it)["time"].c_str(), NULL, 10);
                int result = atoi((*it)["result"].c_str());
        
                log(LOG_DEBUG, "Submission %u was for problem %i at time %u with comment %s and result %d", s_submission_id, s_problem_id, s_time, (*it)["comment"].c_str(), atoi((*it)["result"].c_str()));
        
                // Check that the submission is for the right problem
                if (s_problem_id != problem_id)
                    continue;

                // If this submission occurred later and is correct, then it
                // supersedes any previous submission we've seen.
                if (s_time > time && result == CORRECT) {
                    submission_id = s_submission_id;
                    time = s_time;
                }
            }
            if (submission_id == 0)
            {
                log(LOG_ERR, "No correct submissions for user '%s' and problem '%s'", submission_username.c_str(), problem_shortname.c_str());
                _server_con.disconnect();
                return 1;
            }
        }

        // Finally, retrieve the source
        uint32_t sourceLength;
        char *source;
        log(LOG_DEBUG, "Retrieving source for submission %u", submission_id);
        if (!_server_con.getSubmissionSource(submission_id, &source, &sourceLength))
        {
            log(LOG_ERR, "Failed to retrieve source");
            _server_con.disconnect();
            return 1;
        }

        cout << string(source, sourceLength);

	_server_con.disconnect();
	return 0;
}

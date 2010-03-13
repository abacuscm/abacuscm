/**
 * Copyright (c) 2010 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __SCORE_H__
#define __SCORE_H__

/* A class that holds the scores for a contestant and provides convenience
 * functions to manipulating it
 */

#include <stdint.h>
#include <string>
#include <vector>

class Score {
private:
	/* Which of these fields are populated is up to the application */
	uint32_t _id;
	std::string _username;
	std::string _friendlyname;
	bool _contestant; /* Whether this is an official contestant */
	int _total_solved;
	uint32_t _total_time;

	/* Absolute value of each element is number of attempts. Positive indicates
	 * a successful attempt.
	 */
	std::vector<int> _solved;

public:
	struct CompareId {
		bool operator()(const Score &a, const Score &b) const {
			return a._id < b._id;
		}
	};

	/* Sorts the best score to the top */
	struct CompareRanking {
		bool operator()(const Score &a, const Score &b) const {
			if (a._total_solved != b._total_solved)
				return a._total_solved > b._total_solved;
			else
				return a._total_time < b._total_time;
		}
	};

	/* Like CompareRanking, but forces ties to break in a deterministic but
	 * arbitrary way - used to sort standings to make them stable when there
	 * are lots of teams on zero.
	 */
	struct CompareRankingStable {
		bool operator()(const Score &a, const Score &b) const {
			if (a._total_solved != b._total_solved)
				return a._total_solved > b._total_solved;
			else if (a._total_time != b._total_time)
				return a._total_time < b._total_time;
			else
				return a._username < b._username;  // stabilise the sort
		}
	};

	/* Identifies which score for a contestant is earlier in time */
	struct CompareAttempts {
		bool operator()(const Score &a, const Score &b) const {
			return a.getTotalAttempts() < b.getTotalAttempts();
		}
	};

	Score();
	virtual ~Score() {}

	uint32_t getId() const { return _id; }
	const std::string &getUsername() const { return _username; }
	const std::string &getFriendlyname() const { return _friendlyname; }
	bool isContestant() const { return _contestant; }
	int getTotalSolved() const { return _total_solved; }
	uint32_t getTotalTime() const { return _total_time; }
	const std::vector<int> getSolved() const { return _solved; }
	int getSolved(unsigned int problem) const {
		return problem < _solved.size() ? _solved[problem] : 0;
	}
	int getTotalAttempts() const;

	virtual void setId(uint32_t id);
	virtual void setUsername(const std::string &username);
	virtual void setFriendlyname(const std::string &friendlyname);
	virtual void setContestant(bool contestant);
	virtual void setTotalTime(uint32_t total_time);
	virtual void setSolved(const std::vector<int> &solved);
	virtual void setSolved(unsigned int problem, int attempts);
};

#endif /* !__SCORE_H__ */

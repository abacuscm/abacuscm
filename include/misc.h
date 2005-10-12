#ifndef __MISC_H__
#define __MISC_H__

#include <list>

template<typename _Type>
typename std::list<_Type>::iterator list_find(std::list<_Type>& l, const _Type& w) {
	typename std::list<_Type>::iterator i;
	for(i = l.begin(); i != l.end(); ++i)
		if(*i == w)
			return i;
	return l.end();
}

typedef enum {
    CORRECT = 0,
    WRONG = 1,
    TIME_EXCEEDED = 2,
    ABNORMAL = 3,
    COMPILE_FAILED = 4,
	JUDGE = 5,
    OTHER = 6
} RunResult;

extern char *runMessages[];

#endif

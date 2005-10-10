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

#endif

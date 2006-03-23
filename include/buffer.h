/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <iostream>

class Buffer {
private:
	unsigned char* _data;
	unsigned _size;
	unsigned _capacity;

	void resize(unsigned newsize);
public:
	Buffer(unsigned capacity = 0);
	Buffer(const Buffer& buffer);
	~Buffer();

	unsigned char operator[] (unsigned i) const { return _data[i]; }
	unsigned char& operator[] (unsigned i) { return _data[i]; }

	operator const unsigned char*() const { return _data; }
	operator unsigned char*() { return _data; }

	const unsigned char* data() const { return _data; }
	unsigned char* data() { return _data; }

	unsigned size() const { return _size; }

	void ensureCapacity(unsigned mincap);

	void appendData(const void* data, unsigned size);
	void appendData(const Buffer& data);

	void writeToStream(std::ostream& os) const;
};

inline
std::ostream& operator << (std::ostream& os, const Buffer& bfr) {
	bfr.writeToStream(os);
	return os;
}
#endif

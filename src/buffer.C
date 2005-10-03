#include "buffer.h"

#include <memory.h>

Buffer::Buffer(unsigned capacity) {
	_capacity = capacity;
	_size = 0;
	if(capacity)
		_data = new unsigned char[capacity];
	else
		_data = 0;
}

Buffer::Buffer(const Buffer& buffer) {
	_capacity = buffer._capacity;
	_size = buffer._size;
	_data = new unsigned char[_capacity];
	memcpy(_data, buffer._data, _size);
}

Buffer::~Buffer() {
	if(_data)
		delete []_data;
}

void Buffer::resize(unsigned newcap) {
	if(_capacity >= newcap)
		return;

	unsigned char* nbuf = new unsigned char[newcap];
	if(_data) {
		memcpy(nbuf, _data, _size);
		delete []_data;
	}
	
	_data = nbuf;
	_capacity = newcap;
}

void Buffer::appendData(const void* data, unsigned size) {
	resize(size + _size);
	memcpy(&_data[_size], data, size);
	_size += size;
}

void Buffer::writeToStream(std::ostream& os) const {
	os.write((const char*)_data, _size);
}

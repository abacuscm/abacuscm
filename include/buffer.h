#ifndef __BUFFER_H__
#define __BUFFER_H__

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

	void ensureCapacity(unsigned mincap);

	void appendData(const void* data, unsigned size);
	void appendData(const Buffer& data);
};

#endif

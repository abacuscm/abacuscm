#ifndef __MESSAGEBLOCK_H__
#define __MESSAGEBLOCK_H__

#include <string>
#include <map>

typedef std::map<std::string, std::string> MessageHeaders;

class MessageBlock {
private:
	std::string _message;
	MessageHeaders _headers;
	int _content_length;
	char *_content;
	char *_content_pos;
public:
	MessageBlock(std::string& message);
	MessageBlock();
	~MessageBlock();

	MessageHeaders::const_iterator begin() const { return _headers.begin(); };
	MessageHeaders::iterator begin() { return _headers.begin(); };

	MessageHeaders::const_iterator end() const { return _headers.end(); };
	MessageHeaders::iterator end() { return _headers.end(); };

/*	const std::string& operator[] (const std::string& name) const
		{ return _headers[name]; }; */
	std::string& operator[] (const std::string& name)
		{ return _headers[name]; };

	int content_size() const { return _content_length; };
	const char* content() const { return _content; };
	char operator[] (int c) const { return _content[c]; };
	
	bool setContent(const char* data, int size);

	/**
	 * used to construct a MB, return values are as follows:
	 * -1: An error has occured
	 *  0: All data used, more needed.
	 * >0: Number of bytes used, MB is complete.
	 *
	 * Note: once addBytes have returned >0 you _must_never_
	 * call addBytes again on the same object or it _will_
	 * break.
	 */
	int addBytes(const char* bytes, int count);
	
	void dump() const;
};

#endif

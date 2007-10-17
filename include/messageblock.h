/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __MESSAGEBLOCK_H__
#define __MESSAGEBLOCK_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <string>
#include <map>
#include <openssl/ssl.h>

typedef std::map<std::string, std::string> MessageHeaders;

class MessageBlock {
private:
	std::string _message;
	MessageHeaders _headers;
	int _content_length;
	bool _content_private;
	char *_content;
	char *_content_pos;

	bool writeBlockToSSL(const char *buffer, int length, SSL *ssl) const;
public:
	MessageBlock(const std::string& message);
	MessageBlock();
	~MessageBlock();

	MessageHeaders::const_iterator begin() const { return _headers.begin(); };
	MessageHeaders::iterator begin() { return _headers.begin(); };

	MessageHeaders::const_iterator end() const { return _headers.end(); };
	MessageHeaders::iterator end() { return _headers.end(); };

	const std::string& operator[] (const std::string& name) const
		{
			MessageHeaders::const_iterator i = _headers.find(name);
			if(i != _headers.end())
				return i->second;
			else {
				static std::string empty("");
				return empty;
			}
		};
	std::string& operator[] (const std::string& name)
		{ return _headers[name]; };
	bool hasAttribute(const std::string &name) const
		{ return _headers.find(name) != _headers.end(); }

	std::string& action() { return _message; }

	int content_size() const { return _content_length; };
	const char* content() const { return _content; };
	char operator[] (int c) const { return _content[c]; };

	/**
	 * Set the content of the MessageBlock.  Note that if you set make_private_copy
	 * to false then you must ensure that the content stays consistent for the
	 * duration of the existance of this MessageBlock.
	 */
	bool setContent(const char* data, int size, bool make_private_copy = true);

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

	/**
	 * Writes the MessageBlock to an SSL connection.  The caller
	 * is responsible for ensuring that we have a lock on the ssl
	 * connection - ie: no other thread may also be writing to
	 * this ssl connection.
	 */
	bool writeToSSL(SSL* ssl) const;

	/**
	 * Dumps the MB to log() with LOG_DEBUG
	 */
	void dump() const;
};

#endif

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
#include <memory>
#include <openssl/ssl.h>
#include "threadssl.h"

typedef std::map<std::string, std::string> MessageHeaders;

class MessageBlock {
private:
	std::string _message;
	std::string _message_id;
	MessageHeaders _headers;
	int _content_length;
	bool _content_private;
	char *_content;
	char *_content_pos;

	bool writeBlockToSSL(const char *buffer, int length, ThreadSSL *ssl) const;

	// Prevent copying, which would corrupt content
	MessageBlock &operator= (const MessageBlock &mb);
public:
	typedef MessageHeaders::const_iterator const_iterator;
	typedef MessageHeaders::iterator iterator;

	MessageBlock(const std::string& message);
	MessageBlock(const MessageBlock &mb);
	MessageBlock();
	~MessageBlock();

	/* Static factory to produce an error message */
	static std::auto_ptr<MessageBlock> error(const std::string &msg);
	/* static factory to produce a success message */
	static std::auto_ptr<MessageBlock> ok();

	MessageHeaders::const_iterator begin() const { return _headers.begin(); };
	MessageHeaders::iterator begin() { return _headers.begin(); };

	MessageHeaders::const_iterator end() const { return _headers.end(); };
	MessageHeaders::iterator end() { return _headers.end(); };

	const std::string& operator[] (const std::string& name) const;
	std::string& operator[] (const std::string& name)
		{ return _headers[name]; }
	bool hasAttribute(const std::string &name) const
		{ return _headers.find(name) != _headers.end(); }

	const std::string& action() const { return _message; }

	void setMessageId(const std::string &id) { _message_id = id; }
	const std::string &getMessageId() const { return _message_id; }

	int content_size() const { return _content_length; };
	const char* content() const { return _content; };
	char operator[] (int c) const { return _content[c]; };

	/**
	 * Set the content of the MessageBlock.  Note that if you set make_private_copy
	 * to false then you must ensure that the content stays consistent for the
	 * duration of the existence of this MessageBlock.
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
	bool writeToSSL(ThreadSSL* ssl) const;

	/**
	 * Returns the message as a binary string.
	 */
	std::string getRaw() const;

	/**
	 * Dumps the MB to log() with LOG_DEBUG
	 */
	void dump() const;
};

#endif

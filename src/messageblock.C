/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#include "messageblock.h"
#include "logger.h"
#include "threadssl.h"

#include <sstream>
#include <string>
#include <cassert>

using namespace std;

MessageBlock::MessageBlock(const string& message) {
	_message = message;
	_content_length = 0;
	_content = NULL;
	_content_pos = NULL;
	_content_private = true;
}

MessageBlock::MessageBlock() {
	_content_length = 0;
	_content = NULL;
	_content_pos = NULL;
	_content_private = true;
}

MessageBlock::MessageBlock(const MessageBlock &mb) {
	_message = mb._message;
	_message_id = mb._message_id;
	_headers = mb._headers;
	_content_length = mb._content_length;
	if (mb._content) {
		_content = new char[_content_length];
		memcpy(_content, mb._content, _content_length);
		if (mb._content_pos)
			_content_pos = _content + (mb._content_pos - mb._content);
		else
			_content_pos = NULL;
		_content_private = true;
	}
	else {
		_content = NULL;
		_content_pos = NULL;
		_content_private = true;
	}
}

MessageBlock::~MessageBlock() {
	if(_content && _content_private)
		delete []_content;
}

auto_ptr<MessageBlock> MessageBlock::error(const std::string &msg) {
	auto_ptr<MessageBlock> mb(new MessageBlock("err"));
	(*mb)["msg"] = msg;
	return mb;
}

auto_ptr<MessageBlock> MessageBlock::ok() {
	return auto_ptr<MessageBlock>(new MessageBlock("ok"));
}

bool MessageBlock::setContent(const char* data, int size, bool make_private_copy) {
	if (!data || !size) {
		if (_content && _content_private)
			delete []_content;
		_content = NULL;
		_content_pos = NULL;
		_content_length = 0;
		_content_private = true;
		return true;
	}
	char *new_block = make_private_copy ? new char[size] : const_cast<char*>(data);
	char bfr[21];
	if (!new_block)
		return false;
	if (_content && _content_private)
		delete []_content;
	if (make_private_copy)
		memcpy(new_block, data, size);
	_content_length = size;
	_content = new_block;
	_content_private = make_private_copy;
	_content_pos = _content + size;
	sprintf(bfr, "%d", size);
	_headers["content-length"] = bfr;
	return true;
}

void MessageBlock::dump() const {
	MessageHeaders::const_iterator i;
	log(LOG_DEBUG, "Dumping message: %s %s", _message.c_str(), _message_id.c_str());
	for(i = _headers.begin(); i != _headers.end(); i++)
		log(LOG_DEBUG, "%s:%s", i->first.c_str(), i->second.c_str());
	if(_content_length)
		log(LOG_DEBUG, "The message has content of %d bytes", _content_length);
	log(LOG_DEBUG, "-----");
}

int MessageBlock::addBytes(const char* bytes, int count) {
	if(_content_pos) { // We are busy collecting content.
		int bytesleft = _content_length - (_content_pos - _content);
		int to_copy = count < bytesleft ? count : bytesleft;
		memcpy(_content_pos, bytes, to_copy);
		_content_pos += to_copy;
		return count < bytesleft ? 0 : bytesleft;
	} else {
		int chowed = 0;
		while(count) {
			int nl_pos = 0;
			while(nl_pos < count && bytes[nl_pos] != '\n')
				nl_pos++;
			if(nl_pos == count) { //no newline char
				log(LOG_DEBUG, "Buffering ...");
				// TODO:  enforce some max line length ?
				char *new_buf = new char[_content_length + count];
				if(_content) {
					memcpy(new_buf, _content, _content_length);
					delete []_content;
				}
				memcpy(new_buf + _content_length, bytes, count);
				_content = new_buf;
				_content_length += count;

				chowed += count;
				count = 0;
				bytes = NULL;
			} else {
				string line;
				if(_content) {
					line = string(_content, _content_length);
					delete []_content;
					_content = NULL;
					_content_length = 0;
				}

				line += string(bytes, nl_pos);
				chowed += nl_pos + 1;
				bytes += nl_pos + 1;
				count -= nl_pos + 1;

				if(_message == "") {
					string::size_type p = line.find(' ');
					if (p != string::npos)
					{
						_message = line.substr(0, p);
						_message_id = line.substr(p + 1);
					}
					else
					{
						_message = line;
					}
				}
				else if(line == "") {
//					log(LOG_DEBUG, "Empty line, end of headers");
					MessageHeaders::iterator i = _headers.find("content-length");
					if (i != _headers.end())
						_content_length = atol(i->second.c_str());
					else
						_content_length = 0;
					if(_content_length > 0) {
						_content = new char[_content_length];
						_content_pos = _content;
						int res = addBytes(bytes, count);
						if(res <= 0)
							return res;
						return chowed + res;
					} else
						return chowed;
				} else {
					int colon_pos = line.find(':', 0);
					if(colon_pos != (int)string::npos) {
						_headers[line.substr(0, colon_pos)] =
							line.substr(colon_pos+1);
					} else {
						log(LOG_NOTICE, "Received header without colon in: %s", line.c_str());
						return -1;
					}
				}
			}
		}
		return 0;
	}

	return -1;
}

bool MessageBlock::writeBlockToSSL(const char *buffer, int length, ThreadSSL *ssl) const {
	ThreadSSL::Status status = ssl->write(buffer, length, ThreadSSL::BLOCK_FULL);
	if (status.err != SSL_ERROR_NONE)
	{
		log_ssl_errors_with_err("SSL_write", status.err);
		return false;
	}
	assert(status.processed == length);
	return true;
}

string MessageBlock::getRaw() const {
	ostringstream block;
	block << _message;
	if (!_message_id.empty())
		block << ' ' << _message_id;
	block << '\n';
	for(MessageHeaders::const_iterator i = _headers.begin(); i != _headers.end(); ++i)
		block << i->first << ':' << i->second << '\n';
	block << '\n';

	if (_content) {
		block.write(_content, _content_length);
	}

	return block.str();
}

bool MessageBlock::writeToSSL(ThreadSSL* ssl) const {
	string raw = getRaw();
	return writeBlockToSSL(raw.data(), raw.size(), ssl);
}

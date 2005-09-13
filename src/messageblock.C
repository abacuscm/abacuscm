#include "messageblock.h"
#include "logger.h"

#include <sys/poll.h>
#include <sstream>

using namespace std;

MessageBlock::MessageBlock(const string& message) {
	_message = message;
	_content_length = 0;
	_content = NULL;
	_content_pos = NULL;
}

MessageBlock::MessageBlock() {
	_content_length = 0;
	_content = NULL;
	_content_pos = NULL;
}

MessageBlock::~MessageBlock() {
	if(_content)
		delete []_content;
}

bool MessageBlock::setContent(const char* data, int size) {
	if(!data || !size) {
		if(_content)
			delete []_content;
		_content = NULL;
		_content_pos = NULL;
		_content_length = 0;
		return true;
	}
	char *new_block = new char[size];
	char bfr[21];
	if(!new_block)
		return false;
	if(_content)
		delete []_content;
	_content_length = size;
	_content = new_block;
	memcpy(_content, data, size);
	_content_pos = _content + size;
	sprintf(bfr, "%d", size);
	_headers["content-length"] = bfr;
	return true;
}

void MessageBlock::dump() const {
	MessageHeaders::const_iterator i;
	log(LOG_DEBUG, "Dumping message: %s", _message.c_str());
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

				if(_message == "")
					_message = line;
				else if(line == "") {
//					log(LOG_DEBUG, "Empty line, end of headers");
					_content_length = atol(_headers["content-length"].c_str());
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

bool MessageBlock::writeBlockToSSL(const char *buffer, int length, SSL *ssl) const {
	while(length) {
		int res = SSL_write(ssl, buffer, length);
		if(res > 0) {
			buffer += res;
			length -= res;
		} else {
			struct pollfd sslfd;
			sslfd.fd = SSL_get_fd(ssl);
			sslfd.events = 0;
			sslfd.revents = 0;
			switch(SSL_get_error(ssl, res)) {
			case SSL_ERROR_NONE:
				log(LOG_DEBUG, "Not supposed to get here! (%s:%d)", __FILE__,
						__LINE__);
				break;
			case SSL_ERROR_WANT_READ:
				log(LOG_DEBUG, "Eek, we are blocking on a read in %s "
						"(%s:%d) where we are trying to send data out!",
						__PRETTY_FUNCTION__, __FILE__, __LINE__);
				sslfd.events |= POLLOUT;
				break;
			case SSL_ERROR_WANT_WRITE:
				sslfd.events |= POLLIN;
				break;
			default:
				log_ssl_errors("SSL_write");
				return false;
			}
			if(!sslfd.events)
				break;
			if(poll(&sslfd, 1, 0) < 0) {
				lerror("poll");
				return false;
			}
		}
	}
	return true;
}

bool MessageBlock::writeToSSL(SSL* ssl) const {
	ostringstream init_block;
	init_block << _message << '\n';
	for(MessageHeaders::const_iterator i = _headers.begin(); i != _headers.end(); ++i)
		init_block << i->first << ':' << i->second << '\n';
	init_block << '\n';

	string headerblock = init_block.str();
	int length = headerblock.length();
	const char *buffer = headerblock.c_str();

	if(!writeBlockToSSL(buffer, length, ssl))
		return false;

	if(_content)
		return writeBlockToSSL(_content, _content_length, ssl);

	return true;
}

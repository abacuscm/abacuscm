#ifndef __MESSAGEBUILDER_H__
#define __MESSAGEBUILDER_H__

#include <stdint.h>
#include <map>

class Message;

class MessageBuilder {
private:
	static std::map<uint32_t, MessageBuilder*> _typemap;

	int _message_type_id;
protected:
	MessageBuilder(uint32_t message_type_id);
	virtual ~MessageBuilder();

	/**
	 * a blob will get passed to this function, it needs to transform
	 * this into a Message object (or a subclass thereof) and return
	 * that.  How it does this is up to it.
	 */
	virtual Message* build(uint32_t message_size, const uint8_t * blob) = 0;
public:
	uint32_t message_type_id();

	static Message* buildMessage(uint32_t message_size, const uint8_t * blob);
};

#endif

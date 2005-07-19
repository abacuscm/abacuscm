#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <stdint.h>

class Message {
private:
	uint32_t _server_id;
	uint32_t _message_id;
public:
	Message(uint32_t server_id, uint32_t message_id);
	virtual ~Message();

	/**
	 * This function is a placeholder for implementing the
	 * functionality required to act upon the message.
	 */
	virtual void process() = 0;

	/**
	 * This function needs to return the message_type_id for
	 * the message type.  This must be the same for all instances
	 * of the same class of message, and should only be implemented
	 * by the lowest level message classes.
	 */
	virtual uint32_t message_type_id() = 0;
};

#endif

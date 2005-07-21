#ifndef __MESSAGE_CREATESERVER_H__
#define __MESSAGE_CREATESERVER_H__

#include "message.h"

#include <string>

class Message_CreateServer : public Message {
private:
	std::string _name;
	uint32_t _server_id;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	Message_CreateServer(const std::string& name, uint32_t id);
	virtual ~Message_CreateServer();

	void addAttribute(const std::string& name, const std::string& value);

	virtual void process() const;
	virtual uint16_t message_type_id() const;
};

#endif

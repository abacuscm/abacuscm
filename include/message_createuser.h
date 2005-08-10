#ifndef __MESSAGE_CREATEUSER_H__
#define __MESSAGE_CREATEUSER_H__

#include "message.h"

#include <string>
#include <map>

class Message_CreateUser : public Message {
private:
	std::string _name;
	std::string _password;
	uint32_t _user_id;
	uint32_t _type;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	Message_CreateUser(const std::string& name, const std::string& pass, uint32_t id, uint32_t type);
	virtual ~Message_CreateUser();

	virtual bool process() const;
	virtual uint16_t message_type_id() const;
};

#endif
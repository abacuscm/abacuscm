/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __MESSAGE_CREATEUSER_H__
#define __MESSAGE_CREATEUSER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "message.h"

#include <string>
#include <map>

class Message_CreateUser : public Message {
private:
	std::string _username;
	std::string _friendlyname;
	std::string _password;
	uint32_t _user_id;
	uint32_t _type;
	uint32_t _group_id;
	uint32_t _requestor_id;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);

	virtual bool int_process() const;
public:
	Message_CreateUser();
	Message_CreateUser(const std::string& username, const std::string& friendlyname, const std::string& pass, uint32_t id, uint32_t type, uint32_t group_id, uint32_t requester_id);
	virtual ~Message_CreateUser();

	virtual uint16_t message_type_id() const;
};

#endif

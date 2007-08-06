/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __MESSAGE_CREATESERVER_H__
#define __MESSAGE_CREATESERVER_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "message.h"

#include <string>
#include <map>

class Message_CreateServer : public Message {
private:
	typedef std::map<std::string, std::string> AttribMap;
	std::string _name;
	uint32_t _server_id;
	AttribMap _attribs;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	Message_CreateServer();
	Message_CreateServer(const std::string& name, uint32_t id);
	virtual ~Message_CreateServer();

	void addAttribute(const std::string& name, const std::string& value);

	virtual bool process() const;
	virtual uint16_t message_type_id() const;
};

#endif

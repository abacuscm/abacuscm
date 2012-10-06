/**
 * Copyright (c) 2012 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __MESSAGE_CREATEGROUP_H__
#define __MESSAGE_CREATEGROUP_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "message.h"

#include <string>

class Message_CreateGroup : public Message {
private:
	std::string _groupname;
	uint32_t _group_id;
protected:
	virtual uint32_t storageRequired();
	virtual uint32_t store(uint8_t *buffer, uint32_t size);
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);

	virtual bool int_process() const;
public:
	Message_CreateGroup();
	Message_CreateGroup(const std::string& groupname, uint32_t id);

	virtual uint16_t message_type_id() const;
};

#endif

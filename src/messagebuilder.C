#include "messagebuilder.h"
#include "logger.h"

std::map<uint32_t, MessageBuilder*> MessageBuilder::_typemap;

MessageBuilder::MessageBuilder(uint32_t message_type_id) {
	_message_type_id = message_type_id;
	if(_typemap[_message_type_id] != 0) {
		log(LOG_CRIT, "Duplicate message_type_id 0x%x!!", _message_type_id);
		exit(-1);
	}
	_typemap[_message_type_id] = this;
	log(LOG_INFO, "Registered MessageBuilder for message_type_id 0x%x at 0x%p.", _message_type_id, this);
}

MessageBuilder::~MessageBuilder() {
	_typemap[_message_type_id] = 0;
	log(LOG_INFO, "De-registered MessageBuilder for message_type_id 0x%x.", _message_type_id);
}

#ifndef __NETLIB_H__
#define __NETLIB_H__

#include <stdint.h>

class Message;

class PeerMessenger {
private:
	static PeerMessenger *_messenger;
public:
	PeerMessenger();
	virtual ~PeerMessenger();

	virtual bool initialise() = 0;
	virtual void deinitialise() = 0;
	virtual bool SendMessage(uint32_t server_id, const Message * message) = 0;

	static void setMessenger(PeerMessenger *messenger);
	static PeerMessenger * getMessenger();
};

#endif

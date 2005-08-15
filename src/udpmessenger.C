#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "peermessenger.h"
#include "logger.h"
#include "config.h"
#include "message.h"

#include <map>

using namespace std;

#define ST_FRAME_SIZE		13

#define MAX_FRAGMENT_SIZE	1000
#define BUFFER_SIZE			1500

struct st_frame {
	uint32_t server_id;
	uint32_t message_id;
	uint16_t fragment_num;
	uint16_t fragment_len;
	uint8_t data[1];
} __attribute__ ((packed));

struct st_fragment {
	uint16_t fragment_length;
	uint8_t data[1];
} __attribute__ ((packed));

struct st_fragment_collection {
	uint16_t num_fragments;
	uint16_t fragments_received;
	map<uint16_t, st_fragment*> fragment;
};

class UDPPeerMessenger : public PeerMessenger {
private:
	int _sock;
	uint16_t _max_fragment_size;

	map<uint32_t, struct sockaddr_in> addrmap;

	const sockaddr_in* getSockAddr(uint32_t server_id);

	bool startup();
public:
	UDPPeerMessenger();
	virtual ~UDPPeerMessenger();

	virtual bool initialise();
	virtual void deinitialise();
	virtual bool sendMessage(uint32_t server_id, const Message * message);
	virtual Message* getMessage();
};

UDPPeerMessenger::UDPPeerMessenger() {
	_sock = -1;
}

UDPPeerMessenger::~UDPPeerMessenger() {
	if(_sock > 0)
		deinitialise();
}

bool UDPPeerMessenger::initialise() {
	if(_sock < 0)
		return startup();
	return true;
}

void UDPPeerMessenger::deinitialise() {
	if(_sock > 0) {
		close(_sock);
		_sock = -1;
	}
}

bool UDPPeerMessenger::startup() {
	struct sockaddr_in sock_addr;
	uint16_t portnum;
	Config &config = Config::getConfig();
	
	_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(_sock < 0) {
		lerror("socket");
		goto err;
	}

	portnum = atol(config["udpmessenger"]["port"].c_str());
	if(!portnum)
		portnum = DEFAULT_UDPRECEIVER_PORT;

	log(LOG_INFO, "Binding UDP Peer Messenger to port %d.", portnum);
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(portnum);
	sock_addr.sin_addr.s_addr = INADDR_ANY;

	if(bind(_sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
		lerror("bind");
		goto err;
	}
	
	_max_fragment_size = MAX_FRAGMENT_SIZE;

	log(LOG_INFO, "UDPPeerMessenger started up");
	return true;

err:
	if(_sock > 0) {
		close(_sock);
		_sock = -1;
	}
	return false;
}

bool UDPPeerMessenger::sendMessage(uint32_t server_id, const Message * message) {
	const uint8_t *message_data;
	uint32_t message_len;
	if(!message->getBlob(&message_data, &message_len))
		return false;
	
	const struct sockaddr_in *dest = getSockAddr(server_id);
	if(!dest)
		return false;
	
	// determine the number of fragments, always rounding up.
	uint16_t numfrags = (message_len + _max_fragment_size - 1) / _max_fragment_size;
	uint16_t base_frag_size = message_len / numfrags;
	uint16_t num_large_frags = message_len % numfrags;
	
	union {
		uint8_t buffer[BUFFER_SIZE];
		struct st_frame frame;
	};

	frame.server_id = message->server_id();
	frame.message_id = message->message_id();
	
	for(uint16_t i = 1; i <= numfrags; i++) {
		frame.fragment_num = i;
		frame.fragment_len = base_frag_size;
		if(i <= num_large_frags)
			frame.fragment_len++;

		memcpy(frame.data, message_data, frame.fragment_len);

		int packetsize = frame.fragment_len + ST_FRAME_SIZE - 1;

		int result = sendto(_sock, buffer, packetsize, 0, (const struct sockaddr*)dest, sizeof(*dest));
		if(result < 0) {
			lerror("sendto");
			return false;
		} else if(result < packetsize) {
			log(LOG_ERR, "Only %d bytes of %d got transmitted!  This is a serious problem!",
					result, packetsize);
		}
	}
	
	return true;
}

Message* UDPPeerMessenger::getMessage() {
	ssize_t bytes_received;
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	union {
		uint8_t buffer[BUFFER_SIZE];
		struct st_frame frame;
	};

	Message *message = NULL;

	while(!message) {
		bytes_received = recvfrom(_sock, buffer, BUFFER_SIZE, MSG_TRUNC, (struct sockaddr*)&from, &fromlen);
		if(bytes_received == -1) {
			if(errno != EINTR)
				lerror("recvfrom");
			return NULL;
		}
		log(LOG_DEBUG, "received a frame of size %d", (int)bytes_received);
		NOT_IMPLEMENTED();
	}
	return message;
}

static UDPPeerMessenger _udpPeerMessenger;

static void udp_peer_messenger_init() __attribute__ ((constructor));
static void udp_peer_messenger_init()
{
	// double check that gcc actually did the right thing with the __attribute__ ((packed)).
	// If we don't register the messenger abacusd will detect it and abort.
	if(sizeof(st_frame) != ST_FRAME_SIZE)
		log(LOG_ERR, "Compilation error detected, sizeof(struct st_frame) should be %d but is in fact %d", ST_FRAME_SIZE, (int)sizeof(st_frame));
	else
		PeerMessenger::setMessenger(&_udpPeerMessenger);
}

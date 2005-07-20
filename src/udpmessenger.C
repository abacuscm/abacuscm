#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>

#include "peermessenger.h"
#include "logger.h"
#include "config.h"

#define SIGNATURE_SIZE		128
#define ST_MESSAGE_SIZE		141

#define BUFFER_SIZE			10000

struct st_message {
	uint8_t signature[SIGNATURE_SIZE];
	uint32_t server_id;
	uint32_t seq_num;
	uint16_t message_type;
	uint16_t length;
	uint8_t data[1];
} __attribute__ ((packed));

struct st_signature {
	uint8_t md5[16];
	uint8_t padding[SIGNATURE_SIZE - 16];
} __attribute__ ((packed));

class UDPPeerMessenger : public PeerMessenger {
private:
	int _sock;
	
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
	NOT_IMPLEMENTED();
	return false;
}

Message* UDPPeerMessenger::getMessage() {
	ssize_t bytes_received;
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);
	union {
		uint8_t buffer[BUFFER_SIZE];
		struct st_message message;
	};

	bytes_received = recvfrom(_sock, buffer, BUFFER_SIZE, MSG_TRUNC, (struct sockaddr*)&from, &fromlen);
	log(LOG_DEBUG, "received a packet of size %d", (int)bytes_received);

	NOT_IMPLEMENTED();
	sleep(1);
	return NULL;
}

static UDPPeerMessenger _udpPeerMessenger;

static void udp_peer_messenger_init() __attribute__ ((constructor));
static void udp_peer_messenger_init()
{
	// double check that gcc actually did the right thing with the __attribute__ ((packed)).
	// If we don't register the messenger abacusd will detect it and abort.
	if(sizeof(st_message) != ST_MESSAGE_SIZE)
		log(LOG_ERR, "Compilation error detected, sizeof(struct st_message) should be %d but is in fact %d", ST_MESSAGE_SIZE, (int)sizeof(st_message));
	else if(sizeof(st_signature) != SIGNATURE_SIZE)
		log(LOG_ERR, "Compilation error detected, sizeof(struct st_signature) should be %d but is in fact %d", SIGNATURE_SIZE, (int)sizeof(st_signature));
	else
		PeerMessenger::setMessenger(&_udpPeerMessenger);
}

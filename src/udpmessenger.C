#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>

#include "peermessenger.h"
#include "logger.h"
#include "config.h"

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
	NOT_IMPLEMENTED();
	sleep(1);
	return NULL;
}

static UDPPeerMessenger _udpPeerMessenger;

static void udp_peer_messenger_init() __attribute__ ((constructor));
static void udp_peer_messenger_init()
{
	PeerMessenger::setMessenger(&_udpPeerMessenger);
}

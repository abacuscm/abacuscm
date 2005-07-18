#include "peermessenger.h"

PeerMessenger* PeerMessenger::_messenger = 0;

PeerMessenger::PeerMessenger() {
}

PeerMessenger::~PeerMessenger() {
	if(_messenger)
		_messenger->deinitialise();
}

void PeerMessenger::setMessenger(PeerMessenger *messenger) {
	if(messenger->initialise()) {
		if(_messenger)
			_messenger->deinitialise();
		_messenger = messenger;
	}
}

PeerMessenger* PeerMessenger::getMessenger() {
	return _messenger;
}

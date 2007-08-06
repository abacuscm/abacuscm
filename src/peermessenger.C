/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#if HAVE_CONFIG_H
# include <config.h>
#endif
#include "peermessenger.h"

PeerMessenger* PeerMessenger::_messenger = 0;

PeerMessenger::PeerMessenger() {
}

PeerMessenger::~PeerMessenger() {
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

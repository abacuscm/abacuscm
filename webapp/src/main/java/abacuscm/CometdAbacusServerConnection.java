/*  Copyright (C) 2010-2011  Bruce Merry and Carl Hultquist
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

package abacuscm;

import java.util.Date;
import java.util.HashMap;
import java.util.Map;
import java.util.Collections;

import org.cometd.bayeux.client.ClientSessionChannel;
import org.cometd.bayeux.server.ServerSession;

import org.eclipse.jetty.util.log.Log;

import static abacuscm.AbacusService.logInfo;
import static abacuscm.AbacusService.logWarn;

public class CometdAbacusServerConnection
	extends AbacusServerConnection {
	private final ServerSession mySession;

	public CometdAbacusServerConnection(final ServerSession session) {
		super();

		mySession = session;
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	protected void handleIncomingMessage(MessageBlock mb) {
		Map<String, Object> msgOut = new HashMap<String, Object>();
		msgOut.put("name", mb.getName());
		msgOut.put("headers", mb.getHeaders());
		if (mb.getContent() != null) {
			msgOut.put("content", mb.getContent());
		}
		mySession.deliver(mySession.getLocalSession(), "/service/abacus", msgOut, null);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	protected void connectionReset() {
		sendConnectionReset();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	protected void log(String message, Throwable throwable) {
		logWarn(mySession, message, throwable);
		mySession.deliver(mySession.getLocalSession(),
						  "/service/log", message, null);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	protected void log(String message) {
		logInfo(mySession, message);
		mySession.deliver(mySession.getLocalSession(),
						  "/service/log", message, null);
	}

	/**
	 * Informs the client that the connection has been closed (and that they
	 * will need to re-authenticate).
	 */
	public void sendConnectionReset() {
		Map<String, Object> msg = new HashMap<String, Object>();
		msg.put("name", "connectionreset");
		msg.put("headers", new HashMap<String, String>());

		mySession.deliver(mySession.getLocalSession(), "/service/abacus", msg, null);
	}

}

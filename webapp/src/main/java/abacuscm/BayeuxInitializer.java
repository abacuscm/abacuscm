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

import java.io.IOException;
import javax.servlet.GenericServlet;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;

import org.cometd.bayeux.server.BayeuxServer;
import org.cometd.server.ext.AcknowledgedMessagesExtension;
import org.cometd.server.ext.TimesyncExtension;

public class BayeuxInitializer extends GenericServlet {
	private AbacusService abacus;

	@Override
	public void init() throws ServletException {
		BayeuxServer bayeux = (BayeuxServer)getServletContext().getAttribute(BayeuxServer.ATTRIBUTE);
		bayeux.addExtension(new AcknowledgedMessagesExtension());
		bayeux.addExtension(new TimesyncExtension());
		abacus = new AbacusService(bayeux, getServletConfig());
	}

	@Override
	public void service(ServletRequest request, ServletResponse response) throws ServletException, IOException {
		throw new ServletException();
	}

	@Override
	public void destroy() {
		if (abacus != null) {
			abacus.destroy();
			abacus = null;
		}
	}
}

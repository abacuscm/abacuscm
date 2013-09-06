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

import org.cometd.server.AbstractService;
import org.cometd.server.transport.HttpTransport;
import org.cometd.bayeux.server.BayeuxServer;
import org.cometd.bayeux.server.ServerSession;
import org.cometd.bayeux.client.ClientSessionChannel;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.Timer;
import java.util.TimerTask;
import java.util.Arrays;
import java.io.File;
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;

import java.security.KeyStore;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocketFactory;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpServletRequest;

import org.eclipse.jetty.util.log.Log;

public class AbacusService extends AbstractService {
	/**
	 * String used to store attributes in the shared HttpSession.
	 */
	private static final String HTTP_SESSION_ATTRIBUTE = "abacuscm";

	private static class SessionData {
		public AbacusServerConnection myConn;
		public String myUsername = null;
		public String myPassword = null;
	}

	private static class HttpSessionData {
		public String myUsername = null;
		public String myPassword = null;
	}

	private Map<String, SessionData> mySessions;
	private SSLSocketFactory mySocketFactory;

	private int abacusPort;
	private String abacusHost;

	private void initSocketFactory(InputStream trustStore, String trustStorePassword) {
		try {
			char[] passPhrase = trustStorePassword.toCharArray();

			SSLContext context = SSLContext.getInstance("TLSv1");
			KeyManagerFactory kmf = KeyManagerFactory.getInstance("SunX509");
			KeyStore ks = KeyStore.getInstance("JKS");
			TrustManagerFactory tmf = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());

			ks.load(trustStore, passPhrase);
			kmf.init(ks, passPhrase);
			tmf.init(ks);
			context.init(null, tmf.getTrustManagers(), null);

			mySocketFactory = (SSLSocketFactory) context.getSocketFactory();
		}
		catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	public AbacusService(BayeuxServer bayeux, ServletConfig config) {
		super(bayeux, "abacus");
		String trustStoreFile = config.getInitParameter("trustStoreFile");
		String trustStoreURL = config.getInitParameter("trustStoreURL");
		String trustStorePassword = config.getInitParameter("trustStorePassword");
		if (trustStoreURL == null && trustStoreFile == null) {
			throw new IllegalArgumentException("init-param trustStoreFile or trustStoreURL is required");
		}
		if (trustStorePassword == null) {
			throw new IllegalArgumentException("init-param trustStorePassword is required");
		}

		InputStream trustStore = null;
		if (trustStoreURL != null) {
			try {
				trustStore = new URL(trustStoreURL).openStream();
			}
			catch (IOException e) {
				throw new RuntimeException(e);
			}
		}
		if (trustStore == null && trustStoreFile != null) {
			trustStore = config.getServletContext().getResourceAsStream(trustStoreFile);
		}
		if (trustStore == null) {
			throw new RuntimeException("Failed to load trust store");
		}

		try {
			initSocketFactory(trustStore, trustStorePassword);
		}
		finally {
			try {
				trustStore.close();
			}
			catch (IOException e) {
			}
		}

		addService("/service/abacus", "service");
		mySessions = new HashMap<String, SessionData>();

		abacusHost = config.getInitParameter("abacusHost");
		if (abacusHost == null) {
			abacusHost = "localhost";
		}
		String portStr = config.getInitParameter("abacusPort");
		if (portStr == null) {
			abacusPort = 7368;
		}
		else {
			abacusPort = Integer.parseInt(portStr);
		}
	}

	private SessionData getSessionData(ServerSession session) throws IOException {
		final String id = session.getId();
		SessionData data;
		synchronized(mySessions) {
			data = mySessions.get(id);
			if (data == null) {
				data = new SessionData();
				final CometdAbacusServerConnection conn = new CometdAbacusServerConnection(session);
				data.myConn = conn;
				mySessions.put(id, data);
				session.addListener(new ServerSession.RemoveListener() {
					public void removed(ServerSession session, boolean timeout) {
						synchronized(mySessions) {
							try {
								conn.disconnect();
							}
							catch (IOException e) {
							}
							mySessions.remove(id);
						}
					}
				});
			}
		}

		data.myConn.checkConnected(abacusHost, abacusPort, mySocketFactory);
		return data;
	}

	/**
	 * Attempts to retrieve the persistent HttpSession object for the connection.
	 *
	 * @param   create  Whether to attempt creation if the session does not exist.
	 * @return  The current session, or null if it does not exist and could not
	 * be created.
	 *
	 * @note Even if @a create is true, this method can still return null if
	 * it failed to new associate data with the current session.
	 */
	private HttpSession getHttpSession(boolean create) {
		HttpSession httpSession = null;
		Object rawTransport = getBayeux().getCurrentTransport();
		if (rawTransport != null && rawTransport instanceof HttpTransport) {
			HttpTransport transport = (HttpTransport) rawTransport;
			HttpServletRequest req = transport.getCurrentRequest();
			if (req != null) {
				httpSession = req.getSession(create);
			}
		}
		return httpSession;
	}

	/**
	 * Retrieves the object holding persistent session data.
	 *
	 * If this object doesn't already exist, one is created. If there a
	 * problem finding the HttpSession, a new object is returned anyway, but it
	 * will not persist across requests.
	 */
	private HttpSessionData getHttpSessionData() {
		HttpSessionData httpData = null;
		HttpSession httpSession = getHttpSession(true);
		if (httpSession != null) {
			httpData = (HttpSessionData) httpSession.getAttribute(HTTP_SESSION_ATTRIBUTE);
		}
		if (httpData == null) {
			httpData = new HttpSessionData();
			if (httpSession != null)
				httpSession.setAttribute(HTTP_SESSION_ATTRIBUTE, httpData);
		}
		return httpData;
	}

	static void logInfo(ServerSession client, String message) {
		String id = (client == null) ? "<null>" : client.getId();
		Log.info(id + ": " + message);
	}

	static void logWarn(ServerSession client, String message, Throwable e) {
		String id = (client == null) ? "<null>" : client.getId();
		Log.warn(id + ": " + message, e);
	}

	public Object service(ServerSession client, Map<String, Object> data) throws IOException {
		String name = (String) data.get("name");
		@SuppressWarnings("unchecked")
			Map<String, String> headers = (Map<String, String>) data.get("headers");
		String content = (String) data.get("content");
		logInfo(client, "Received command: " + name + ((content != null) ? " with content" : ""));

		AbacusServerConnection.MessageBlock msg = new AbacusServerConnection.MessageBlock(name);
		for (Map.Entry<String, String> entry : headers.entrySet()) {
			String key   = entry.getKey();
			String value = entry.getValue();
			msg.setHeader(key, value);

			// If the value is a password, then we should not log it.
			if (key.equals("pass") || key.equals("newpass"))
				value = "<hidden>";

			logInfo(client, "  " + key + " => " + value);
		}
		if (content != null) {
			msg.setContent(content.getBytes());
		}

		SessionData sessionData = null;
		try {
			// Special handling for logins, so that we can use cached
			// credentials
			if (name.equals("auth")) {
				String user = headers.get("user");
				String pass = headers.get("pass");
				HttpSessionData httpData = getHttpSessionData();

				if (user == null || pass == null) {
					// Request for auto-login using cached credentials
					user = httpData.myUsername;
					pass = httpData.myPassword;
					if (user == null || pass == null) {
						// No cached credentials available, so give up
						// but do not display an error in the client
						sendError(client, "");
						return null;
					}
					msg.setHeader("user", user);
					msg.setHeader("pass", pass);
				}

				/* Only create a session once we've passed the above checks,
				 * so that simply showing the login page doesn't cause a
				 * connection to abacus to be established.
				 */
				sessionData = getSessionData(client);
				sessionData.myUsername = user;
				sessionData.myPassword = pass;
				httpData.myUsername = user;
				httpData.myPassword = pass;
			}

			if (sessionData == null) {
				sessionData = getSessionData(client);
			}

			// Special handling for submissions, as we cannot get at the file
			// contents from the Javascript client. We do this only if the
			// content object is null, since this allows for flexibility in
			// devloping a mechanism where the client is actually able to send
			// the content in the servlet request.
			if (name.equals("submit") && content == null) {
				String fileKey = (String) data.get("file_key");
				byte[] fileContent = null;
				try {
					logInfo(client, "Querying for file data with key " + fileKey);
					fileContent = AbacusUploader.getUploadedFile(sessionData.myUsername, fileKey);
				}
				catch (IOException e) {
					sendError(client, "Error obtaining submission file data for " +
							  "key " + fileKey + ": " + e);
					return null;
				}

				if (fileContent == null) {
					sendError(client, "Unable to find submission file data for " +
							  "key " + fileKey);
					return null;
				}

				msg.setContent(fileContent);
			}

			try {
				if (name.equals("logout")) {
					sessionData.myConn.disconnect();
					sessionData.myUsername = null;
					sessionData.myPassword = null;
					HttpSession session = getHttpSession(false);
					if (session != null) {
						session.removeAttribute(HTTP_SESSION_ATTRIBUTE);
					}
					sendOk(client);
				}
				else {
					sessionData.myConn.getOutput().sendMessageBlock(msg);
				}
			}
			catch (IOException e) {
				logWarn(client, "Failed to send message to Abacus server.", e);
				sendError(client, "Error sending message to Abacus server.");
			}
		}
		catch (IOException e) {
			logWarn(client, "Failed to connect to Abacus server.", e);
			sendError(client, "Error obtaining connection to Abacus server.");
		}

		return null;
	}

	/**
	 * Sends the given error message to the given client.
	 */
	public void sendError(ServerSession client, String error) {
		Map<String, String> headers = new HashMap<String, String>();
		headers.put("msg", error);

		Map<String, Object> msg = new HashMap<String, Object>();
		msg.put("name", "err");
		msg.put("headers", headers);

		client.deliver(client.getLocalSession(), "/service/abacus", msg, null);
	}

	/**
	 * Sends an "ok" to the given client.
	 */
	public void sendOk(ServerSession client) {
		Map<String, String> headers = new HashMap<String, String>();
		Map<String, Object> msg = new HashMap<String, Object>();
		msg.put("name", "ok");
		msg.put("headers", headers);

		client.deliver(client.getLocalSession(), "/service/abacus", msg, null);
	}

	public void destroy() {
	}
}

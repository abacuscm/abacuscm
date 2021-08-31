/*  Copyright (C) 2010-2011, 2013  Bruce Merry and Carl Hultquist
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

import java.util.*;
import java.io.*;
import java.net.*;
import java.security.*;
import javax.net.ssl.*;
import java.nio.charset.*;
import java.nio.*;

/**
 * A connection with the Abacus server. This handles the basic communications
 * and makes calls to abstract methods to handle various incoming events;
 * extend this class and implement the missing methods to provide a fully
 * fledged connection.
 */
public abstract class AbacusServerConnection {
	/**
	 * Encapsulates the output end of the server connection. It's a separate
	 * class partially to allow it to be synchronized while not blocking
	 * concurrent input.
	 */
	public class Output {
		private PrintWriter myPrintWriter;

		public Output() throws IOException {
			OutputStream os = mySocket.getOutputStream();
			OutputStreamWriter osw = new OutputStreamWriter(os, Charset.forName("UTF-8"));
			myPrintWriter = new PrintWriter(new BufferedWriter(osw));
		}

		synchronized void sendMessageBlock(MessageBlock mb) throws IOException {
			mb.sendHeader(myPrintWriter);
			mb.sendContent(mySocket.getOutputStream());
		}
	}

	/**
	 * Encapsulates the input end of the server connection. This is run in its
	 * own thread so as to effectively wait for the socket to have data, rather
	 * than periodically polling it.
	 */
	public class Input implements Runnable {
		private BufferedInputStream myInput;

		/**
		 * UTF-8 character set.
		 */
		private Charset myUTF8;

		/**
		 * Whether or not we have been requested to terminate.
		 */
		private boolean myIsTerminating = false;

		public Input() throws IOException {
			InputStream is = mySocket.getInputStream();
			myInput = new BufferedInputStream(is);
			myUTF8 = Charset.forName("UTF-8");
		}

		/**
		 * Indicate that we wish to terminate this input handler.
		 */
		public void setIsTerminating() {
			myIsTerminating = true;
		}

		/**
		 * Reads bytes from the input stream until a particular byte is seen.
		 *
		 * The resulting bytes, excluding the terminator, are returned.
		 * If the end-of-stream is reached before the terminator is seen,
		 * returns null.
		 */
		private String readUntil(int terminator) throws IOException {
			ByteArrayOutputStream os = new ByteArrayOutputStream();
			while (true) {
				int next = myInput.read();
				if (next == -1) {
					return null;
				}
				else if (next == terminator) {
					break;
				}
				else {
					os.write(next);
				}
			}
			ByteBuffer buffer = ByteBuffer.wrap(os.toByteArray());
			return myUTF8.decode(buffer).toString();
		}

		/**
		 * Reads one message from the input and returns it. This function will
		 * block if data is not available.
		 *
		 * @throw IOException if there was a transport error
		 * @throw IOException if the stream ends in the middle of the header
		 * @throw CharacterCodingException if a header was malformed
		 * @throw NumberFormatException if the content-length header was malformed
		 * @todo Should we have custom exceptions instead?
		 */
		private synchronized MessageBlock receiveMessageBlock() throws IOException {
			/* The stream mixes characters and raw bytes (for content blobs),
			 * and the documentation for InputStreamReader explicitly says that
			 * it can do read-ahead. So we have to manually extract headers
			 * ourselves.
			 */
			final int charNewline = 10; /* UTF-8 encoding of newline */
			String action = readUntil(charNewline);

			if (action == null) {
				throw new IOException("Unexpected end of stream while reading action");
			}

			MessageBlock mb = new MessageBlock(action);

			String keyValue = readUntil(charNewline);
			if (keyValue == null) {
				throw new IOException("Unexpected end of stream while reading header");
			}

			int contentLength = 0;
			while (!keyValue.isEmpty()) {
				int index = keyValue.indexOf(':');
				if (index == -1)
					throw new IOException("Malformed header line (no colon)");
				String key = keyValue.substring(0, index);
				String value = keyValue.substring(index + 1);

				if (key.equals("content-length")) {
					int _contentLength = Integer.valueOf(value);
					if (_contentLength <= 0) {
						log("Received message with action '" +
								mb.getName() + "' and invalid " +
								"content length of " + _contentLength);
					}
					else {
						contentLength = _contentLength;
					}
				}
				else {
					mb.setHeader(key, value);
				}

				keyValue = readUntil(charNewline);
				if (keyValue == null) {
					throw new IOException("Unexpected end of stream while reading header");
				}
			}

			if (contentLength > 0) {
				byte[] content = new byte[contentLength];
				/* BufferedReader rather unhelpfully checks whether more reads would block,
				 * and if so returns rather than continuing.
				 */
				int done = 0;
				while (done < contentLength)
				{
					int next = myInput.read(content, done, contentLength - done);
					if (next <= 0)
						throw new IOException("Unexpected end of stream while reading content");
					done += next;
				}
				mb.setContent(content);
			}

			return mb;
		}

		public void run() {
			Exception error = null;
			try {
				while (!myIsTerminating) {
					MessageBlock mb = receiveMessageBlock();
					handleIncomingMessage(mb);
				}
			}
			catch (IOException e) {
				error = e;
			}
			catch (NumberFormatException e) {
				error = e;
			}

			if (error != null) {
				// Only log the error and notify the client if the error was
				// unexpected.
				if (!myIsTerminating) {
					log("Exception generated whilst processing input; " +
						"terminating input event handler.", error);

					connectionReset();
				}

				// Make an effort to cleanly disconnect the socket.
				try {
					disconnect();
				}
				catch (IOException e) {}

			}
		}
	}

	public static class MessageBlock {
		private String myName;
		private Map<String, String> myHeaders;
		private byte[] myContent;

		public MessageBlock(String name) {
			myName = name;
			myHeaders = new HashMap<String, String>();
			myContent = null;
		}

		public void setName(String name) {
			myName = name;
		}

		public String getName() {
			return myName;
		}

		public void setContent(byte[] content) {
			myContent = content.clone();
		}

		public void setHeader(String key, String value) {
			if (key.equals("content-length")) {
				throw new IllegalArgumentException("content-length is a reserved header");
			}
			myHeaders.put(key, value);
		}

		public String getHeader(String key) {
			return myHeaders.get(key);
		}

		public Map<String, String> getHeaders() {
			return Collections.unmodifiableMap(myHeaders);
		}

		public byte[] getContent() {
			return myContent;
		}

		/**
		 * Sends the header part of a message. Should only be called by
		 * Output.sendMessageBlock.
		 */
		void sendHeader(PrintWriter out) throws IOException {
			out.print(myName + "\n");
			for (Map.Entry<String, String> i : myHeaders.entrySet()) {
				out.print(i.getKey() + ":" + i.getValue() + "\n");
			}
			if (myContent != null) {
				out.print("content-length:" + myContent.length + "\n");
			}
			out.print("\n");
			out.flush();
		}

		/**
		 * Sends the binary message content, if any. Should only be called
		 * by Output.sendMessageBlock.
		 */
		void sendContent(OutputStream out) throws IOException {
			if (myContent != null) {
				out.write(myContent);
				out.flush();
			}
		}
	}

	private static SSLSocketFactory ourSSLSocketFactory;

	static {
		Security.addProvider(new com.sun.net.ssl.internal.ssl.Provider());

		try {
			ourSSLSocketFactory = (SSLSocketFactory) SSLSocketFactory.getDefault();
		}
		catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * The output side of the connection.
	 */
	private Output myOutput;

	/**
	 * The input side of the connection.
	 */
	private Input myInput;

	/**
	 * The socket used by this connection;
	 */
	private SSLSocket mySocket;

	/**
	 * Establishes a connection to the given hostname on the given port.
	 *
	 * @throws IOException       if an error occurred during the connection process
	 */
	/**
	 * Establishes a connection using the default socket factory.
	 *
	 * @param hostname The host to connect to.
	 * @param port     The port to connect to.
	 *
	 * @throws IOException       if an error occurred during the connection process
	 */
	public void connect(String hostname, int port) throws IOException {
		connect(hostname, port, ourSSLSocketFactory);
	}

	/**
	 * Establishes a connection using a user-specified socket factory.
	 *
	 * @param hostname       The host to connect to.
	 * @param port           The port to connect to.
	 * @param socketFactory  The socket factory to use.
	 *
	 * @throws IOException   if an error occurred during the connection process
	 */

	public synchronized void connect(String hostname, int port, SSLSocketFactory socketFactory)
		throws IOException {
		try {
			mySocket = (SSLSocket)socketFactory.createSocket(hostname, port);

			// The abacus server expects us to connect with at least TLSv1.2 SSL
			// support; any other enabled protocols will cause problems!
			mySocket.setEnabledProtocols(new String[] {"TLSv1.2"});

			// Start the SSL handshake.
			mySocket.startHandshake();

			myOutput = new Output();
			myInput = new Input();

			// Run the input processor in its own thread.
			new Thread(myInput).start();
		}
		catch (IOException e) {
			// Something got screwed up whilst connecting. Make sure to close
			// off the socket and set is to null, so that we don't leave things
			// in a half-baked state.
			disconnect();

			// Now re-throw the exception so that callers may catch it.
			throw e;
		}
	}

	public Output getOutput() {
		return myOutput;
	}

	/**
	 * Disconnects this connection (if we are connected).
	 */
	public synchronized void disconnect()
		throws IOException {
		if (myInput != null)
			myInput.setIsTerminating();

		myOutput = null;
		myInput = null;

		Socket oldSocket = mySocket;
		mySocket = null;
		if (oldSocket != null) {
			oldSocket.close();
		}
	}

	/**
	 * Checks that we are connected, and if not connects to the given hostname
	 * on the given port.
	 *
	 * <p>FIXME: should we just fold this into connect() instead?
	 */
	public synchronized void checkConnected(String hostname, int port)
		throws IOException {
		checkConnected(hostname, port, ourSSLSocketFactory);
	}

	/**
	 * Checks that we are connected, and if not connects to the given hostname
	 * on the given port. The socket is created using the given socket factory.
	 */
	public synchronized void checkConnected(String hostname, int port,
											SSLSocketFactory socketFactory)
		throws IOException {
		if (mySocket != null)
			return;

		connect(hostname, port, socketFactory);
	}

	// -------------------------------------------------------------------------
	// Methods which we expect subclasses to implement, and which dictate how
	// various incoming events are dealt with and dispatched to clients.
	// -------------------------------------------------------------------------

	/**
	 * Process a message received from the abacus server.
	 */
	protected abstract void handleIncomingMessage(final MessageBlock mb);

	/**
	 * Called when the connection has been reset. This will probably require the
	 * client to re-authenticate.
	 */
	protected abstract void connectionReset();

	/**
	 * Log a message with a stack trace from a throwable.
	 */
	protected abstract void log(String message, Throwable throwable);

	/**
	 * Log the given message to the client and server logs.
	 */
	protected abstract void log(String message);
}

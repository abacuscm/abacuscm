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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;

import java.math.BigInteger;

import java.security.SecureRandom;

import java.util.Enumeration;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.commons.fileupload.FileItem;
import org.apache.commons.fileupload.FileUploadException;
import org.apache.commons.fileupload.disk.DiskFileItemFactory;
import org.apache.commons.fileupload.servlet.ServletFileUpload;
import org.apache.commons.io.IOUtils;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Handles file uploads from the web interface, and provides a mechanism for
 * the Abacus service to obtain these. The keys used to index the files are
 * generated using a SecureRandom instance, and for additional protection the
 * servlet insists on each upload being associated with a specific username.
 * The Abacus service must present the username matching the upload key in
 * order to retrieve the uploaded data.
 */
public class AbacusUploader extends HttpServlet {

	private static final long serialVersionUID = 6748857432950840322L;
	private static final Logger logger = LoggerFactory.getLogger(AbacusUploader.class);

	/**
	 * The factory that we use for creating FileItem instances from multipart
	 * uploads. This is a little yuck, but Jetty 6 and 7 do not use the servlet
	 * 3.0 interface which means we need to use Apache Commons in order to
	 * parse such requests in a non-hacky way. This can all go away if we can
	 * work out how to get servlet-3.0 in, since then we have access to the
	 * multipart data straight from the ServletRequest.
	 *
	 * Note that the DiskFileItemFactory is poorly named, in that it only
	 * stores items on disk if they exceed a certain size. We configure it
	 * with a size of 2Mb, which should be more than anything we ever receive
	 * (as the file uploader widget in the client prevents uploads larger than
	 * 1Mb).
	 */
	private static final DiskFileItemFactory ourFileItemFactory;
	static {
		ourFileItemFactory = new DiskFileItemFactory();
		ourFileItemFactory.setSizeThreshold(2097152);
	}

	/**
	 * A helper class for storing all the information that we need related to an
	 * uploaded file.
	 */
	private static class UploadData {
		public String filename;
		public String userId;
		public byte[] fileData;
		public long   timeMs;

		public UploadData(String filename, String userId, byte[] fileData) {
			this.filename = filename;
			this.userId = userId;
			this.fileData = fileData;

			timeMs = System.currentTimeMillis();
		}
	}

	/**
	 * Our map of uploaded data.
	 */
	private static ConcurrentHashMap<String, UploadData> ourUploadedFiles =
		new ConcurrentHashMap<String, UploadData>();

	/**
	 * The number of milliseconds that we sleep for betweeb successive checks
	 * of which items of file data can be removed. We set this to 60s for now.
	 */
	private static final long REMOVAL_PERIOD = 60 * 1000;

	/**
	 * The time in milliseconds after which an upload is considered to have
	 * expired, making it eligible for automatic removal. We set this to 10
	 * minutes for now.
	 */
	private static final long EXPIRY_TIME = 10 * 60 * 1000;

	static {
		// Kick off a thread to periodically remove old file data.
		new Thread() {
			@Override
			public void run() {
				while (true) {
					try {
						sleep(REMOVAL_PERIOD);
					}
					catch (InterruptedException e) {
						// We don't expect to be interrupted (and don't particularly care
						// if we are.
					}

					long expiryMs = System.currentTimeMillis() - EXPIRY_TIME;
					for (Map.Entry<String, UploadData> entry : ourUploadedFiles.entrySet())
						if (expiryMs > entry.getValue().timeMs) {
							// This one is old enough to be removed.
							String key = entry.getKey();
							logger.info("Automatically removing file data for key {} which is too old", key);
							ourUploadedFiles.remove(key);
						}
				}
			}
		}.start();
	}

	/**
	 * Our secure random instance, for generating keys to assign to uploads.
	 */
	private SecureRandom myRandom = new SecureRandom();

	/**
	 {@inheritDoc}
	 */
	@Override
	protected void doPost(HttpServletRequest request, HttpServletResponse response)
		throws ServletException {
		if (request.getParameter("remove") != null) {
			// This is a request to remove an upload.
			removeUpload(request, response);
		}
		else {
			// Otherwise, we assume that it is a request to upload a new file.
			addUpload(request, response);
		}
	}

	/**
	 * Add a new upload using the data in the given request.
	 */
	private void addUpload(HttpServletRequest request, HttpServletResponse response) {
		PrintWriter writer = null;
		InputStream is = null;
		ByteArrayOutputStream os = null;

		try {
			writer = response.getWriter();
		} catch (IOException ex) {
			logger.info("Unexpected exception: {}", ex.getMessage());
		}

		String filename = request.getHeader("X-File-Name");
		String user = request.getParameter("user");
		try {
			if (user == null) {
				// Insist on the user id being present in the request.
				response.setStatus(response.SC_BAD_REQUEST);
				writer.print("{success: false}");
				logger.info("no user id in request");
				return;
			}

			// If this is a multipart/form-data request, then the data we want
			// will be inside one of the parts. This is necessary for handling
			// uploads from Internet Explorer.
			if (ServletFileUpload.isMultipartContent(request)) {
				List<FileItem> parts = null;
				try {
					parts = new ServletFileUpload(ourFileItemFactory).parseRequest(request);
				}
				catch (FileUploadException e) {
					logger.info("Failed to parse multipart request", e);
				}

				// XXX which part should we get the data from? Need to check
				// this. For now, I assume that there will actually only be
				// one part (since this code is to support IE's weirdness), and
				// just log information about all the parts seen.
				if (parts != null) {
					for (FileItem part : parts) {
						logger.info("Multipart content, type '" +
								 part.getContentType() + "', name '" +
								 part.getFieldName() + "', filename '" +
								 part.getName() + ", size " + part.getSize());

						// We expect the content to have the name "qqfile".
						if (part.getFieldName().equals("qqfile")) {
							if (is != null) {
								logger.info("Multiple matching " +
										 "parts found in multipart content; " +
										 "only considering the first one found");
							}
							else {
								is = part.getInputStream();
							}
						}
					}
				}
			}

			// If no InputStream has been assigned yet, then we defer to the
			// full request contents. This happens either if the request was
			// not multipart, or if none of the parts were what we would expect
			// to read our file data from.
			if (is == null) {
				is = request.getInputStream();
				logger.info("Using default input stream");
			}

			os = new ByteArrayOutputStream();
			IOUtils.copy(is, os);
			UploadData data = new UploadData(filename, user, os.toByteArray());
			String key;
			do {
				key = new BigInteger(130, myRandom).toString(32);
			} while (ourUploadedFiles.putIfAbsent(key, data) != null);
			response.setStatus(response.SC_OK);
			writer.print("{success: true, key: '" + key + "'}");
			logger.info("Accepted file data of length {} bytes with key {} for user {}", data.fileData.length, key, user);
		} catch (IOException ex) {
			response.setStatus(response.SC_INTERNAL_SERVER_ERROR);
			writer.print("{success: false}");
			logger.info("Unexpected exception: {}", ex.getMessage());
		} finally {
			try {
				os.close();
				is.close();
			} catch (IOException ignored) {
			}
		}

		writer.flush();
		writer.close();
	}

	/**
	 * Remove an upload using the data in the given request.
	 */
	private void removeUpload(HttpServletRequest request, HttpServletResponse response) {
		String user = request.getParameter("user");
		String key  = request.getParameter("key");

		if (key == null) {
			// Insist on the key being present in the request.
			response.setStatus(response.SC_BAD_REQUEST);
			logger.info("no key in request");
			return;
		}

		if (user == null) {
			// Insist on the user id being present in the request.
			response.setStatus(response.SC_BAD_REQUEST);
			logger.info("no user id in request");
			return;
		}

		UploadData data = ourUploadedFiles.get(key);
		if (data != null) {
			if (data.userId.equals(user)) {
				logger.info("Removing file data for key {} at request of user", key);
				ourUploadedFiles.remove(key);
			}
			else {
				logger.info("Ignoring request to remove file data for key {} from user {}, as the file is owned by {}", key, user, data.userId);
			}
		}
		else {
			logger.info("Ignoring request to remove file data for key " + key + " from user " + user + " which is not in our cache");
		}
	}

	/**
	 * Query interface for the Abacus servlet, which passes in the key
	 * generated during the upload process (and which is passed to the servlet
	 * by the Javascript client to identify the upload).
	 */
	public static byte[] getUploadedFile(String user, String key) throws IOException {
		UploadData data = ourUploadedFiles.get(key);
		if (data != null) {
			if (data.userId.equals(user)) {
				logger.info("Returning file data for key {} of {} bytes", key, data.fileData.length);
				return data.fileData;
			}
			else {
				logger.info("Not returning file data for key {}, which is for user {} and not user {}", key, data.userId, user);
				return null;
			}
		}
		else {
			return null;
		}
	}
}

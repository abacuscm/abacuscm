/*  Copyright (C) 2010-2011, 2015  Bruce Merry and Carl Hultquist
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

// ---------------------------------------------------------------------
// Debugging functionality
// ---------------------------------------------------------------------

(function($) {
	/**
	 * Show the debug controls.
	 */
	this.showDebug = function() {
		showTab('tab-debug');
		showTab('tab-log');
	}

	/**
	 * Hide the debug controls.
	 */
	this.hideDebug = function() {
		hideTab('tab-debug');
		hideTab('tab-log');
	}

	// Output an array of strings to the log
	var logLines = function(lines) {
		var logNode = $('#log');
		var ts = new Date() + ': ';
		var text = ts + '----------------------------\n';
		for (var i = 0; i < lines.length; i++) {
			text += ts + lines[i] + '\n';
		}
		logNode.html(logNode.html() + escapeHTML(text));
	}

	// Add a single line of text to the log
	this.logLine = function(line) {
		logLines([line]);
	}

	// Logs an abacus message. Here, message is just the payload containing
	// the abacus protocol, not any of the cometd wrapping.
	var logMessage = function(caption, message) {
		var lines = new Array();
		lines.push(caption);
		lines.push(message.name);
		for (var key in message.headers) {
			lines.push('\t' + key + ':' + message.headers[key]);
		}
		if ($.isArray(message.content)) {
			lines.push('content-length:' + message.content.length);
		}
		logLines(lines);
	}

	// Log a received message to the logging window (message includes
	// the cometd wrapping).
	this.logReceivedHandler = function(message) {
		logMessage('Received', message.data);
	}

	// Log an outgoing message (message is just the payload)
	this.logSentMessage = function(message) {
		logMessage('Sent', message);
	}

	// Log an exception being generated.
	this.logException = function(message) {
		logLine(message);
		window.jAlert(message, 'Unexpected exception');
	}

	// Handles a message sent to /service/log
	this.logChannelHandler = function(message) {
		logLine(message.data);
	}

	// Uses messages typed into the standings search box for debugging
	this.debugHook = function(command) {
		if (command == 'debug show') {
			showDebug();
		} else if (command == 'debug hide') {
			hideDebug();
		}
	}

	$(document).ready(function() {
		$('#out-msg-submit').click(function(event) {
			var msg, i;
			msg = {}
			msg.name = $('#out-msg-name')[0].value;
			msg.headers = {};
			i = 1;
			while (i < 20) {
				var key = $('#out-msg-key' + i);
				if (key.length == 0) {
					break;
				}
				var value = $('#out-msg-value' + i);
				if (value.length == 0) {
					break;
				}
				key = key[0].value;
				value = value[0].value;
				if (key.length > 0 && value.length > 0) {
					msg.headers[key] = value;
				}
				i++;
			}
			sendMessageBlock(msg, defaultReplyHandler);
		});
	});
})(jQuery);

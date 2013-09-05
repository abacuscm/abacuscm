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

(function($) {
	window.jAlert = function(msg, title, callback) {
		if (title == null)
			title = "Alert";
		if (callback == null)
			callback = function (result) { return false; };

		$("#alert-dialog-text").text(msg);
		$("#alert-dialog").dialog({
				title: title,
				buttons: {
					"Ok": function (result) { $(this).dialog('close'); callback(result); }
				}
			}
		).dialog('open');
	}

	var hmsToString = function(hours, minutes, seconds) {
		return padNumber(hours) + ':' + padNumber(minutes) + ':' + padNumber(seconds);
	}

	this.dateToString = function(date) {
		var seconds = date.getSeconds();
		var minutes = date.getMinutes();
		var hours = date.getHours();
		return hmsToString(hours, minutes, seconds);
	}

	this.epochTimeToString = function(time) {
		var date = new Date();
		date.setTime(time * 1000);
		return dateToString(date);
	}

	this.timeToString = function(time) {
		var seconds = time % 60;
		var minutes = (time - seconds) / 60 % 60;
		var hours = (time - 60 * minutes - seconds) / 3600;
		return padNumber(hours) + ':' + padNumber(minutes) + ':' + padNumber(seconds);
	}

	this.parseUtf8 = function(byteArray){
		var result = new Array;
		for(var a, b, i = -1, l = byteArray.length, o = String.fromCharCode; ++i < l;) {
			var k = byteArray[i];
			a = k;
			if (a & 0x80) {
				if ((a & 0xfc) == 0xc0) {
					b = byteArray[i + 1];
					if ((b & 0xc0) == 0x80) {
						var ch = o(((a & 0x03) << 6) + (b & 0x3f));
						result.push(ch);
					}
					else {
						var ch = o(128);
						result.push(ch);
					}
				}
				else {
					var ch = o(128);
					result.push(ch);
				}
				++i;
			}
			else {
				var ch = o(k);
				result.push(ch);
			}
		}
		return result.join('');
	}

	this.escapeHTML = function (str) {
		return str.split('&').join('&amp;').split('<').join('&lt;').split('>').join('&gt;');
	}

	this.padNumber = function (num) {
		var s = '' + num;
		while (s.length < 2) s = '0' + s;
		return s;
	}

	var cometd = $.cometd;

	// ---------------------------------------------------------------------
	// Abacus communications
	// ---------------------------------------------------------------------

	var pendingMessages = new Array();

	this.nullReplyHandler = function(msg) {
	}

	this.defaultReplyHandler = function(msg) {
		if (msg.data.name == 'err') {
			window.jAlert(msg.data.headers.msg);
		}
	}

	var replyHandler = nullReplyHandler;
	var waiting = false;

	this.queueHandler = function(data, handler) {
		var queuedMessage = {};
		queuedMessage.handler = handler;
		queuedMessage.handlerOnly = true;
		queuedMessage.data = data;

		queueAndProcessMessage(queuedMessage);
	}

	this.sendMessageBlock = function(msg, handler) {
		var queuedMessage = {};
		queuedMessage.msg = msg;
		queuedMessage.handler = handler;
		queuedMessage.handlerOnly = false;

		queueAndProcessMessage(queuedMessage);
	}

	this.queueAndProcessMessage = function(queuedMessage) {
		pendingMessages.push(queuedMessage);

		processMessageFromQueue();
	}

	/**
	 * Resets the message queue, clearing any messages that are in the message queue,
	 * and clearing the reply handler and waiting state.
	 */
	this.resetMessageQueue = function() {
		pendingMessages = new Array();
		replyHandler = nullReplyHandler;
		waiting = false;
		hideWaiting();
	}

	// This method explicitly kept private, since it should not be called
	// directly by functions from other scripts.
	var processMessageFromQueue = function() {
		if (waiting)
			return;

		if (pendingMessages.length > 0) {
			var queuedMessage = pendingMessages.shift();
			showWaiting(pendingMessages.length + 1);
			waiting = true;
			replyHandler = queuedMessage.handler;

			// If this message is only used to call a handler, then do this
			// directly.
			if (queuedMessage.handlerOnly) {
				reply({data: {name: 'ok', data: queuedMessage.data}});
			} else {
				logSentMessage(queuedMessage.msg);
				cometd.publish('/service/abacus', queuedMessage.msg);
			}
		} else {
			hideWaiting();
			waiting = false;
		}
	}

	// This method explicitly kept private, since it should not be called
	// directly by functions from other scripts.
	var reply = function(msg) {
		try {
			// Determine what to do based on the name of the message received.
			switch (msg.data.name) {
			case 'ok':
			case 'err':
				if (waiting) {
					// Explicitly handle exceptions generated in response to our
					// queries, so that we continue to process other items in
					// the message queue.
					try {
						replyHandler(msg);
					}
					catch (e) {
						logException('Unexpected exception "' + e + '" generated whilst ' +
									 'processing message "' + msg.data.name + '"');
					}

					waiting = false;
					processMessageFromQueue();
				}
				break;

			case 'updateclarificationrequests':
				// A new clarification request
				updateClarificationRequests(msg);

				// Contestant clarification requests are always generated
				// by themselves (general clarifications will come in on
				// the "updateclarifications" message). As such, this message
				// will never contain "new" information for the contestant,
				// and so we do not highlight the "Clarification requests"
				// tab. We will need to revisit this when extending the web
				// interface to allow for judging.
				break;

			case 'updateclarifications':
				// A new clarification
				updateClarifications(msg);
				highlightTab('clarifications');
				break;

			case 'updatesubmissions':
				// A new or updated submission
				updateSubmissions(msg);
				highlightTab('submissions');
				break;

			case 'updatestandings':
				// New information for the standings tab
				updateStandings(msg);
				highlightTab('standings');
				break;

			case 'startstop':
				// The contest has been started or stopped
				startStop(msg);
				break;

			case 'connectionreset':
				// The connection with the Abacus server was reset, which requires
				// us to reauthenticate.
				connectionReset();
				break;

			default:
				// Do nothing for now... should we perhaps add to the log?
				break;
			}
		}
		catch (e) {
			window.jAlert('Unexpected exception "' + e + '" generated whilst ' +
						  'processing message "' + msg.data.name + '"');
		}
	}

	// Function invoked when first contacting the server and when the server
	// has lost the state of this client. Note that this method explicitly kept
	// private, since it should not be called directly by functions from other
	// scripts.
	var metaHandshake = function(handshake) {
		if (handshake.successful === true) {
			connectionReset();
		}
	}

	var showWaiting = function(num) {
		var message = '<span>Working; ' + num + ' message' + (num > 1 ? 's' : '') + ' remaining... please wait</span>';
		$('#waiting').html(message);
		$('#waiting').show();
	}

	var hideWaiting = function() {
		$('#waiting').hide();
	}

	var setStatus = function(stat) {
		$('#status').html('<p>' + stat + '</p>')
	}

	var connected = false;

	var setConnected = function (newConnected) {
		if (connected != newConnected) {
			connected = newConnected;
			if (connected) {
				$('#status-connected').show();
				$('#status-disconnected').hide();
			}
			else {
				$('#status-connected').hide();
				$('#status-disconnected').show();
			}
		}
	}

	// Function that manages the connection status with the Bayeux server
	var metaConnect = function(message) {
		if (cometd.isDisconnected())
			setConnected(false);
		else
			setConnected(message.successful === true);
	}

	var metaDisconnect = function(message) {
		setConnected(false);
	}

	/**
	 * Highlights the given tab name to indicate that the tab has new data.
	 * Will not highlight the tab if it is currently selected.
	 */
	this.highlightTab = function(name) {
		var tab = $('#' + name + '-tab-label');
		if (!tab.parent().parent().hasClass('ui-tabs-selected')) {
			tab.addClass('tab-highlight');
		}
	}

	$(document).ready(function() {
		// Initialisation -- we need to set up the properties of some
		// reusable dialogs, and initially hide some things.

		// General alert dialog
		$('#alert-dialog').dialog({
			disabled: false,
			autoOpen: false,
			closeOnEscape: true,
			draggable: true,
			modal: true,
			position: 'center',
			title: 'Alert',
			width: 'auto',
			height: 'auto'
		});

		// Login dialog
		$('#login-dialog').dialog({
			disabled: false,
			autoOpen: false,
			closeOnEscape: false,
			draggable: true,
			modal: true,
			position: 'center',
			resizable: false,
			title: 'Login to Abacus',
			width: 'auto',
			height: 'auto',
			beforeClose: isLoginCloseAllowed,
			open: function(event, ui) { $(this).closest('.ui-dialog').find('.ui-dialog-titlebar-close').hide(); }
		});

		// Clarification request dialog
		$('#clarification-request-dialog').dialog({
			disabled: false,
			autoOpen: false,
			closeOnEscape: true,
			draggable: true,
			modal: true,
			position: 'center',
			resizable: false,
			title: 'Request clarification',
			width: 'auto',
			height: 'auto'
		});

		// Submission dialog
		$('#submission-dialog').dialog({
			disabled: false,
			autoOpen: false,
			closeOnEscape: true,
			draggable: true,
			modal: true,
			position: 'center',
			resizable: false,
			close: submissionDialogClose,
			title: 'Make submission',
			width: 'auto',
			height: 'auto'
		});

		// Submission result dialog
		$('#submission-result-dialog').dialog({
			disabled: false,
			autoOpen: false,
			closeOnEscape: true,
			draggable: true,
			modal: true,
			position: 'center',
			resizable: false,
			title: 'Submission result',
			width: 'auto',
			height: 'auto'
		});

		$('#waiting').hide();
		$('#tabs').hide();
		$('#logout').hide();
		$('#status-connected').hide();

		// Whenever we switch tabs, remove any highlight that the new selected
		// tab may have had.
		$('#tabs').bind('tabsselect', function(event, ui) {
			$(ui.tab.children[0]).removeClass('tab-highlight');
			return true;
		});

		// Initially, the user should not be logged in and so we show the
		// login dialog.
		showLoginDialog();

		// Disconnect when the page unloads
		$(window).unload(function() {
			cometd.disconnect(true);
		});

		$('#tabs').tabs();

		// Disable the WebSocket transport, which is experimental and possibly
		// buggy.
		cometd.unregisterTransport('websocket');

		var cometURL = location.protocol + '//' + location.host + config.contextPath + '/cometd';
		cometd.configure({
			url: cometURL,
			logLevel: 'debug'
		});

		cometd.addListener('/meta/handshake', metaHandshake);
		cometd.addListener('/meta/connect', metaConnect);
		cometd.addListener('/meta/disconnect', metaDisconnect);

		cometd.addListener('/service/abacus', reply);
		cometd.addListener('/service/abacus', logReceivedHandler);
		cometd.addListener('/service/log', logChannelHandler);

		cometd.handshake();
	});
})(jQuery);

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

// ---------------------------------------------------------------------
// Login/logout related methods
// ---------------------------------------------------------------------

(function($) {
	var username = '';

	this.getUsername = function() {
		return username;
	}

	this.showLoginDialog = function() {
		$('#login-dialog-error').html('&nbsp;');
		$('#login-dialog-username').val('');
		$('#login-dialog-password').val('');
		$('#login-dialog').dialog('open');
	}

	this.isLoginCloseAllowed = function() {
		return username != '';
	}

	var loginReplyHandler = function (msg) {
		if (msg.data.name == 'ok') {
			// Login was successful
			username = msg.data.headers.user;
			$('#login-dialog').dialog('close');
			$('#tabs').show();
			$('#logout').show();
			$('#user-status').show();
			$('#username').html(escapeHTML(getUsername()));
			$('#username').show();
			$('#contest-status').show();

			// Now that we have logged in, we need to update all our state.
			registerStartStop();
			getProblems();
			getLanguages();
			getContestStatus();
			getClarifications();
			getClarificationRequests();
			getSubmissions();
			getStandings();
		} else {
			var error = msg.data.headers.msg;
			if (error == '') {
				error = '&nbsp;';
			} else {
				error = escapeHTML(error);
			}
			$('#login-dialog-error').html(error);
		}
	}

	var logoutReplyHandler = function (msg) {
		if (msg.data.name == 'ok') {
			// Logout was successful
			// Reloading is simpler than trying to reset all the state.
			window.location.reload();
		} else {
			window.jAlert('Failed to logout: ' + msg.data.headers.msg);
		}
	}

	/**
	 * Attempts to automatically authenticate the user using credentials stored
	 * in the server's session data.
	 */
	var attemptAutomaticAuthentication = function() {
		var autoLoginMessage = {
			name: 'auth',
			headers: {}
		};

		// If our configuration configures us to automatically login as a
		// particular user, then do so instead of using the server-side
		// credentials.
		if (config.autoUser && config.autoPass) {
			autoLoginMessage.headers.user = config.autoUser;
			autoLoginMessage.headers.pass = config.autoPass;
		}

		if (!config.autoUser || config.autoUser != 'nologin') {
			sendMessageBlock(autoLoginMessage, loginReplyHandler);
		}
	}

	/**
	 * Called to indicate that the connection to the Abacus server was reset,
	 * which requires re-authentication.
	 */
	this.connectionReset = function() {
		resetMessageQueue();
		if (username != '') {
			// We only need to notify the user if currently logged in.
			username = '';
			window.jAlert('The connection with the Abacus server has been ' +
						  'reset. This page will reload once you close this ' +
						  'popup.', 'Connection reset',
						  function() {
							  // We treat this as if the user had logged out.
							  logoutReplyHandler({data: {name: 'ok'}});
						  });
		} else {
			// Attempt to automatically authenticate the user at page load.
			attemptAutomaticAuthentication();
		}
	}

	$(document).ready(function() {
		$('#username').hide();
		$('#user-status').hide();

		$('#login-dialog-submit').click(function(event) {
			var username = $('#login-dialog-username')[0].value;
			var password = $('#login-dialog-password')[0].value;
			var msg = {
				name: 'auth',
				headers: {
					user: username,
					pass: password
				}
			};

			sendMessageBlock(msg, loginReplyHandler);
		});

		$('#logout').click(function(event) {
			var msg = {
				name: 'logout',
				headers: {}
			};

			sendMessageBlock(msg, logoutReplyHandler);
		});
	});
})(jQuery);

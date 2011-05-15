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
// Contest status
// ---------------------------------------------------------------------

(function($) {
	var projectedStop;

	/**
	 * Register ourselves for start/stop notifications.
	 */
	this.registerStartStop = function() {
		sendMessageBlock({
				name: 'subscribetime',
				headers: {}
			},
			nullReplyHandler
		);
	}

	this.getContestStatus = function() {
		sendMessageBlock({
				name: 'contesttime',
				headers: {}
			},
			contestStatusHandler
		);
	}

	function contestStatusHandler(msg) {
		if (msg.data.name != 'ok') {
			// Ick. What do we do here? For now, spam a popup and return.
			window.jAlert('Error updating submissions: ' + msg.data.headers.msg);
			return;
		}

		var status;
		if (msg.data.headers.running == 'yes') {
			var nowMs = new Date().getTime();

			// We also need to adjust for the lag between us and the server. We
			// decrease nowMs by the measured lag to give us the time at which
			// the server would have sent its message.
			var lagMs = $.cometd.getExtension('timesync').getNetworkLag();
			nowMs -= lagMs;
			logLine('Lag from server is currently ' + lagMs + 'ms');

			var now = Math.floor(nowMs / 1000);
			var remain = msg.data.headers.remain;
			projectedStop = now + parseInt(remain, 10);
		} else {
			projectedStop = 0;
		}

		updateStatus();
	}

	this.startStop = function(msg) {
		if (msg.data.headers.action == 'start') {
			// Fire off a request for the latest contest status, so that we get
			// the time remaining.
			getContestStatus();

			window.jAlert('The contest has been started');
		} else {
			// The contest has stopped, so clear our projected stop time.
			projectedStop = 0;

			window.jAlert('The contest has been stopped');
		}

		// Update our status now for good measure (although I don't think we
		// really need to, as this should happen automagically in both of the
		// above code paths).
		updateStatus();
	}

	function updateStatus() {
		var status;
		if (projectedStop == 0) {
			status = 'Contest stopped';
		} else {
			var now = Math.floor(new Date().getTime() / 1000);
			var remain = projectedStop - now;
			status = 'Contest running: ' + timeToString(remain) + ' remaining';
			setTimeout(updateStatus, 1000);
		}

		$('#contest-status').html(status);
	}

	$(document).ready(function() {
		$('#contest-status').hide();
	});
})(jQuery);

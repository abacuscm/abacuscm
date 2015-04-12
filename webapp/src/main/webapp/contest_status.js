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
	var blinds;
	var lastRemain; // remain header from last contesttime query

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
			window.jAlert('Error updating status: ' + msg.data.headers.msg);
			return;
		}

		var status;
		lastRemain = parseInt(msg.data.headers.remain, 10);
		blinds = parseInt(msg.data.headers.blinds, 10);
		if (msg.data.headers.running == 'yes') {
			var nowMs = new Date().getTime();

			// We also need to adjust for the lag between us and the server. We
			// decrease nowMs by the measured lag to give us the time at which
			// the server would have sent its message.
			var lagMs = $.cometd.getExtension('timesync').getNetworkLag();
			nowMs -= lagMs;
			logLine('Lag from server is currently ' + lagMs + 'ms');

			var now = Math.floor(nowMs / 1000);
			projectedStop = now + lastRemain;
		} else {
			projectedStop = 0;
		}

		updateStatus();
	}

	this.startStop = function(msg) {
		if (msg.data.headers.action == 'start') {
			window.jAlert('The contest has been started');
		} else {
			// The contest has stopped, so clear our projected stop time.
			projectedStop = 0;
			window.jAlert('The contest has been stopped');
		}

		// Do a synchronous display update, to immediate change the status
		// display when stopped
		updateStatus();
		// Make sure we have a correct understanding of time remaining
		getContestStatus();
	}

	function updateStatus() {
		var status;
		var remain;
		if (projectedStop == 0) {
			status = 'Contest stopped';
			remain = lastRemain;
		} else {
			var now = Math.floor(new Date().getTime() / 1000);
			remain = projectedStop - now;
			if (remain < 0)
				remain = 0;
			status = 'Contest running: ' + timeToString(remain) + ' remaining';
			setTimeout(updateStatus, 1000);
		}
		if (hasPermission('see_final_standings') || remain >= blinds)
			$('#standings-frozen').removeClass('frozen');
		else
			$('#standings-frozen').addClass('frozen');

		$('#contest-status').html(status);
	}

	$(document).ready(function() {
		$('#contest-status').hide();
	});
})(jQuery);

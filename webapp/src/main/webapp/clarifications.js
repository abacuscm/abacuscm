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

// ---------------------------------------------------------------------
// Clarifications and clarification requests
// ---------------------------------------------------------------------

(function($) {
	var clarificationRequests = new Array();
	var clarifications = new Array();

	/**
	 * Encodes a clarification request string for transmission to Abacus,
	 * by replacing the newline characters with a special code.
	 */
	var encodeNewlines = function(s) {
		var result = new Array();
		for (var i = 0; i < s.length; i++)
			if (s[i] == '\n')
				result.push(String.fromCharCode(1));
			else
				result.push(s[i]);

		return result.join('');
	}

	/**
	 * Decodes a clarification or clarification request sent by Abacus, by
	 * replacing the special code used for newlines with an actual newline
	 * character. The isPre parameter specifies whether or not we are in a
	 * <pre> block, and uses either \n or <br> newlines accordingly.
	 */
	var decodeNewlines = function(s, isPre) {
		var result = new Array();
		var replacement = isPre ? '\n' : '<br />';
		var newline = String.fromCharCode(1);
		for (var i = 0; i < s.length; i++)
			if (s[i] == newline)
				result.push(replacement);
			else
				result.push(s[i]);
		return result.join('');
	}

	/**
	 * Returns a shortened form of the given string, suitable for display in a
	 * table. If the string is less than 40 characters and has no new lines,
	 * then we return it as-is; otherwise, we return the portion of the string
	 * up to either 40 characters or the first new line (whichever comes first).
	 * In the event of a shortened string being returned, we also append ... to
	 * the end to indicate that more content follows.
	 */
	var shortText = function(s) {
		var result = new Array();
		var maxLen = 40;
		var newline = String.fromCharCode(1);
		var continued = false;
		for (var i = 0; i < s.length; i++) {
			if (s[i] == newline || i == maxLen) {
				continued = true;
				break;
			}
			result.push(s[i]);
		}
		if (continued)
			result.push(' ...');
		return result.join('');
	}

	this.updateClarificationRequests = function(msg) {
		var request = {};
		request.id = msg.data.headers.id;
		request.time = msg.data.headers.time;
		request.user_id = msg.data.headers.user_id;
		request.problem = msg.data.headers.problem;
		request.question = msg.data.headers.question;

		clarificationRequests.push(request);
		updateClarificationRequestsTable();
	}

	this.getClarificationRequests = function() {
		sendMessageBlock({
				name: 'getclarificationrequests',
				headers: {}
			},
			getClarificationRequestsReplyHandler
		);
	}

	function updateClarificationRequestsTable() {
		// First make sure that the clarification requests are correctly
		// sorted, from newest to oldest.
		clarificationRequests.sort(function(a, b) {
			var diff = b.time - a.time;
			if (diff == 0)
				diff = b.id - a.id;
			return diff;
		});

		var html = '';
		for (var i = 0; i < clarificationRequests.length; i++) {
			var request = clarificationRequests[i];

			// To set the status, we need to go through the set of
			// clarifications.
			var answered = false;
			for (var j = 0; j < clarifications.length; j++) {
				var clarification = clarifications[j];
				if (clarification.req_id == request.id) {
					answered = true;
					break;
				}
			}

			var status = answered ?
				'<span class="clarification-request-answered">Answered</span>' :
				'<span class="clarification-request-unanswered">Unanswered</span>';

			html +='<tr class="clarification-request-row">' +
				'<td>' + request.id + '</td>' +
				'<td>' + escapeHTML(epochTimeToString(request.time)) + '</td>' +
				'<td>' + escapeHTML(request.problem) + '</td>' +
				'<td>' + status + '</td>' +
				'<td>' + decodeNewlines(escapeHTML(shortText(request.question)), false) + '</td>' +
				'</tr>';
		 }

		$('#clarification-requests-tbody').html(html);
		$('tr.clarification-request-row').click(function() {
			var requestId = $($(this).children()[0]).text();
			showClarificationRequestDetails(requestId, -1, 0);
		});
		$('tr.clarification-request-row').hover(
			function() {
				$(this).addClass('row-hover');
				$(this).addClass('pointer-hover');
			},
			function() {
				$(this).removeClass('row-hover');
				$(this).removeClass('pointer-hover');
			}
		);
	}

	function getClarificationRequestsReplyHandler(msg) {
		if (msg.data.name != 'ok') {
			// Ick. What do we do here? For now, spam a popup and return.
			window.jAlert('Error updating clarification requests: ' + msg.data.headers.msg);
			return;
		}

		var requests = new Array();

		// We iterate over the request indices until we find one which is not
		// defined in the message.
		var i = 0;
		do {
			var request = {};
			request.time = msg.data.headers['time' + i];
			if (typeof request.time === 'undefined') {
				break;
			}

			request.id       = msg.data.headers['id' + i];
			request.question = msg.data.headers['question' + i];
			request.problem  = msg.data.headers['problem' + i];
			request.user_id  = msg.data.headers['user_id' + i];

			requests.push(request);

			i++;
		} while (true);

		clarificationRequests = requests;
		updateClarificationRequestsTable();
	}

	/**
	 * Returns the clarification request with the given id, or false if the id
	 * could not be found.
	 */
	var getClarificationRequestById = function(id) {
		for (var i = 0; i < clarificationRequests.length; i++) {
			var request = clarificationRequests[i];
			if (request.id == id)
				return request;
		}
		return false;
	}

	/**
	 * Returns the clarification with the given id, or false if the id could not
	 * be found.
	 */
	var getClarificationById = function(id) {
		for (var i = 0; i < clarifications.length; i++) {
			var clarification = clarifications[i];
			if (clarification.id == id)
				return clarification;
		}
		return false;
	}

	var showClarificationRequestDetails = function(requestId, replyId, offset) {
		// If requestId is not set, then a clarification reply has been clicked
		// on and we need to determine the request associated with that reply.
		if (requestId < 0)
			requestId = getClarificationById(replyId).req_id;

		// Determine if the request has been replied to, and if so how many
		// replies it has. If more than one, we show the first reply (or the
		// specified reply if one was passed to this function).
		var hasReply = false;
		var multipleReplies = false;
		var replies = new Array();
		var pos = 0;

		for (var j = 0; j < clarifications.length; j++) {
			var clarification = clarifications[j];
			if (clarification.req_id == requestId) {
				if (hasReply)
					multipleReplies = true;
				else
					hasReply = true;

				if (replyId == clarification.id)
					pos = replies.length;

				replies.push(clarification);
			}
		}

		pos += offset;

		// If there are no replies, then hide the answer area and previous/next
		// buttons in the dialog.
		if (!hasReply) {
			$('#clarification-request-details-dialog-answer').hide();
			$('#clarification-request-details-dialog-answer-label').hide();
			$('#clarification-request-details-dialog-previous').hide();
			$('#clarification-request-details-dialog-next').hide();

			// If there are no replies, then the user must have clicked on an
			// unanswered clarification request (i.e. one of their own
			// requests). As such, we extract the text from the request object.
			var request = getClarificationRequestById(requestId);
			$('#clarification-request-details-dialog-question').val(decodeNewlines(request.question, true));
		}
		else {
			$('#clarification-request-details-dialog-answer').show();
			$('#clarification-request-details-dialog-answer-label').show();
			
			if (multipleReplies) {
				// Disable either the next or previous buttons according to
				// whichever reply we are currently showing. Note that the
				// clarifications are listed in decreasing order of their
				// generation time, i.e. replies[0] is the most recent reply.
				// As such, if we are viewing reply X where X > 0, then reply
				// X-1 would be the _next_ reply chronologically. Similarly,
				// reply X+1 is the previous reply chronologically.
				if (pos > 0)
					$('#clarification-request-details-dialog-next').show();
				else
					$('#clarification-request-details-dialog-next').hide();
				if (pos < replies.length - 1)
					$('#clarification-request-details-dialog-previous').show();
				else
					$('#clarification-request-details-dialog-previous').hide();
			} else {
				// No other replies, so hide both the previous and next buttons.
				$('#clarification-request-details-dialog-previous').hide();
				$('#clarification-request-details-dialog-next').hide();
			}

			// Note that we obtain the question from the clarification reply, to
			// cater for general replies for which the user will not have the
			// associated clarification request in their data.
			$('#clarification-request-details-dialog-question').val(decodeNewlines(replies[pos].question, true));
			$('#clarification-request-details-dialog-answer').val(decodeNewlines(replies[pos].answer, true));
			$('#clarification-request-details-dialog-cur-reply').val(replies[pos].id);
		}
		
		// Show the dialog
		$('#clarification-request-details-dialog').dialog('open');
	}
	
	this.updateClarifications = function(msg) {
		var clarification = {};
		clarification.id = msg.data.headers.id;
		clarification.req_id = msg.data.headers.req_id;
		clarification.time = msg.data.headers.time;
		clarification.problem = msg.data.headers.problem;
		clarification.question = msg.data.headers.question;
		clarification.answer = msg.data.headers.answer;

		clarifications.push(clarification);
		updateClarificationsTable();

		// Also update the clarification requests table, since one of
		// our requests may have been answered.
		updateClarificationRequestsTable();
	}

	this.getClarifications = function() {
		sendMessageBlock({
				name: 'getclarifications',
				headers: {}
			},
			getClarificationsReplyHandler
		);
	}

	function updateClarificationsTable() {
		// First make sure that the clarifications are correctly sorted,
		// from newest to oldest.
		clarifications.sort(function(a, b) {
			var diff = b.time - a.time;
			if (diff == 0)
				diff = b.id - a.id;
			return diff;
		});

		var html = '';
		for (var i = 0; i < clarifications.length; i++) {
			var clarification = clarifications[i];

			html += '<tr class="clarification-row">' +
				'<td>' + clarification.id + '</td>' +
				'<td>' + clarification.req_id + '</td>' +
				'<td>' + escapeHTML(epochTimeToString(clarification.time)) + '</td>' +
				'<td>' + escapeHTML(clarification.problem) + '</td>' +
				'<td>' + decodeNewlines(escapeHTML(shortText(clarification.question)), false) + '</td>' +
				'<td>' + decodeNewlines(escapeHTML(shortText(clarification.answer)), false) + '</td>' +
				'</tr>';
		}

		$('#clarifications-tbody').html(html);
		$('tr.clarification-row').click(function() {

			var replyId = $($(this).children()[0]).text();
			showClarificationRequestDetails(-1, replyId, 0);
		});
		$('tr.clarification-row').hover(
			function() {
				$(this).addClass('row-hover');
				$(this).addClass('pointer-hover');
			},
			function() {
				$(this).removeClass('row-hover');
				$(this).removeClass('pointer-hover');
			}
		);
	}

	function getClarificationsReplyHandler(msg) {
		if (msg.data.name != 'ok') {
			// Ick. What do we do here? For now, spam a popup and return.
			window.jAlert('Error updating clarifications: ' + msg.data.headers.msg);
			return;
		}

		var items = new Array();

		// We iterate over the request indices until we find one which is not
		// defined in the message.
		var i = 0;
		do {
			var clarification = {};

			clarification.time = msg.data.headers['time' + i];
			if (typeof clarification.time === 'undefined') {
				break;
			}

			clarification.req_id   = msg.data.headers['req_id' + i];
			clarification.id       = msg.data.headers['id' + i];
			clarification.question = msg.data.headers['question' + i];
			clarification.problem  = msg.data.headers['problem' + i];
			clarification.answer   = msg.data.headers['answer' + i];

			items.push(clarification);

			i++;
		} while (true);

		clarifications = items;
		updateClarificationsTable();
	}

	this.requestClarificationDialog = function(problemId, question) {
		// In order to display the clarification request dialog, we need to
		// query for the set of problems from the server first. Note that we
		// need to do this every time to handle the fact that the set of
		// problems for which we may request clarification may change (and
		// the server does not send us updates of the set in the current
		// protocol).
		getProblems();
		queueHandler({problemId: problemId, question: question},
					 showClarificationRequestDialog);
	}

	function showClarificationRequestDialog(msg) {
		if (msg.data.data.problemId != '') {
			$('#clarification-request-dialog-problems').val(msg.data.data.problemId);
		}

		$('#clarification-request-dialog-question').val(msg.data.data.question);
		$('#clarification-request-dialog').dialog('open');
	}

	function clarificationRequestReplyHandler(msg) {
		if (msg.data.name == 'ok')
			$('#clarification-request-dialog').dialog('close');
		else
			window.jAlert('Failed to send clarification request. Error was: ' + msg.data.headers.msg);
	}

	$(document).ready(function() {
		// Clarification reuest details dialog
		$('#clarification-request-details-dialog').dialog({
			disabled: false,
			autoOpen: false,
			closeOnEscape: true,
			draggable: true,
			modal: true,
			position: 'center',
			resizable: false,
			title: 'Details of clarification request',
			width: 'auto',
			height: 'auto',
		});

		$('#clarification-request-details-dialog-cur-reply').hide();

		$('#clarification-request-details-dialog-ok').click(function(event) {
			$('#clarification-request-details-dialog').dialog('close');
		});
		
		$('#clarification-request-details-dialog-previous').click(function(event) {
			var replyId = $('#clarification-request-details-dialog-cur-reply').val();
			showClarificationRequestDetails(-1, replyId, 1);
		});
		
		$('#clarification-request-details-dialog-next').click(function(event) {
			var replyId = $('#clarification-request-details-dialog-cur-reply').val();
			showClarificationRequestDetails(-1, replyId, -1);
		});
		
		$('#clarification-requests-request').click(function(event) {
			requestClarificationDialog('', '');
		});

		$('#clarification-request-dialog-submit').click(function(event) {
			var problemId = $('#clarification-request-dialog-problems').val();
			if (problemId == -1) {
				window.jAlert('You must select a problem!');
				return;
			}

			var question = $('#clarification-request-dialog-question').val();

			sendMessageBlock({
					name: 'clarificationrequest',
					headers: {
						prob_id: problemId,
						question: encodeNewlines(question)
					}
				},
				clarificationRequestReplyHandler
			);
		});

		$('#clarification-request-dialog-cancel').click(function(event) {
			$('#clarification-request-dialog').dialog('close');
		});
	});
})(jQuery);

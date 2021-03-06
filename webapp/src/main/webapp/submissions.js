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
// Submissions handling
// ---------------------------------------------------------------------

(function($) {
	var submissions = new Array();
	var submissionUploader;

	var problemFilter = {}; // Maps problem IDs to true or false
	var resultFilter = {};  // Maps RunResult enums to true or false
	var filterActive = false; // Overrides problemFilter and resultFilter

	function newUploader() {
		submissionUploader = new qq.FileUploader({
			element: document.getElementById('submission-dialog-file-selector'),
			action: 'submissionUpload',
			debug: true,
			onComplete: submissionUploaded,
			sizeLimit: 1048576,
			multiple: false
		});
	}

	function submissionUploaded(id, filename, response) {
		$('#submission-dialog-submit').button({disabled: false});
		$('#submission-dialog-file-key').val(response.key);
		$('.qq-upload-button').hide();
	}

	var parseResult = function(result) {
		if (result == 'PENDING') {
			return RunResult.PENDING;
		}
		else {
			return parseInt(result);
		}
	}

	var filterMatches = function(submission) {
		if (!filterActive)
			return true;
		// If a new problem has turned up but is not in the filter,
		// assume it should be shown
		return (!(submission.prob_id in problemFilter)
				|| problemFilter[submission.prob_id])
			&& resultFilter[submission.result];
	}

	this.updateSubmissionsFilter = function() {
		var problems = getCachedProblems();
		var rows = Math.max(problems.length, RunResult.OTHER - RunResult.PENDING + 1);
		var html = '';
		for (var i = 0; i < rows; i++) {
			html += '<tr><td>';
			if (i < problems.length) {
				var problem = problems[i];
				if (!(problem.id in problemFilter)) {
					problemFilter[problem.id] = true;
				}
				var id = "filter-problem-" + problem.id;
				html += '<input type="checkbox" id="' + id + '"';
				if (problemFilter[problem.id])
					html += ' checked="checked"';
				if (!filterActive)
					html += ' disabled="disabled"';
				html += '><label for="' + id + '">'
					+ escapeHTML(problems[i].code) + ' &mdash; '
					+ escapeHTML(problems[i].name) + '</label>';
			}
			else
				html += '&nbsp;';
			html += '</td><td>';
			var result = i + RunResult.PENDING;
			if (result <= RunResult.OTHER) {
				var id = "filter-result-" + result;
				html += '<input type="checkbox" id="' + id + '"';
				if (resultFilter[result])
					html += ' checked="checked"';
				if (!filterActive)
					html += ' disabled="disabled"';
				html += '><label for="' + id + '">' + escapeHTML(runResultString(result)) + '</label>';
			}
			else
				html += '&nbsp;';
			html += '</td></tr>';
		}
		$('#submissions-filter-table-body').html(html);
	}

	this.getSubmissions = function() {
		sendMessageBlock({
				name: 'getsubmissions',
				headers: {}
			},
			getSubmissionsReplyHandler
		);
	}

	var getSubmissionsReplyHandler = function(msg) {
		if (msg.data.name != 'ok') {
			// Ick. What do we do here? For now, spam a popup and return.
			window.jAlert('Error updating submissions: ' + msg.data.headers.msg);
			return;
		}

		var items = new Array();

		// We iterate over the request indices until we find one which is not
		// defined in the message.
		var i = 0;
		do {
			var submission = {};

			submission.time = msg.data.headers['time' + i];
			if (typeof submission.time === 'undefined') {
				break;
			}

			submission.submission_id = msg.data.headers['submission_id' + i];
			submission.contesttime = msg.data.headers['contesttime' + i];
			submission.prob_id = msg.data.headers['prob_id' + i];
			submission.problem = msg.data.headers['problem' + i];
			submission.comment = msg.data.headers['comment' + i];
			submission.result = parseResult(msg.data.headers['result' + i]);

			items.push(submission);

			i++;
		} while (true);

		submissions = items;
		updateSubmissionsTable();
	}

	/* Receives an updatesubmissions message, and returns true if the
	 * submission passed the filter and hence should be displayed.
	 */
	this.updateSubmissions = function(msg) {
		// First check if this is a submission that we already know about.
		var id = msg.data.headers.submission_id;
		var index;
		var updating = false;
		for (var i = 0; i < submissions.length; i++) {
			var sub = submissions[i];
			if (sub.submission_id == id) {
				updating = true;
				index = i;
				break;
			}
		}

		var submission = {};
		submission.submission_id = id;
		submission.time = msg.data.headers.time;
		submission.contesttime = msg.data.headers.contesttime;
		submission.prob_id = msg.data.headers.prob_id;
		submission.problem = msg.data.headers.problem;
		submission.comment = msg.data.headers.comment;
		submission.result = parseResult(msg.data.headers.result);

		if (updating)
			submissions[index] = submission;
		else
			submissions.push(submission);

		updateSubmissionsTable();
		return filterMatches(submission);
	}

	/**
	 * Returns the submission data for the given submission ID.
	 */
	var getSubmissionById = function(id) {
		for (var i = 0; i < submissions.length; i++) {
			var submission = submissions[i];
			if (submission.submission_id == id)
				return submission;
		}
		return false;
	}

	function updateSubmissionsTable() {
		// First make sure that the submissions are correctly sorted,
		// from newest to oldest.
		submissions.sort(function(a, b) {
			var comp = b.time - a.time;
			if (comp == 0)
				comp = b.submission_id - a.submission_id;
			return comp;
		});

		var html = '';
		for (var i = 0; i < submissions.length; i++) {
			var submission = submissions[i];
			if (!filterMatches(submission))
				continue;
			var commentClass = '';
			var rowClass = 'submission-row';
			var clickable = false;
			var extra = '';

			switch (submission.result) {
			case RunResult.COMPILE_FAILED:
				commentClass = 'submission-compilation-failed';
				clickable = true;
				break;
			case RunResult.ABNORMAL:
			case RunResult.WRONG:
			case RunResult.TIME_EXCEEDED:
			case RunResult.FORMAT_ERROR:
				commentClass = 'submission-wrong';
				clickable = true;
				break;
			case RunResult.CORRECT:
				commentClass = 'submission-correct';
				break;
			default:
				commentClass = '';
				break;
			}
			if (clickable)
				extra = ' (click for details)';

			html += '<tr class="' + rowClass + '">' +
				'<td>' + escapeHTML(submission.submission_id) + '</td>' +
				'<td>' + escapeHTML(timeToString(submission.contesttime)) + '</td>' +
				'<td>' + escapeHTML(epochTimeToString(submission.time)) + '</td>' +
				'<td>' + escapeHTML(submission.problem) + '</td>' +
				'<td><div class="' + commentClass + '">' + escapeHTML(submission.comment) + extra + '</div></td>' +
				'</tr>';
		}

		$('#submissions-tbody').html(html);

		$('tr.submission-row').hover(
			function() {
				$(this).addClass('pointer-hover row-hover');
			},
			function() {
				$(this).removeClass('pointer-hover row-hover');
			}
		).click(
			function() {
				var submissionId = $($(this).children()[0]).text();
				showSubmission(getSubmissionById(submissionId));
			}
		);

		$('.abacus-button:not(.ui-state-disabled)')
			.hover(
				function () {
					$(this).addClass('ui-state-hover');
				},
				function () {
					$(this).removeClass('ui-state-hover');
				}
			)
			.mousedown(
				function () {
					$(this).addClass('ui-state-active');
				}
			)
			.mouseup(
				function () {
					$(this).removeClass('ui-state-active');
				}
			);
	}

	function refilterSubmissions(event, ui) {
		var problems = getCachedProblems();
		for (var i = 0; i < problems.length; i++) {
			var problem = problems[i];
			var id = '#filter-problem-' + problem.id;
			problemFilter[problem.id] = $(id).prop('checked');
		}
		for (var i = RunResult.PENDING; i <= RunResult.OTHER; i++) {
			var id = '#filter-result-' + i;
			resultFilter[i] = $(id).prop('checked');
		}
		updateSubmissionsTable();
	}

	function makeSubmissionHandler(msg) {
		submissionUploader.setParams({user: getUsername()});
		$('#submission-dialog-submit').button({'disabled': true});
		$('#submission-dialog').dialog('open');
		$('.qq-upload-button').show();
	}

	function submissionReplyHandler(msg) {
		if (msg.data.name == 'ok') {
			removeUploadedFile();
			$('#submission-dialog').dialog('close');
		}
		else
			window.jAlert('Failed to make submission. Error was: ' + msg.data.headers.msg);
	}

	this.submissionDialogClose = function(event, ui) {
		newUploader();
	}

	function getXMLHttpRequest() {
		if (window.ActiveXObject) { //Test for support for ActiveXObject in IE first (as XMLHttpRequest in IE7 is broken)
			var activexmodes=['Msxml2.XMLHTTP', 'Microsoft.XMLHTTP'] //activeX versions to check for in IE
			for (var i = 0; i < activexmodes.length; i++) {
				try {
					return new ActiveXObject(activexmodes[i]);
				}
				catch(e) {
					//suppress error
				}
			}
		}
		else if (window.XMLHttpRequest) // if Mozilla, Safari etc
			return new XMLHttpRequest();
		else
			return false;
	}

	/**
	 * Helper method to tell the server that the most recently uploaded file should
	 * be removed from its cache. Although the server will do this periodically, this
	 * provides a mechanism for doing so as soon as the client no longer requires the
	 * upload (i.e. as soon as a submission is successful, or is cancelled).
	 */
	function removeUploadedFile() {
		var key = getFileKey();
		if (!key)
			// If no file has been uploaded, then bail out.
			return;

		var request = getXMLHttpRequest();
		if (request) {
			request.open('POST', 'submissionUpload', true);
			request.setRequestHeader('X-Requested-With', 'XMLHttpRequest');
			request.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
			request.send('remove&user=' + getUsername() + '&key=' + key);

			// We don't care about the result of this request -- if it is unsuccessful,
			// the server will eventually end up removing this upload anyways during
			// its periodic cleanup.
		} else {
			// We're unable to instruct the server to remove this upload, but that's
			// OK since it will periodically purge uploads anyways.
		}
	}

	/**
	 * Get the key of the file that was uploaded to the server, if any.
	 */
	function getFileKey() {
		var fileKey = $('#submission-dialog-file-key').val();
		if (fileKey != '')
			return fileKey;
		else
			return false;
	}

	$(document).ready(function() {
		newUploader();

		$('#submissions-filter-accordion').accordion({
			collapsible: true,
			active: false,
			heightStyle: 'content',
			header: 'h3'
		});
		$('#submissions-filter-table').on('change', 'input', null, refilterSubmissions);
		for (var i = RunResult.PENDING; i <= RunResult.OTHER; i++) {
			resultFilter[i] = false;
		}
		resultFilter[RunResult.JUDGE] = true;
		resultFilter[RunResult.OTHER] = true;
		$('#submissions-filter-enable').change(function (event, ui) {
			filterActive = $(this).prop('checked');
			$('#submissions-filter-table :input').prop('disabled', !filterActive);
			updateSubmissionsFilter();
			refilterSubmissions();
		}).change();

		$('#submissions-make-submission').click(function(event) {
			// In order to display the submission dialog, we need to query for
			// the set of problems and languages from the server first. Note
			// that we need to do this every time to handle the fact that the
			// set of problems (and possibly languages) for which we may make
			// submissions may change (and the server does not send us updates
			// of the set in the current protocol).
			getProblems();
			getLanguages();
			queueHandler({}, makeSubmissionHandler);
		});

		$('#submission-dialog-cancel').click(function(event) {
			removeUploadedFile();
			$('#submission-dialog').dialog('close');
		});

		$('#submission-dialog-submit').click(function(event) {
			// Check that a language and a problem have been selected.
			var problemId = $('#submission-dialog-problems').val();
			if (problemId == -1) {
				window.jAlert('You must select a problem!');
				return;
			}

			var language = $('#submission-dialog-languages').val();
			if (language == -1) {
				window.jAlert('You must select a language!');
				return;
			}

			sendMessageBlock({
					name: 'submit',
					headers: {
						prob_id: problemId,
						lang: language,
					},
					file_key: getFileKey(),
				},
				submissionReplyHandler
			);
		});
	});
})(jQuery);

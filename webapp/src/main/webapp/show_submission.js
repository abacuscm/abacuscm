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
// Submission judging (also shows compilation failure to contestant)
// ---------------------------------------------------------------------

(function($) {
	var submissionFiles = new Array();

	this.showSubmission = function (submission) {
		submissionId = submission.submission_id;
		sendMessageBlock({
				name: 'fetchfile',
				headers: {
					request: 'count',
					submission_id: submissionId,
				}
			},
			function (msg) {
				if (msg.data.name == 'ok')
				{
					submissionFiles = new Array();
					var count = msg.data.headers.count;
					for (var i = 0; i < count; i++)
					{
						sendMessageBlock({
								name: 'fetchfile',
								headers: {
									request: 'data',
									submission_id: submissionId,
									index: '' + i
								}
							},
							getFileHandler
						);
					}
					if (hasPermission('see_problem_details')) {
						sendMessageBlock({
								name: 'getprobfile',
								headers: {
									prob_id: submission.prob_id,
									file: 'testcase.output'
								}
							},
							function (msg) { getFileHandler(msg, 'Expected output'); }
						);
					}
					sendMessageBlock({
							name: 'getsubmissionsource',
							headers: {
								submission_id: submissionId
							}
						},
						function (msg) { getFileHandler(msg, 'Contestant source'); }
					);
					// Once all the files have been retrieved, show the submission dialog
					queueHandler(submissionId, showSubmissionHandler);
				}
				else
					defaultHandler(msg);
			}
		);
	}

	var getFileHandler = function (msg, name) {
		if (msg.data.name == 'ok') {
			content = parseUtf8(msg.data.content);
			if (typeof name === 'undefined') {
				name = msg.data.headers.name;
			}
			submissionFiles.push({name: name, content: content});
		}
		else
			defaultHandler(msg);
	}

	var showSubmissionHandler = function (msg) {
		html = ''
		for (var i = 0; i < submissionFiles.length; i++) {
			var name = submissionFiles[i].name;
			html += '<option value="' + i + '">' + escapeHTML(name) + '</option>';
		}
		$('#submission-result-dialog-file').html(html);
		$('#submission-result-dialog-file').val(0);
		$('#submission-result-dialog-file').on('change keypress keyup', selectFileHandler);
		$('#submission-result-dialog-file').change();
		var judge = hasPermission('judge');
		$('#submission-result-dialog-buttons-contestant').toggle(!judge);
		$('#submission-result-dialog-buttons-judge').toggle(judge);
		$('#submission-result-dialog').dialog('open');
	}

	var selectFileHandler = function () {
		var idx = $(this).val();
		if (idx >= 0 && idx < submissionFiles.length) {
			$('#submission-result-dialog-contents').val(submissionFiles[idx].content);
		}
	}

	$(document).ready(function() {
		$('#submission-result-dialog-ok').click(function(event) {
			$('#submission-result-dialog').dialog('close');
		});
		// Hack to work around https://bugzilla.mozilla.org/show_bug.cgi?id=82711 on Firefox
		$('#submission-result-dialog-contents')[0].wrap = 'off';
	});

})(jQuery);

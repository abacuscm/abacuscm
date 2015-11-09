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
		var submissionId = submission.submission_id;

		// Run either after fetching the mark-related files (if permissable),
		// otherwise immediately. This is done like this so that these files
		// also come after the mark-related files.
		finishFetch = function() {
			if (hasPermission('see_problem_details')) {
				sendMessageBlock({
						name: 'getprobfile',
						headers: {
							prob_id: submission.prob_id,
							file: 'testcase.output'
						}
					},
					function (msg) { getFileHandler(msg, 'Expected output', true); }
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
			queueHandler(submission, showSubmissionHandler);
		}

		if (hasPermission('see_submission_details')
			|| submission.result == RunResult.COMPILE_FAILED) {
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
						finishFetch();
					}
					else
						defaultReplyHandler(msg);
				}
			);
		}
		else {
			submissionFiles = new Array();
			var explanation = null;
			switch (submission.result)
			{
			case RunResult.WRONG:
				explanation =
					'When given our test data, your solution produced the wrong answers.\n' +
					'Keep in mind that we test your solution with test data that is\n' +
					'DIFFERENT TO (and usually harder than) the sample data shown in the\n' +
					'question.\n' +
					'\n' +
					'Please do not ask for more details of what you got wrong. That is up\n' +
					'to you to discover.\n';
				break;
			case RunResult.TIME_EXCEEDED:
				explanation =
					'When given our test data, your solution ran for longer than the\n' +
					'allowed time (which should appear on the question paper).\n' +
					'Keep in mind that we test your solution with test data that is\n' +
					'DIFFERENT TO (and usually harder than) the sample data shown in the\n' +
					'question.\n' +
					'\n' +
					'Please do not ask how close your program was to running in time. It\n' +
					'is killed as soon as it runs over time, so we do not know.\n';
				break;
			case RunResult.FORMAT_ERROR:
				explanation =
					'The output from your solution did not match the format specified\n' +
					'in the problem statement. Please check that\n' +
					'- you do not have any debugging print statements\n' +
					'- you have no written any prompts for input\n' +
					'- you have used uppercase and lowercase correctly, if applicable\n' +
					'Hint: when running on the sample input, the output should EXACTLY\n' +
					'match the sample output, with nothing added.\n' +
					'\n' +
					'Please do not ask what is wrong with your formatting. It is up\n' +
					'to you to figure this out, and you will not be told.\n';
				break;
			case RunResult.ABNORMAL:
				explanation =
					'When given our test data, your solution did not terminate cleanly.\n' +
					'There are many reasons this can happen, including\n' +
					'- throwing an uncaught exception\n' +
					'- exceeding the resource limits (such as memory)\n' +
					'- invalid Python syntax\n' +
					'- returning an exit code other than 0 from C/C++\n' +
					'- invalid memory use in C/C++\n' +
					'- invalid mathematical operations (e.g., divide by zero)\n' +
					'For more details, refer to the manual (left-hand tab).\n' +
					'Keep in mind that we test your solution with test data that is\n' +
					'DIFFERENT TO (and usually harder than) the sample data shown in the\n' +
					'question.\n' +
					'\n' +
					'Please do not ask for more details of your error. You will not be told.\n';
				break;
			}
			if (explanation !== null)
			{
				submissionFiles.push({name: 'Explanation', content: explanation});
			}
			finishFetch();
		}
	}

	var getFileHandler = function (msg, name, ignore_errors) {
		if (msg.data.name == 'ok') {
			var content = parseUtf8(msg.data.content);
			if (typeof name === 'undefined') {
				name = msg.data.headers.name;
			}
			submissionFiles.push({name: name, content: content});
		}
		else if (ignore_errors && msg.data.name == 'err')
		{
			// Suppress the error.
			// TODO: this is a hack to deal with not being able to query the
			// problem type, and hence not knowing whether certain files can
			// be safely retrieved.
		}
		else
			defaultReplyHandler(msg);
	}

	var showSubmissionHandler = function (msg) {
		var submission = msg.data.data;
		var submissionId = submission.submission_id;
		var mayJudge =
			(hasPermission('judge') && submission.result == RunResult.JUDGE)
			|| hasPermission('judge_override');

		html = ''
		for (var i = 0; i < submissionFiles.length; i++) {
			var name = submissionFiles[i].name;
			html += '<option value="' + i + '">' + escapeHTML(name) + '</option>';
		}
		$('#submission-result-dialog-file')
			.html(html)
			.val(0)
			.on('change keypress keyup', selectFileHandler)
			.change();
		if (mayJudge) {
			$('#submission-result-dialog').dialog({
					buttons: [
						{
							text: 'Defer',
							click: function() { $(this).dialog('close'); }
						},
						{
							text: 'Correct',
							click: function() { mark(submissionId, RunResult.CORRECT); }
						},
						{
							text: 'Wrong',
							click: function() { mark(submissionId, RunResult.WRONG); }
						},
						{
							text: 'Format error',
							click: function() { mark(submissionId, RunResult.FORMAT_ERROR); }
						}
					]
				}
			);
		}
		else {
			$('#submission-result-dialog').dialog({
					buttons: [
						{
							text: 'Ok',
							click: function() { $(this).dialog('close'); }
						},
						{
							text: 'Request clarification',
							click: function() {
								requestClarificationDialog(submission.prob_id, '[Submission ' + submissionId + '] ');
							}
						}
					]
				}
			);
		}
		$('#submission-result-dialog').dialog('open');
	}

	var selectFileHandler = function () {
		var idx = $(this).val();
		if (idx >= 0 && idx < submissionFiles.length) {
			$('#submission-result-dialog-contents').val(submissionFiles[idx].content);
		}
	}

	var mark = function (submissionId, runResult) {
		sendMessageBlock({
				name: 'mark',
				headers: {
					submission_id: submissionId,
					result: '' + runResult,
					comment: runResultString(runResult)
				}
			},
			markHandler
		);
	}

	var markHandler = function (msg) {
		if (msg.data.name == 'ok') {
			$('#submission-result-dialog').dialog('close');
		}
		else {
			defaultReplyHandler(msg);
		}
	}

	$(document).ready(function() {
		// Hack to work around https://bugzilla.mozilla.org/show_bug.cgi?id=82711 on Firefox
		$('#submission-result-dialog-contents')[0].wrap = 'off';

		$('#submission-result-dialog-wrap').change(function (event, ui) {
			var content = $('#submission-result-dialog-contents');
			if ($(this).prop('checked')) {
				content.removeClass('nowrap');
				content[0].wrap = 'on';
			}
			else {
				content.addClass('nowrap');
				content[0].wrap = 'off';
			}
		});
	});

})(jQuery);

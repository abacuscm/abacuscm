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
// Problem and language information related methods
// ---------------------------------------------------------------------

(function($) {
	var problems = new Array();
	var languages = new Array();

	// Returns the problem with the given id.
	this.getProblem = function(id) {
		for (var i = 0; i < problems.length; i++) {
			if (problems[i].id == id)
				return problems[i];
		}
		return {};
	}

	this.getProblemForCode = function(code) {
		for (var i = 0; i < problems.length; i++) {
			if (problems[i].code == code)
				return problems[i];
		}
		return {};
	}

	this.getProblems = function() {
		sendMessageBlock({
				name: 'getproblems',
				headers: {}
			},
			getProblemsReplyHandler
		);
	}

	var getProblemsReplyHandler = function(msg) {
		if (msg.data.name != 'ok') {
			// Ick. What do we do here? For now, spam a popup and return.
			window.jAlert('Error fetching problem info: ' + msg.data.headers.msg);
			return;
		}

		// We iterate over the request indices until we find one which is not
		// defined in the message.
		var i = 0;

		problems = new Array();
		do {
			var problem = {};

			problem.id = msg.data.headers['id' + i];
			if (typeof problem.id === 'undefined') {
				break;
			}

			problem.name = msg.data.headers['name' + i];
			problem.code = msg.data.headers['code' + i];

			problems.push(problem);
			i++;
		} while (true);

		// And update anything that depends on the set of problems
		// (e.g. combo-boxes for dialogs).
		updateProblemComboBoxes();
	}

	var updateProblemComboBoxes = function() {
		var headerHtml = '<option value="-1">Select a problem...</option>';
		var html = '';
		for (var i = 0; i < problems.length; i++) {
			var problem = problems[i];
			html += '<option value="' + escapeHTML(problem.id) + '">' + escapeHTML(problem.code) + ' - ' + escapeHTML(problem.name) + '</option>';
		}

		$('#clarification-request-dialog-problems').html(headerHtml + '<option value="0">General</option>' + html);
		$('#submission-dialog-problems').html(headerHtml + html);
	}

	this.getLanguages = function() {
		sendMessageBlock({
				name: 'getlanguages',
				headers: {}
			},
			getLanguagesReplyHandler
		);
	}

	function getLanguagesReplyHandler(msg) {
		if (msg.data.name != 'ok') {
			// Ick. What do we do here? For now, spam a popup and return.
			window.jAlert('Error fetching language info: ' + msg.data.headers.msg);
			return;
		}

		// We iterate over the request indices until we find one which is not
		// defined in the message.
		var i = 0;

		languages = new Array();
		do {
			var language = msg.data.headers['language' + i];
			if (typeof language === 'undefined') {
				break;
			}

			languages.push(language);
			i++;
		} while (true);

		// And update anything that depends on the set of languages
		// (e.g. combo-boxes for dialogs).
		updateLanguageComboBoxes();
	}

	function updateLanguageComboBoxes() {
		var headerHtml = '<option value="-1">Select a language...</option>';
		var html = '';
		for (var i = 0; i < languages.length; i++) {
			var language = languages[i];
			html += '<option value="' + escapeHTML(language) + '">' + escapeHTML(language) + '</option>';
		}

		$('#submission-dialog-languages').html(headerHtml + html);
	}
})(jQuery);

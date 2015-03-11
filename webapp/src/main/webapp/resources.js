/*  Copyright (C) 2015  Bruce Merry
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
// Resources page
// ---------------------------------------------------------------------

(function($) {
	var updateResources = function(force) {
		ifModified = force ? false : true;
		$.ajax('../resources/', {dataType: 'html', ifModified: ifModified})
		.success(function(data, textStatus, jqXHR) {
			if (data)
			{
				var body = $("<div>").append($.parseHTML(data)).find("#body");
				$('#tab-resources').html(body);
				$('.tab-resources').removeClass('tab-hidden');
			}
		})
		.complete(function(data, textStatus, jqXHR) {
			setTimeout(updateResources, 15000);
		});
	};

	$(document).ready(function() {
		updateResources(true);
	});
})(jQuery);

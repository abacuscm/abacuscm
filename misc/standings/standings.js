data = [];
data_regex = null;
first_time = true;

STANDING_RAW_ID = 0;
STANDING_RAW_USERNAME = 1;
STANDING_RAW_FRIENDLYNAME = 2;
STANDING_RAW_GROUP = 3;
STANDING_RAW_CONTESTANT = 4;
STANDING_RAW_TOTAL_SOLVED = 5;
STANDING_RAW_TOTAL_TIME = 6;
STANDING_RAW_SOLVED = 7;

COLUMN_PLACE = 0;
COLUMN_USERNAME = 1;
COLUMN_FRIENDLYNAME = 2;
COLUMN_TOTAL_SOLVED = 3;
COLUMN_TOTAL_TIME = 4;
COLUMN_SOLVED = 5;

function escapeHTML(str)
{
	return str.split("&").join("&amp;").split("<").join("&lt;").split(">").join("&gt;");
}

function padNumber(num)
{
	var s = "" + num;
	while (s.length < 2) s = "0" + s;
	return s;
}

function timeToString(time)
{
	var seconds = time % 60;
	var minutes = (time - seconds) / 60 % 60;
	var hours = (time - 60 * minutes - seconds) / 3600;
	return padNumber(hours) + ":" + padNumber(minutes) + ":" + padNumber(seconds);
}

function same_score(arr1, arr2)
{
	var L = arr1.length;
	if (arr2.length != L)
		return false;
	return arr1[STANDING_RAW_ID] == arr2[STANDING_RAW_ID]
		&& arr1[STANDING_RAW_TOTAL_SOLVED] == arr2[STANDING_RAW_TOTAL_SOLVED]
		&& arr1[STANDING_RAW_TOTAL_TIME] == arr2[STANDING_RAW_TOTAL_TIME];
}

/* Updates the table in response to either new data or an updated search.
 * In the latter case, pass null for new_data.
 */
function update_table(new_data)
{
	var old_data = data;
	if (new_data !== null)
		data = new_data;

	var box = $("#searchbox");
	try
	{
		if (box[0].value == '')
			data_regex = null;
		else
			data_regex = new RegExp(box[0].value, 'i');
		box.removeClass('invalid');
	}
	catch (ex)
	{
		box.addClass('invalid');
	}

	var html = '';
	var seen = {};
	var old_row = 0;
	var highlight_rows = [];
	var last_solved;
	var last_time;
	var tie_place = 1;
	var row;
	var parity = 1;
	for (row = 0; row < data.length; row++)
	{
		var classes = (parity & 1) ? 'odd' : 'even';
		if (old_data !== data)
		{
			while (old_row < old_data.length
				   && typeof(seen[old_data[old_row][STANDING_RAW_ID]]) != 'undefined')
				old_row = old_row + 1;
			if (!first_time && 
				(old_row >= old_data.length || !same_score(old_data[old_row], data[row])))
				classes += ' do_highlight';
		}

		var C = data[row].length;
		if (last_solved != data[row][STANDING_RAW_TOTAL_SOLVED] || last_time != data[row][STANDING_RAW_TOTAL_TIME])
			tie_place = row + 1;
		last_solved = data[row][STANDING_RAW_TOTAL_SOLVED];
		last_time = data[row][STANDING_RAW_TOTAL_TIME];

		if (data_regex !== null && data[row][STANDING_RAW_USERNAME].match(data_regex))
			classes += ' search';

		html += '<tr class="' + classes + '">';
		fields = [];
		fieldclass = [];
		fields[COLUMN_PLACE] = tie_place;
		fieldclass[COLUMN_PLACE] = 'column_place';
		fields[COLUMN_USERNAME] = escapeHTML(data[row][STANDING_RAW_USERNAME]);
		fieldclass[COLUMN_USERNAME] = 'column_username';
		fields[COLUMN_FRIENDLYNAME] = escapeHTML(data[row][STANDING_RAW_FRIENDLYNAME]);
		fieldclass[COLUMN_FRIENDLYNAME] = 'column_friendlyname';
		fields[COLUMN_TOTAL_TIME] = escapeHTML(timeToString(data[row][STANDING_RAW_TOTAL_TIME]));
		fieldclass[COLUMN_TOTAL_TIME] = 'column_total_time';
		fields[COLUMN_TOTAL_SOLVED] = escapeHTML(data[row][STANDING_RAW_TOTAL_SOLVED]);
		fieldclass[COLUMN_TOTAL_SOLVED] = 'column_total_solved';
		for (i = STANDING_RAW_SOLVED; i < data[row].length; i++)
		{
			fieldclass.push('column_solved');
			if (data[row][i] > 1)
				fields.push('<span class="correct">+' + (data[row][i] - 1) + '</span>');
			else if (data[row][i] == 1)
				fields.push('<span class="correct">+</span>')
			else if (data[row][i] < 0)
				fields.push('<span class="incorrect">&minus;' + (-data[row][i]) + '</span>');
			else
				fields.push('');
		}
		for (f in fields)
			html += '<td class="' + fieldclass[f] + '">' + fields[f] + '</td>';
		html += '</tr>';
		parity++;

		seen[data[row][STANDING_RAW_ID]] = 1;
	}

	$("#tbody").html(html);
	if (old_data !== data)
		$(".do_highlight").effect('highlight', {color: "#33ff33"}, 3000);
}

function data_callback(text, textStatus)
{
	var i;
	var lines = text.split('\n');

	var new_data = [];
	for (i in lines)
		if (lines[i] != "")
		{
			var fields = lines[i].split('\t');
			if (fields.length > 0)
				new_data.push(fields);
		}

	var raw_header = new_data[0];
	new_data.shift();

	var html = '';
	for (i = 0; i < raw_header.length - STANDING_RAW_SOLVED; i++)
		html += '<col />';
	$("colgroup.problem").html(html);

	html = '<tr><th>Place</th><th>Team</th><th>Name</th><th>Points</th><th>Time</th>';
	for (i = STANDING_RAW_SOLVED; i < raw_header.length; i++)
		html += '<th>' + raw_header[i] + '</th>';
	html += '</tr>';
	$("#thead").html(html);

	update_table(new_data);

	$("#updatedat").text('Updated at ' + new Date());
	first_time = false;
}

function update()
{
	$.get('standings.txt', data_callback, 'text');
}

$(document).ready(function()
{
	// Make Javascript-only elements visible
	$(".jsonly").removeClass('jsonly');
	$("#searchbox").keypress(function(e)
	{
		// If we try to do this immediately, we get the old input value
		setTimeout(function() { update_table(null); }, 0);
	});
	// IE doesn't send backspace through keypress
	$("#searchbox").keyup(function(e)
	{
		if (e.keyCode == 8)
			setTimeout(function() { update_table(null); }, 0);
	});

	$.ajaxSetup({cache: false});
	update();
	setInterval(function() { update() }, 5000);
});

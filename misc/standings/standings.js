data = [];
first_time = true;

function escapeHTML(str)
{
    return str.split("&").join("&amp;").split("<").join("&lt;").split(">").join("&gt;");
}

function same_score(arr1, arr2)
{
    var L = arr1.length;
    if (arr2.length != L)
        return false;
    return arr1[0] == arr2[0] && arr1[L - 2] == arr2[L - 2] && arr1[L - 1] == arr2[L - 1];
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

    var header = new_data[0];
    new_data.shift();
    var old_data = data;
    data = new_data;

    var html = '';
    for (i = 0; i < header.length - 2; i++)
        html += '<col />';
    $("colgroup.problem").html(html);

    html = '<tr><th>Place</th>';
    for (i in header)
        html += '<th>' + header[i] + '</th>';
    html += '</tr>';
    $("#thead").html(html);

    html = '';
    var seen = {};
    var old_row = 0;
    var highlight_rows = [];
    var last_score;
    var last_time;
    var tie_place = 1;
    var row;
    for (row = 0; row < data.length; row++)
    {
        var classes = (row & 1) ? 'odd' : 'even';
        while (old_row < old_data.length
               && typeof(seen[old_data[old_row][0]]) != 'undefined')
            old_row = old_row + 1;
        if (!first_time && 
            (old_row >= old_data.length || !same_score(old_data[old_row], data[row])))
            classes += ' do_highlight';

        html += '<tr class="' + classes + '">';
        var C = data[row].length;
        if (last_score != data[row][C - 2] || last_time != data[row][C - 1])
            tie_place = row + 1;
        html += '<td>' + tie_place + '</td>';
        last_score = data[row][C - 2];
        last_time = data[row][C - 1];

        for (i in data[row])
        {
            var s = escapeHTML(data[row][i]);
            if (i > 0 && i < C - 2)
                s = s.replace('Yes', '<span class="correct">Yes</span>');
            html += '<td>' + s + '</td>';
        }
        html += '</tr>';

        seen[data[row][0]] = 1;
    }

    $("#tbody").html(html);
    $(".do_highlight").effect('highlight', {color: "#33ff33"}, 3000);
    $("#updatedat").text('Updated at ' + new Date());
    first_time = false;
}

function update()
{
    $.get('standings.txt', data_callback, 'text');
}

$(document).ready(function()
{
    $.ajaxSetup({cache: false});
    update();
    setInterval("update()", 5000);
});

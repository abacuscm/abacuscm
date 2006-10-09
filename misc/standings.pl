#!/usr/bin/perl -w
use strict;
use warnings;
use CGI::Pretty qw/:standard *table/;

my $standings_path = "/home/bmerry/devel/abacuscm/standings.txt";
if (!open(IN, '<', $standings_path))
{
	print(header(-status => 500),
		start_html("ACM ICPC South Africa"),
		p(strong("Error: could not open " . escapeHTML($standings_path))),
		end_html);
	exit 0;
}


print(header,
	start_html(
		-title => "ACM ICPC South Africa",
		-style => {-src => "icpc.css"}
	),
	h1("ACM ICPC standings"),
	start_table({-border => undef}),
        Tr(th([split(/\t|\n/, <IN>)])));
while (<IN>)
{
	print Tr(td([split /\t|\n/]));
}
print(end_table,
	p(small("Generated at " . escapeHTML(scalar(localtime())) . ".")),
	end_html);
close(IN);

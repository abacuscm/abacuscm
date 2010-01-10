#!/usr/bin/perl -w -T
use strict;
use warnings;
use CGI::Pretty qw/:standard *table *tbody/;

# User-configurable stuff
my $standings_path = '/home/bruce/compolymp/sbitc2010/abacuscm/standings.txt';
my $css_path = '/abacuscm.css';  # From web server point of view
my $local_regex = qr/^(?:uct|sun|uwc)/;
my $title = 'ACM ICPC South Africa';
my $header = 'ACM ICPC Standings';

if (!open(IN, '<', $standings_path))
{
	print(header(-status => 500),
		start_html($title),
		p(strong("Error: could not open " . escapeHTML($standings_path))),
		end_html);
	exit 0;
}

my @stat = stat(IN);
print(header(-charset => 'utf-8'),
	start_html(
		-encoding => 'utf-8',
		-title => $title,
		-style => {-src => $css_path},
		-head => meta({-http_equiv => 'Refresh', -content => '30' })
	),
	h1($header),
	p(small("Generated at " . escapeHTML(scalar(localtime())) . ". " .
			"Standings last changed at " . escapeHTML(scalar(localtime($stat[9]))) . ".")),
	start_table({-border => undef}));
$_ = <IN>;
chomp;
my @fields = split(/\t/);
unshift @fields, 'Place';
print(
	colgroup({-span => 3}),
	colgroup({-class => 'score', -span => scalar(@fields) - 5}),
	colgroup({-span => 2}),
	thead(Tr(th([map { escapeHTML($_) } @fields]))),
	start_tbody);
my $place = 0;
my $tie_place = 1;
my $last_solved = -1;
my $last_time = '';
while (<IN>)
{
	chomp;
	my @fields = split(/\t/);
	next unless scalar(@fields);
	my $atlocal = ($fields[0] =~ $local_regex);

	$place++;
	my $solved = $fields[$#fields - 1];
	my $time = $fields[$#fields];
	if ($solved ne $last_solved || $time ne $last_time)
	{
		$tie_place = $place;
		$last_solved = $solved;
		$last_time = $time;
	}

	unshift @fields, $tie_place;
	@fields = map { escapeHTML($_) } @fields;
	for (@fields[1..$#fields-2])
	{
		s/Yes/span({-class => 'correct'}, 'Yes')/e;
	}
	my $class = ($place & 1 ? "odd" : "even");
	if ($atlocal) { $class .= " local"; }
	print Tr({-class => $class}, td([@fields]));
}
print(end_tbody,
	end_table,
	end_html);
close(IN);

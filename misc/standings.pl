#!/usr/bin/perl -w -T
use strict;
use warnings;
use CGI::Pretty qw/:standard *table *tbody/;

my $standings_path = "/home/bmerry/devel/abacuscm/standings.txt";
my $names_path = "/home/bmerry/names.txt";

my %name_map = ();
if (open(NAMES, '<', $names_path))
{
	while (<NAMES>)
	{
		chomp;
		my ($id, $name) = split(/\t/, $_, 2);
		$name_map{$id} = $name;
	}
	close(NAMES);
}

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
		-style => {-src => "abacuscm.css"},
		-head => meta({-http_equiv => 'Refresh', -content => '30' })
	),
	h1("ACM ICPC standings"),
	start_table({-border => undef}));
$_ = <IN>;
chomp;
my @fields = split(/\t/);
unshift @fields, 'Team name';
unshift @fields, 'Place';
print(
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
	$place++;
	my $name = exists($name_map{$fields[0]}) ? $name_map{$fields[0]} : $fields[0];
	my $solved = $fields[$#fields - 1];
	my $time = $fields[$#fields];
	if ($solved ne $last_solved || $time ne $last_time)
	{
		$tie_place = $place;
		$last_solved = $solved;
		$last_time = $time;
	}

	unshift @fields, $name;
	unshift @fields, $tie_place;
	@fields = map { escapeHTML($_) } @fields;
	for (@fields[1..$#fields-2])
	{
		s/Yes/span({-class => 'correct'}, 'Yes')/e;
	}
	print Tr({-class => ($. & 1 ? "odd" : "even")},
		td([@fields]));
}
print(end_tbody,
	end_table,
	p(small("Generated at " . escapeHTML(scalar(localtime())) . ".")),
	end_html);
close(IN);

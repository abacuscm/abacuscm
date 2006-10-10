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
	start_table({-border => undef}),
        thead(Tr(th([map { escapeHTML($_) } split(/\t|\n/, <IN>)]))),
	start_tbody);
while (<IN>)
{
	chomp;
	my @fields = split(/\t/);
	next unless scalar(@fields);
	if (exists($name_map{$fields[0]}))
	{
		$fields[0] = "$name_map{$fields[0]} ($fields[0])";
	}
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

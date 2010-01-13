#!/usr/bin/perl -w -T
use strict;
use warnings;
use CGI qw/:standard/;

# User-configurable stuff
my $title = 'Standings';

# File written with raw standings
my $standings_path = '/home/bruce/public_html/standings/standings.txt';
# HTML template on local filesystem
my $template_path = '/home/bruce/public_html/standings/standings.html';
# Path relative to which one files .css and .js files (undef if not needed)
my $base_url = 'http://localhost/~bruce/standings/standings.html';

sub load_file($)
{
	local $/;
	my $fname = shift;
	my $fh;
	if (!open($fh, '<', $fname))
	{
		die "Error: could not open $fname";
	}
	$/ = undef;
	my $content = <$fh> or die("Error: could not read $fname: $!");
	close($fh) or die("Error: could not read $fname: $!");

	return $content;
}

my ($template, $html, %replace);
eval
{
	$template = load_file($template_path);
	open(IN, '<', $standings_path) or die "Error: could not open $standings_path";

	$_ = <IN>;
	chomp;
	my @fields = split(/\t/);
	unshift @fields, 'Place';

	$replace{'COLS'} = '<col/>' x (scalar(@fields) - 4);
	$replace{'HEAD'} = Tr(th([map { escapeHTML($_) } @fields]));
	$replace{'BODY'} = '';
	$replace{'UPDATEDAT'} = 'Updated at ' . escapeHTML(scalar(localtime()));
	if (defined($base_url))
	{
		$replace{'BASE'} = base({-href => $base_url});
	}

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
		$replace{'BODY'} .= Tr({-class => $class}, td([@fields]));
	}
	close(IN) or die("Error reading $standings_path: $!");

	$html = $template;
	while (my ($key, $value) = each(%replace))
	{
		$html =~ s/<!-- $key -->/$value/g;
	}
};

if ($@)
{
	print(header(-status => 500),
		start_html($title),
		p(strong(escapeHTML($@))),
		end_html);
	exit 0;
}
else
{
	print(header(-charset => 'UTF-8'), $html);
}

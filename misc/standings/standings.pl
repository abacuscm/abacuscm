#!/usr/bin/perl -w -T
use strict;
use warnings;
use CGI qw/:standard :pretty/;
use constant {
	STANDING_RAW_ID => 0,
	STANDING_RAW_USERNAME => 1,
	STANDING_RAW_FRIENDLYNAME => 2,
	STANDING_RAW_CONTESTANT => 3,
	STANDING_RAW_TOTAL_SOLVED => 4,
	STANDING_RAW_TOTAL_TIME => 5,
	STANDING_RAW_SOLVED => 6,

	COLUMN_PLACE => 0,
	COLUMN_USERNAME => 1,
	COLUMN_FRIENDLYNAME => 2,
	COLUMN_TOTAL_SOLVED => 3,
	COLUMN_TOTAL_TIME => 4,
	COLUMN_SOLVED => 5
};

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
	my $content = <$fh>;
	defined ($content) or die("Error: could not read $fname: $!");
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

	$replace{'COLS'} = '<col/>' x (scalar(@fields) - STANDING_RAW_SOLVED);
	$replace{'HEAD'} = th([map { escapeHTML($_) } @fields[STANDING_RAW_SOLVED..$#fields]]);
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
		@fields = split(/\t/);
		next unless scalar(@fields);

		$place++;
		my $solved = 0;
		for (my $i = STANDING_RAW_SOLVED; $i < scalar(@fields); $i++)
		{
			if ($fields[$i] > 0)
			{
				$solved++;
			}
		}
		my $time = $fields[STANDING_RAW_TOTAL_TIME];
		if ($solved ne $last_solved || $time ne $last_time)
		{
			$tie_place = $place;
			$last_solved = $solved;
			$last_time = $time;
		}

		my @out_fields = ('') x COLUMN_SOLVED;
		$out_fields[COLUMN_PLACE] = $tie_place;
		$out_fields[COLUMN_USERNAME] = escapeHTML($fields[STANDING_RAW_USERNAME]);
		$out_fields[COLUMN_FRIENDLYNAME] = escapeHTML($fields[STANDING_RAW_FRIENDLYNAME]);
		$out_fields[COLUMN_TOTAL_SOLVED] = $solved;
		$out_fields[COLUMN_TOTAL_TIME] = sprintf('%02d:%02d:%02d', $time / 3600, $time / 60 % 60, $time % 60);

		foreach my $attempts (@fields[STANDING_RAW_SOLVED..$#fields])
		{
			if ($attempts > 0)
			{
				my $base = ($attempts - 1);
				if ($base == 0) { $base = '+'; }
				else { $base = '+' . $base; }
				push @out_fields, span({-class => 'correct'}, $base);
			}
			elsif ($attempts < 0)
			{
				push @out_fields, span({-class => 'incorrect'}, sprintf("&minus;%d", -$attempts));
			}
			else
			{
				push @out_fields, '';
			}
		}

		my $class = ($place & 1 ? "odd" : "even");
		$replace{'BODY'} .= Tr({-class => $class}, td([@out_fields]));
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

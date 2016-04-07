package Bolo::Collector;

use strict;
use warnings;
use Time::HiRes qw/time/;
use LWP::UserAgent;
use HTTP::Headers;
use HTTP::Request;
use Getopt::Long qw/:config bundling/;
use YAML qw/LoadFile/;
use Data::Dumper;

use base qw/Exporter/;
our @EXPORT = qw/
	PLUGIN OPTION START
	DEBUG DUMP
	TRACE TDUMP
	FQDN PREFIX
	MARK TIMED
	STATE OK WARNING CRITICAL UNKNOWN
	SAMPLE COUNTER RATE
	KEY
	TRACK THRESHOLD
	BAIL
	STAGE TIMEOUT
	CREDENTIALS
	HTTP HTTP_AUTH
/;

our $VERSION = "0.9";

$ENV{PERL_LWP_SSL_VERIFY_HOSTNAME} = 0;

my %PLUGIN;
sub PLUGIN
{
	my ($name, %options) = @_;
	($options{name} = $name) =~ s!.*/!!;
	$PLUGIN{$_} = $options{$_} for keys %options;
}
PLUGIN($0);

my %OPTIONS      = ();
my @OPT_FLAGS    = ();
my %OPT_DEF      = ();
sub _usage
{
	return "-$2, --$1 <value>" if $_[0] =~ m/([^\|]+)\|(.)=[si]/;
	return  "    --$1 <value>" if $_[0] =~ m/([^\|]+)=[si]/;
	return "-$2, --$1"         if $_[0] =~ m/([^\|]+)\|(.)$/;
	return  "    --$1"         if $_[0] =~ m/([^\|=]+)/;
}
sub OPTION
{
	my ($spec, %options) = @_;
	return \%OPTIONS unless defined $spec;

	(my $primary = $spec) =~ s/[|=].*//;

	push @OPT_FLAGS, $spec;
	$OPTIONS{$primary} = $options{default} if $options{default};
	$OPT_DEF{$spec} = {
		primary  => $primary,
		usage    => $options{usage} || _usage($spec),
		help     => $options{help},
		default  => $options{default},
		required => $options{required},
	};
}
OPTION 'help|h|?',  help => 'Show this help screen';
OPTION 'debug|D+',  help => 'Enable debugging output';
OPTION 'noop',      help => 'Dry-run mode; no state files will be updated';
OPTION 'prefix=s',  help => 'Prefix for result submission';
OPTION 'timeout=i', help => 'Execution timeout, in seconds', default => 15;

sub START
{
	my $err = 0;
	GetOptions \%OPTIONS, @OPT_FLAGS
		or $err++;

	my $L = 0;
	for (values %OPT_DEF) { $L = length($_->{usage}) if length($_->{usage}) > $L; }
	$L += 4;

	if ($OPTIONS{help}) {
		print STDERR "USAGE: $PLUGIN{name}\n\n";
		if (defined $PLUGIN{summary}) {
			printf STDERR "   %s\n", $_ for split /\r?\n/, $PLUGIN{summary};
			print  STDERR "\n";
		}
		for (@OPT_FLAGS) {
			my $o = $OPT_DEF{$_};
			printf STDERR "   %-${L}s %s\n", $o->{usage}, $o->{help} || '';
			printf STDERR "   %-${L}s (defaults to %s)\n", "", $o->{default}
				if defined $o->{default};
		}

		exit 1;
	}

	for (values %OPT_DEF) {
		next if defined $OPTIONS{$_->{primary}};
		next unless $_->{required};

		print STDERR "Missing required --$_->{primary} parameter!\n";
		$err++;
	}

	exit 1 if $err;

	TRACE("plugin $PLUGIN{name} starting up");
	TIMEOUT($OPTIONS{timeout});

	$SIG{__DIE__} = sub {
		die @_ if $^S; # rethrow in eval { ... }
		(my $msg = join(' ', @_)) =~ s/[\r\n]+/ /g;
		$msg =~ s/\s+$//;
		BAIL(UNKNOWN => "Unhandled exception '%s'", $msg);
	};
}

sub DEBUG
{
	return unless $OPTIONS{debug};
	my @l = split /\r?\n/, join("\n", @_);
	printf STDERR "DEBUG> %s\n", shift @l;
	printf STDERR "       %s\n", $_ for @l;
	printf STDERR "\n";
}

sub DUMP
{
	return unless $OPTIONS{debug};
	$Data::Dumper::Pad = "DEBUG> ";
	print STDERR Dumper(@_);
}

sub TRACE
{
	return unless $OPTIONS{debug} and $OPTIONS{debug} > 2; # -DDD
	my @l = split /\r?\n/, join("\n", @_);
	printf STDERR "TRACE> %s\n", shift @l;
	printf STDERR "       %s\n", $_ for @l;
	printf STDERR "\n";
}

sub TDUMP
{
	return unless $OPTIONS{debug} and $OPTIONS{debug} > 2; # -DDD
	$Data::Dumper::Pad = "TRACE> ";
	print STDERR Dumper(@_);
}

sub FQDN
{
	chomp(my $fqdn = qx(/bin/hostname -f));
	$fqdn;
}

my $PREFIX;
sub _init_prefix
{
	return if $PREFIX;

	print STDERR "dbolo-prefix='$ENV{DBOLO_PREFIX}'\n";
	$PREFIX = $ENV{DBOLO_PREFIX};
	return if $PREFIX;

	(my $script = $0) =~ s{.*/}{};
	$PREFIX = FQDN.":$script";
}
sub PREFIX
{
	my ($v) = @_;
	_init_prefix;
	my $return = $PREFIX;
	($PREFIX = $v) =~ s/{{fqdn}}/FQDN/ge if defined $v;
	$return;
}

my $UA;
sub _ua
{
	if (!$UA) {
		$UA = LWP::UserAgent->new;
	}

	$UA;
}

my $MARK = time;
sub MARK
{
	my $now = time;
	my $elapsed = $now - $MARK;
	$MARK = time;
	$elapsed;
}

sub TIMED(&)
{
	MARK;
	$_[0]->();
	MARK;
}

sub STATE
{
	my ($status, $name, $fmt, @args) = @_;
	$name = $name ? PREFIX.":$name" : PREFIX;
	printf "STATE %i %s %s %s\n", int(time), $name, $status, sprintf($fmt || '', @args);
}

sub OK       { STATE OK       => @_ }
sub WARNING  { STATE WARNING  => @_ }
sub CRITICAL { STATE CRITICAL => @_ }
sub UNKNOWN  { STATE UNKNOWN  => @_ }

sub SAMPLE
{
	my ($name, @values) = @_;
	$name = $name ? PREFIX.":$name" : PREFIX;
	printf "SAMPLE %i %s", int(time), $name, join(' ', @values);
	printf " %0.5lf", $_ for @values;
	printf "\n";
}

sub COUNTER
{
	my ($name, $value) = @_;
	$value = 1 unless defined $value;
	$value >= 0           or warn "COUNTER given a negative value ($value)\n"   and return;
	int($value) == $value or warn "COUNTER given a fractional value ($value)\n" and return;

	$name = $name ? PREFIX.":$name" : PREFIX;
	printf "COUNTER %i %s %i\n", int(time), $name, $value;
}

sub RATE
{
	my ($name, $value) = @_;
	defined $value or warn "RATE not given a value\n" and return;

	$name = $name ? PREFIX.":$name" : PREFIX;
	printf "RATE %i %s %llu\n", int(time), $name, $value;
}

sub TRACK
{
	my ($type, @rest) = @_;
	$type eq 'SAMPLE'  and return SAMPLE  @rest;
	$type eq 'RATE'    and return RATE    @rest;
	$type eq 'COUNTER' and return COUNTER @rest;
	warn "Unknown data type given to TRACK: $type\n";
}

sub _inflate
{
	my ($range) = @_;
	if ($range =~ m/(\d+(?:\.\d+)):(\d+(?:\.\d+))/) {
		return { low => $1, high => $2 };
	} elsif ($range =~ m/(\d+(?:\.\d+)):/) {
		return { low => $1, high => undef };
	} elsif ($range =~ m/:(\d+(?:\.\d+))/) {
		return { low => undef, high => $1 };
	} elsif ($range =~ m/(\d+(?:\.\d+))/) {
		return { low => undef, high => $1 };
	} else {
		WARNING "invalid threshold range: $range";
		return { low => undef, high => undef };
	}
}

sub THRESHOLD
{
	my ($value, $msg, %options) = @_;
	if ($options{critical}) {
		my $range = _inflate($options{critical});
		if ($range->{low} && $value < $range->{low}) {
			$msg ||= "value, $value below critically low level, $range->{low}.";
			CRITICAL $msg;
			return;
		} elsif ($range->{high} && $value > $range->{high}) {
			$msg ||= "value, $value above critically high level, $range->{high}.";
			CRITICAL $msg;
			return;
		}
	}
	if ($options{warning}) {
		my $range = _inflate($options{warning});
		if ($range->{low} && $value < $range->{low}) {
			$msg ||= "value, $value below lower warning limit, $range->{low}.";
			WARNING $msg;
			return;
		} elsif ($range->{high} && $value > $range->{high}) {
			$msg ||= "value, $value above upper warning limit, $range->{high}.";
			WARNING $msg;
			return;
		}
	}
	if ( $options{skip_ok}) {
		return;
	}
	$msg ||= "value, $value does not exceed any thresholds.";
	OK $msg;
}

sub KEY
{
	my ($name, $value) = @_;
	$name = $name ? PREFIX.":$name" : PREFIX;
	printf "KEY %i %s %s\n", int(time), $name, $value;
}

sub BAIL
{
	my ($status, @msg) = @_;
	STATE($status, '', @msg) if defined $status;
	exit 0;
}

my $STAGE = 'running check';
sub STAGE
{
	my ($v) = @_;
	my $return = $STAGE;
	$STAGE = $v if defined $v;
	$return;
}

sub TIMEOUT
{
	my $s = $_[0] || 0;

	TRACE("setting global plugin timeout to $s seconds");
	alarm($s);
	$SIG{ALRM} = $s == 0 ? undef : sub {
		BAIL CRITICAL => "Timed out after %i seconds while %s", $s, $STAGE;
	};
}

sub CREDENTIALS
{
	my $file = "$ENV{HOME}/.creds";
	return () unless -f $file and -r $file;
	TRACE("Found credentials file:", $file);

	my $PW = LoadFile("$ENV{HOME}/.creds");
	for (@_) {
		next unless exists $PW->{$_};
		return ($PW->{$_}{username}, $PW->{$_}{password});
	}

	return ();
}

my @HTTP_AUTH;
sub HTTP_AUTH
{
	my ($type, $username, $password) = @_;
	@HTTP_AUTH = ($username, $password);
}
sub HTTP
{
	my ($method, $url, %options) = @_;

	TRACE("Making a $method request to $url");

	my $headers = HTTP::Headers->new;
	$headers->header('User-Agent' => __PACKAGE__."/$VERSION $0 Perl/$^V");

	my $content = undef;
	if ($options{body}) {
		$content = $options{body};
		delete $options{body};

		if (ref $content) {
			my $uri = URI->new('http:');
			$uri->query_form(ref($content) eq 'HASH' ? %$content : @$content);
			$content = $uri->query;
			$headers->header('Content-Type' => 'application/x-www-form-urlencoded');
		}

		$headers->header('Content-Length' => length($content));
	}

	if (keys %options) {
		for (keys %options) {
			$headers->header($_ => $options{$_});
		}
	}

	my $req = HTTP::Request->new(uc($method) => $url, $headers, $content);
	$req->authorization_basic(@HTTP_AUTH) if @HTTP_AUTH;

	TRACE("Making request:",
	      $req->as_string);

	my $res = _ua->request($req);

	TRACE("Received response from remote HTTP server",
	      "(to $method $url)",
	      "",
	      $res->as_string);
	if ($res->is_success()) {
		return wantarray ? ($res->decoded_content, $res)
		                 :  $res->decoded_content;
	} else {
		return wantarray ? (undef, $res)
		                 :  undef;
	}
}

1;

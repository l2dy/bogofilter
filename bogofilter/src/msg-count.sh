#! /bin/sh

# msg-count.sh
#
#	performs lexical analysis and database lookup
#	and converts an email to the msg-count format
#
# $Id$ 


# split file on stdin at "From " lines and pipe the parts into a command
# which is given in the arguments.
# WARNING: this does not escape any shell prompts or something
pipesplitmbox() {
      $AWK "BEGIN { cmd=\"$*\"; } /^From / { close(cmd); } { print | cmd; }"
}


if [ "x$1" = "x-?" ] || [ "x$1" = "x-h" ]; then
    cat <<EOT
NAME
  msg-count.sh - lexical analysis and database lookup for an email message
                 to convert it to the msg-count format	
SYNOPSIS
  msg-count.sh [-h | path/to/bogofilter/directory [bogolexer options]]

DESCRIPTION
  msg-count.sh creates a message digest consisting of a .MSG_COUNT line,
  followed by one line per token. The format is

    ".MSG_COUNT" 1234 2345
    "token" 234 345
    ...

  The .MSG_COUNT line gives the numbers of messages that make up the
  spam and nonspam wordlists, respectively; each token line gives the
  counts of the token in the spam and nonspam wordlists, respectively.
  The msg-count script is meant to be invoked by commands like:

    msg-count.sh db_dir <mboxfile >digestfile

    msg-count.sh db_dir msg_dir >digestfile

OPTIONS
  -h    The option -h displays help.

BOGOFILTER DIRECTORY
  The bogofilter directory is where the wordlists are kept. It defaults
  to ~/.bogofilter if no path is provided on the command line.  The
  path must be provided if further options are given; any further
  options are passed through to bogolexer.

SEE ALSO
  bogofilter(1), bogolexer(1), bogoutil(1).
EOT
    exit 0
fi

[ -z "$BOGOLEXER" ] && BOGOLEXER=`which bogolexer`
[ -z "$BOGOUTIL"  ] && BOGOUTIL=`which bogoutil`

if [ -d "$1" ]; then
    BOGOFILTER_DIR=$1
    shift
fi

[ -z "$BOGOFILTER_DIR" ] && BOGOFILTER_DIR="~/.bogofilter"

if [ -z "$BOGOLEXER" ] || [ -z "$BOGOUTIL" ] || [ -z "$BOGOFILTER_DIR" ];
then
    echo BOGOLEXER or BOGOUTIL or BOGOFILTER_DIR not set
    exit
fi

if [ "$1" = "formail" ] ; then
    shift
    ( echo .MSG_COUNT ; $BOGOLEXER -p "$@" | sort -u ) | \
	$BOGOUTIL -w $BOGOFILTER_DIR | \
	awk 'NF == 3 { printf( "\"%s\" %s %s\n", $1, $2, $3 ) } '
elif [ ! -d "$1" ] ; then
    formail -es "$0" formail "$@"
else
    DIR="$1"
    shift
    for f in $DIR/* ; do
	( echo .MSG_COUNT ; $BOGOLEXER -p "$@" < $f | sort -u ) | \
	    $BOGOUTIL -w $BOGOFILTER_DIR | \
	    awk 'NF == 3 { printf( "\"%s\" %s %s\n", $1, $2, $3 ) } '
    done
fi

#!/bin/sh

[ -z "$BOGOLEXER" ] && BOGOLEXER=`which bogolexer`
[ -z "$BOGOUTIL" ] && BOGOUTIL=`which bogoutil`

if [ -z "$BOGOLEXER" ] || [ -z "$BOGOUTIL" ] || [ -z "$BOGOFILTER_DIR" ];
then
    echo BOGOLEXER or BOGOUTIL or BOGOFILTER_DIR not set
else
( echo .MSG_COUNT ; $BOGOLEXER -p $* | sort -u ) | \
    $BOGOUTIL -w $BOGOFILTER_DIR | \
    awk 'NF == 3 { printf( "\"%s\" %s %s\n", $1, $2, $3 ) } '
fi

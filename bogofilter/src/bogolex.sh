( echo .MSG_COUNT ; bogolexer -p $* | sort -u ) | \
    bogoutil -w $BOGOFILTER_DIR | \
    awk 'NF == 3 { printf( "\"%s\" %s %s\n", $1, $2, $3 ) } '

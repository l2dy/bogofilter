#! /bin/sh

set -e

# Note:  When run via "make check", test output files are automatically deleted.
#	 When run from the command line, outputs files are left in directory
#	 robx.YYYYMMDD.  This is useful when something is different.
#
#	 ./inputs  - test inputs
#	 ./outputs - known correct outputs
#
#	 robx.YYYYMMDD:
#		tests   - directory containing individual output files

: ${srcdir=.}
relpath="`pwd`/../.."
NODB=1 . ${srcdir}/../t.frame

: ${AWK=awk}

verbose=0
if [ "$1" = "-v" ]; then
    verbose=1
fi

[ -d ${TMPDIR}/tests ] && rm -rf ${TMPDIR}/tests
mkdir -p ${TMPDIR}/tests

CONFIG="${TMPDIR}/test.cf"
BOGOFILTER="$VAL ${relpath}/bogofilter -c ${CONFIG} -y 0"
BOGOUTIL="$VAL ${relpath}/bogoutil"
SYSTEST="${srcdir}"

cat <<EOF > ${CONFIG}
robx=0.415
min_dev=0.0
spam_header_name=X-Bogosity
stats_in_header=Y
EOF

$BOGOFILTER -r -s < ${SYSTEST}/inputs/spam.mbx
$BOGOFILTER -r -n < ${SYSTEST}/inputs/good.mbx

if [ ! -z "$RUN_FROM_MAKE" ] ; then
    $BOGOUTIL -R $TMPDIR
else
    $BOGOUTIL -d $TMPDIR/wordlist.${DB_EXT} > ${TMPDIR}/wordlist.txt
    $BOGOUTIL -vvv -R $TMPDIR > ${TMPDIR}/output.vvv
fi

[ ! -z "$BF_SAVEDIR" ] && . ${srcdir}/../t.save

bogoutil -w $TMPDIR .ROBX
bogoutil -w $TMPDIR .ROBX | awk '{print $2}'
bogoutil -w $TMPDIR .ROBX | awk '/.ROBX/ { print $2; }'
echo AWK: $awk
$BOGOUTIL -w $TMPDIR .ROBX | $AWK '/.ROBX/ { print $2; }'

RESULT=`$BOGOUTIL -w $TMPDIR .ROBX | $AWK '/.ROBX/ { print $2; }'`
echo RESULT: $RESULT

WANT="252685"

if  [ $verbose -ne 0 ]; then
    echo want: $WANT, have: $RESULT
fi

test $RESULT = "$WANT"

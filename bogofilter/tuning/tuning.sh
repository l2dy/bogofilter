#! /bin/bash

#  smindev.sh 
#
#    a script for studying the effects of varying s and mindev

# mailboxes for testing:

[ -z "$SPAM_FILES" ] && SPAM_FILES="spam.mbx"
[ -z "$GOOD_FILES" ] && GOOD_FILES="good.mbx"

# range of values for testing

svals="1 3.2e-1 1e-1 3.2e-2 1e-2"
mvals=`seq 0.025 0.025 0.45001`		# wide value range 
#mvals=`seq 0.420 0.020 0.46001` 	# high value range

# approx 0.1%-0.3% of nonspam corpus size for selecting spam_cutoff value

target=12

# file names

DATE=`date "+%m%d.%H%M"`

CFG="tuning.cf"
PARM_TBL="parms.$DATE.tbl"
RESULTS="results.$DATE.txt"

########## Code Begins Here ##########

# validate program paths

error="n"
for prog in bogofilter bogolexer bogoutil bogolex.sh ; do
    path=`which $prog`
    if [ -z "$path" ] ; then
	echo "Can't find $prog"
	error="y"
    fi
done

if [ "$error" = "y" ] ; then
    exit
fi

BOGOFILTER_DIR="."

export BOGOFILTER BOGOLEXER BOGOUTIL BOGOFILTER_DIR
export CFG GOOD_FILES SPAM_FILES

cat <<EOF >$CFG
algorithm=fisher
strict_check=no
ignore_case=no
block_on_subnets=no
header_line_markup=yes
tokenize_html_tags=yes
terse_format=%1.1c %8.2d
spamicity_tags=1, 0, ?
spamicity_formats=%6.2e, %6.2e, %0.6f 
EOF

if [ ! -f $BOGOFILTER_DIR/goodlist.db -o ! -f $BOGOFILTER_DIR/spamlist.db ] ; then
    echo Distributing messages and making dir $BOGOFILTER_DIR ...
    [ -d $BOGOFILTER_DIR ] || mkdir $BOGOFILTER_DIR
    mkdb 
    echo Done - distributing messages and making $BOGOFILTER_DIR.
fi

function getco () {
    opts="-m$1 -o$2"; tgt=$3
    shift ; shift ; shift
    res=`cat $* | bogofilter -t -c $CFG $opts -v 2>&1 | \
    perl -e ' $target = shift @ARGV; while (<>) { ' \
	 -e ' ($i, $d) = split; push @diffs, $d unless $i != 1; }' \
	 -e ' die "dainbramage" unless scalar @diffs > 15;' \
         -e ' @s = sort { $a <=> $b } (@diffs); $co = $s[$target];' \
	 -e ' while($co < 0.000001) { ++$target; $co = $s[$target]; }' \
	 -e ' printf("%8.6f %d",1.0-$s[$target],$target);' $tgt`
}

function wrapper () {
    v="-v"
    opts="-m$1 -o$2"
    shift ; shift
    res=`cat $1 | bogofilter -t -c $CFG $opts -v | grep -c $v '^1'`
}

function doit () {
    rs=$1 ; shift
    md=$1 ; shift
    date=`date "+%m/%d %H:%M:%S"`
    echo -n $date "  "
    printf "%-7s %5.3f fpos..." $rs $md
    getco $md,$rs 0.10 $target r0.ns.mc r1.ns.mc r2.ns.mc
    fpos=${res##* }; co=${res%% *}; 
    printf "%d at cutoff %8.6f, run0..." $fpos $co
    let fpos3=$fpos/3
    run=0; wrapper $md,$rs $co r0.sp.mc; fneg1=$res
    echo "$rs $md $co $run $fpos3 $fneg1" >> $PARM_TBL
    printf "%3d  run1..." $fneg1
    run=1; wrapper $md,$rs $co r1.sp.mc; fneg2=$res
    echo "$rs $md $co $run $fpos3 $fneg2" >> $PARM_TBL
    printf "%3d  run2..." $fneg2
    run=2; wrapper $md,$rs $co r2.sp.mc; fneg3=$res
    echo "$rs $md $co $run $fpos3 $fneg3" >> $PARM_TBL
    printf "%3d"  $fneg3
    let fneg="$fneg1+$fneg2+$fneg3"
    printf " %4d\n"  $fneg
    printf "%s %s %-6s %5.3f fpos...%d at cutoff %8.6f, run0...%3d  run1...%3d  run2...%3d %4d\n" \
	$date $rs $md  $fpos $co $fneg1 $fneg2 $fneg3 $fneg >> $RESULTS
}

./sizes | tee $RESULTS
./sizes | grep mc > $PARM_TBL

for rs in $svals; do
  echo "" | tee -a $RESULTS
  bogofilter -t -c $CFG -m,$rs -Q | grep 00 | grep -v off | tee -a $RESULTS
  for md in $mvals; do
      doit $rs $md
  done
done

# get 10 best results (lowest false negative count)
(echo "" ; \
echo "Top 10 Results:" ; \
echo " robs   min_dev spam_cutoff  run0 run1 run2 total" ; \
cat $RESULTS | grep fpos...$target | grep -v cutoff.0.415000 | sed "s@\.\.\.@.. @g" | \
awk '{printf "%6.4f %8.3f %10.6f  %4d %4d %4d %5d\n", $3, $4, $9, $11, $13, $15, $16 }' | \
sort -g --key=7 | head -10 ) | tee -a $RESULTS

date "+%m/%d %H:%M:%S"

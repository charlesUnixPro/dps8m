#!/bin/sh
#set -x

Version="1.0.1 (HWR was here)"
verbose=0

while [ $# -ge 1 ]
do
    case $1 in
        -h  | --help     ) echo help; exit ;;
        -v  | --version  ) echo $Version ; exit ;;
        -V  | --Verbose  ) verbose=1 ; echo Verbose mode;;
        -RR | --RegenerateAll) regen_all=1 ;;
        -d  | --DontRemove) dont_remove=1;;
        -r              ) run=$2; shift ;;
        -r*             ) run=${1#-r};;
        -e              ) EPOCH=$2; shift ; echo EPOCH="$EPOCH";;
        -e*             ) EPOCH=${1#-e}; echo EPOCH="$EPOCH";;
#        --epoch=        ) EPOCH=${1#*=}; echo EPOCH="$EPOCH";;
#        -o              ) filename=$2; shift ;;
#        -o*             ) filename=${1#-o} ;;
#        --output=       ) filename=${1#*=} ;;
#        -s              ) searchphrase=$2; shift ;;
#        -s*             ) ${1#-s} ;;
#        --search=       ) searchphrase=${1#*=} ;;
#        -f              ) format=$2; shift ;;
#        -f*             ) format=${1#-f} ;;
#        --format=       ) format${1#*=} ;;
        -*              ) echo invalid option >&2; exit 1 ;;
        *               ) echo invalid option >&2; exit 1 ;;
    esac
    shift
done

oldIFS=$IFS
IFS='
'
set -f
set -- $args
IFS=$oldIFS
set +f


#BASE=~/Documents/dps8m\ \(New\)/XCode/DerivedData/Build/Products/Debug
BASE=../../src/

DPS8=${BASE}/dps8/dps8
AS8=${BASE}/as8+/as8+

SRC="TestAppendA TestCSR TestAppend TestString TestFP TestEIS TestFXE TestAddrMods TestIndirect TestMpy TestBugs TestConsole test_20184.ini"


#EPOCH=$(date -j -f "%a %b %d %T %Z %Y" "`date`" "+%s")
EPOCH=$(date +%s)


#
# See if user wants to regenate all of the regression outputs
#
if [ "$regen_all" ]
then
    read -p "Are you sure you want to regenerate *ALL* regression outputs (y/n)? "
    if [ "$REPLY" == "y" ]
    then
        echo "Will regenerate *ALL* regression outputs"
        for VARIABLE in $SRC
        do
            TEST="$VARIABLE".out

            echo Regenerating "$TEST" ...
            "$DPS8" "$VARIABLE" > $TEST

        done
        exit
    else
        echo Aborting ...
        exit
    fi
fi

if [ "$verbose" -eq 1 ]
then
    echo Base directory = "$BASE"
    echo as8 = "$AS8"
    echo dps8 = "$DPS8"
    echo Regenerate = "$regen"
fi

if [ $run ]
then
#   echo Will run "$run"

    echo Epoch is $EPOCH

    VARIABLE="$run"

    echo  Running "$VARIABLE" ...

    TEST="$VARIABLE"."$EPOCH"

#    $AS8 -Isrc src/"$VARIABLE".as8 -o"$VARIABLE".o8
    "$DPS8" "$VARIABLE" | ./tidy > $TEST
    ./tidy <  "$VARIABLE".out >  "$VARIABLE".out.tidy
    xxdiff -D -w $TEST "$VARIABLE".out.tidy
    [ -z "$dont_remove" ] && rm $TEST
    if [ "$dont_remove" ]
    then
        echo File: "$TEST" not removed.
    fi

    exit 0
fi


echo Epoch is $EPOCH

for VARIABLE in $SRC
do
	echo  Running "$VARIABLE" ...

	TEST="$VARIABLE"."$EPOCH"

#    $AS8 -Isrc src/"$VARIABLE".as8 -o"$VARIABLE".o8
	"$DPS8" "$VARIABLE" | ./tidy > $TEST
        ./tidy <  "$VARIABLE".out >  "$VARIABLE".out.tidy
	xxdiff -D -w $TEST "$VARIABLE".out.tidy
    [ -z "$dont_remove" ] && rm $TEST
    if [ "$dont_remove" ]
    then
        echo File: "$TEST" not removed.
    fi

done



#!/bin/bash

FILE="${1}"
SCHEDULER="${2}"
STEP="${3}"
MIN="${4}"
MAX="${5}"

if [ -z "${SCHEDULER}" ] || [ -z "${FILE}" ]; then
	echo "Usage: threads_even.sh <file> <scheduler> [step=even|power] [min=1] [max=32]"
	exit 1
fi

if [ -z "${MIN}" ]; then
	MIN=1
fi
if [ -z "${MAX}" ]; then
	MAX=32
fi
if [ -z "${STEP}" ]; then
   STEP=even
fi
if [ ! -r "${FILE}" ]; then
   echo "Code file $FILE does not exist."
   exit 1
fi

RUN="bash ./threads.sh"

do_run ()
{
	NUM_THREADS="${1}"
	$RUN "${SCHEDULER}${NUM_THREADS}" ${FILE}
}

if [ "$STEP" = "even" ]; then
   for x in 1 2 4 6 8 10 12 14 16 20 24 28 32; do
      if [ $MIN -le $x ]; then
         if [ $MAX -ge $x ]; then
            do_run $x
         fi
      fi
   done
elif [ "$STEP" = "power" ]; then
   for x in 1 2 4 8 16; do
      if [ $MIN -le $x ]; then
         if [ $MAX -ge $x ]; then
            do_run $x
         fi
      fi
   done
else
   echo "Unrecognized step $STEP"
   exit 1
fi


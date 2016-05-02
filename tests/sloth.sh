#!/bin/sh

# A program to slowly cat file or standard input.

if [ "$1" ] ; then
file="$1"
else
file="-"
fi

cat "$file" | while read line ; do
echo "$line"
sleep 0.1
done

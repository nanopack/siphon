#!/usr/bin/env bash

input="$(cat <<-END
one
two
three
END
)"

expected="$(cat <<-END
+> one
+> two
+> three
END
)"

output=$(echo "$input" | src/siphon --prefix '+> ')

echo "Testing siphon"
echo
echo "Input:"
echo "$input"
echo
echo "Expecting:"
echo "$expected"
echo
echo "Received:"
echo "$output"
echo

failed="false"

if [[ "$output" != "$expected" ]]; then
  echo "FAIL"
  failed="true"
else
  echo "SUCCESS"
fi



for i in test1 test2
do
  src/siphon --prefix "=> " -- tests/sloth.sh tests/${i}.in | tee /tmp/${i}.out
  if [ "$(md5sum tests/${i}.out | awk -e '{print $1}')" = "$(md5sum /tmp/${i}.out | awk -e '{print $1}')" ]; then
  	echo "SUCCESS"
  else
  	failed="true"
  	echo "FAIL"
  fi
  rm /tmp/${i}.out
done

if [ "$failed" = "true" ]; then
  exit 1
fi

exit 0

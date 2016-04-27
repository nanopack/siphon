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

if [[ "$output" != "$expected" ]]; then
  echo "FAIL"
  exit 1
else
  echo "SUCCESS"
  exit 0
fi

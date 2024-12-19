#!/bin/bash

RETRIES=10

for i in $(seq 1 "$RETRIES"); do
	"$@" && break
	if [ "$i" == "$RETRIES" ]; then
		echo "Command \"$@\" failed after ${RETRIES} retries."
		exit 1
	fi
done

exit 0

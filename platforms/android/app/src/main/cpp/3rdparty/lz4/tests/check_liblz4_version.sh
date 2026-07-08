#!/usr/bin/env sh
set -e

# written as a script shell, because pipe management in python is horrible
ldd $1 | grep liblz4


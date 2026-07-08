#!/bin/sh

set -e
set -x

datagen -g4200MB | lz4 -v3 | lz4 -qt

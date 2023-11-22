#!/bin/sh -x

CLANG_CHECKER_NAME=checker-278

cd ~

if [ ! -d ~/$CLANG_CHECKER_NAME ]
then
  curl http://clang-analyzer.llvm.org/downloads/$CLANG_CHECKER_NAME.tar.bz2 -o ~/$CLANG_CHECKER_NAME.tar.bz2
  tar -xf ~/$CLANG_CHECKER_NAME.tar.bz2
fi

export SCAN_BUILD_PATH=~/$CLANG_CHECKER_NAME/bin/scan-build

cd -

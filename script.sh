#!/bin/bash


if [[ -x /usr/bin/curl ]]; then
  get=curl
else
  echo need curl 1>&2
  exit 1
fi 


make=$(find . -type f -name 'Makefile' -print)
if [[ -z $make ]]; then
  echo no makefile found 1>&2
  exit 1
fi

dir=$(dirname $make)
if [[ -z $dir || ! -d $dir ]]; then
  echo no directory found 1>&2
  exit 1
fi

cd $dir || exit 1
make all
# ampersand just in case they forgot to background it
make run &

testcase=1

while read url auth; do
   echo testcase $testcase is $url $auth 1>&3
   [[ -n $auth ]] && auth="-u $auth"
   curl -o test-$testcase --stderr error-$testcase -D header-$testcase --trace-ascii trace-$testcase $auth $url
   let testcase=testcase+1
done 3> testcases-run < testcases

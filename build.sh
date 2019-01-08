#!/bin/bash
if [ ! -d build ]; then
    mkdir build
fi
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" ..
RESULT=$?
if [[ $RESULT != 0 ]]; then
	printf "\nCMake error. exiting.\n"
	exit 1
fi
make -j4
RESULT=$?
cd ..

if [[ $RESULT = 0 ]]; then
	cp build/Omega-Y* ./
	printf "\n Success.\n\n"
	exit 0
else
	printf "\n Errors encountered. \n\n"
	exit 1
fi
#!/bin/bash
# Prerequisites
# Install lcov (f.e. for Ubuntu use "sudo apt-get install lcov")

exit_on_error()
{
    arg1=$1

    echo "Failed to execute with error "$arg1". Exiting..."
    exit $arg1
}

# Compile the program with coverage options (i.e. debug)
{
    cd ..
    mkdir build
    cd build
    cmake -D CMAKE_BUILD_TYPE=Debug ..
    make
} &> /dev/null
rc=$?; if [[ $rc != 0 ]]; then exit_on_error $rc; fi

# Execute the program and try to cover as much as possible
# Ignore return codes, this is not a component test but purely for coverage
{
    ./vs-ftp 127.0.0.1 2021 /tmp &
    lftp -p2021 127.0.0.1 -e "nlist;bye"
    #TODO: add more lftp commands and/or wget transfers
    killall vs-ftp
} &> /dev/null

# Output the coverage result
{
    gcov main.c
} &> /dev/null
rc=$?; if [[ $rc != 0 ]]; then exit_on_error $rc; fi

# Create gcov files
{
    gcov -abcfu main.c
} &> /dev/null
rc=$?; if [[ $rc != 0 ]]; then exit_on_error $rc; fi

# Generate lcov data info
{
    lcov --rc lcov_branch_coverage=1 --directory . --capture --output-file coverage.info
} &> /dev/null
rc=$?; if [[ $rc != 0 ]]; then exit_on_error $rc; fi

# Generate html representation
genhtml --branch-coverage coverage.info
rc=$?; if [[ $rc != 0 ]]; then exit_on_error $rc; fi

{
    sensible-browser index.html &
} &> /dev/null
rc=$?; if [[ $rc != 0 ]]; then exit_on_error $rc; fi

echo "Script completed."

exit 0

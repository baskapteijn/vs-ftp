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
    # Start the server
    ./vs-ftp 127.0.0.1 2021 /tmp &

    # Perform an `NLIST` (and `FEAT`, `USER` and `QUIT` command)
    lftp -p2021 127.0.0.1 -e "nlist;bye"

    # Perform an `NLIST .` command
    lftp -p2021 127.0.0.1 -e "nlist .;bye"

    # Perform an `NLIST ..` command (which should fail because it's above the root path)
    lftp -p2021 127.0.0.1 -e "nlist ..;bye"

    # Perform a `CWD ..` command (which should fail because it's above the root path)
    lftp -p2021 127.0.0.1 -e "cd ..;bye"

    # Perform a `CWD ..` command (which should fail because it's above the root path)
    lftp -u anon,@ -p2021 127.0.0.1 -e "nlist;bye"

    # Create and retrieve an existing file
    dd if=/dev/urandom of=/tmp/file.bin bs=1024 count=1024
    wget ftp://127.0.0.1:2021//tmp/file.bin

    # Try to retrieve a non-existing file
    wget ftp://127.0.0.1:2021//tmp/not_existing_file.bin

    #TODO: add more lftp commands and/or wget transfers

    # Kill all and any vs-ftp process by sending SIGTERM (which is handled properly)
    killall vs-ftp
} # &> /dev/null

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

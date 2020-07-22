#!/bin/bash
# Prerequisites
# Install lcov (f.e. for Ubuntu use "sudo apt-get install lcov")

exit_on_error()
{
    arg1=$1

    echo "Failed to execute with error "$arg1". Exiting..."
    exit $arg1
}

# Print some package versions
{
    cmake --version
    gcc -v
    lftp -v
    lcov --version
    # unclear how to get the version of 'ftp'
}

# Compile the program with coverage options (i.e. debug)
{
    cd ..
    mkdir build
    cd build
    cmake -D CMAKE_BUILD_TYPE=Debug ..
    make clean
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

    # Perform an `NLIST somedir` command (which should fail because it's a non-existing dir)
    lftp -p2021 127.0.0.1 -e "nlist somedir;bye"

    # Perform an `CWD somedir` command (which should fail because it's a non-existing dir)
    lftp -p2021 127.0.0.1 -e "cd somedir;bye"

    # Perform an `CWD .` command
    mkdir /tmp/anotherdir
    lftp -p2021 127.0.0.1 -e "cd anotherdir;bye"

    # Perform a `CWD ..` command (which should fail because it's above the root path)
pftp 127.0.0.1 2021 <<HERE
anonymous
cd ..
quit
HERE

    # Perform a login with anon (too short, which should fail because it should be anonymous)
    lftp -u anon,@ -p2021 127.0.0.1 -e "nlist .;bye"

    # Perform a login with anonymousu (too long, which should fail because it should be anonymous)
    lftp -u anonymousu,@ -p2021 127.0.0.1 -e "nlist .;bye"

    # Perform a login with anonymouu (equal length but wrong chars, which should fail because it should be anonymous)
    lftp -u anonymouu,@ -p2021 127.0.0.1 -e "nlist .;bye"

    # Create a binary file and retrieve it (binary mode)
    dd if=/dev/urandom of=/tmp/file.bin bs=1024 count=1024
    wget ftp://127.0.0.1:2021//file.bin

    # Try to retrieve a non-existing file
    wget ftp://127.0.0.1:2021//not_existing_file.bin

    # Try to retrieve a directory
    lftp -p2021 127.0.0.1 -e "get .;bye"

    # Get the remote HELP, ascii mode and binary mode through pftp with a 'here-document', lftp cannot do it.
    # Note that each line in the here-document cannot be preceded by a space (tabs do not seem to work either).
    # Also not we are trying to retrieve a file in ASCII mode here
    echo text > /tmp/text_new.txt
pftp 127.0.0.1 2021 <<HERE
anonymous
rhelp
ascii
binary
ascii
get text_new.txt
quit
HERE

    # Do some invalid argument tests, don't start a background process because they should terminate automatically

    # Kill all and any vs-ftp process by sending SIGTERM (which is handled properly)
    killall vs-ftp

    # Start the server with a hostname (not supported)
    ./vs-ftp localhost 2021 /tmp

    # Start the server with an invalid (might be relative) path (not supported)
    ./vs-ftp 127.0.0.1 2021 tmp

    # Start the server with an invalid (non numeric) port (not supported)
    ./vs-ftp 127.0.0.1 abcd /tmp

    # Start the server with another non numeric port (not supported)
    ./vs-ftp 127.0.0.1 / /tmp

    # Start the server with a max port - 1
    ./vs-ftp 127.0.0.1 65364 /tmp &
    killall vs-ftp

    # Start the server with a max port + 1 (not supported)
    ./vs-ftp 127.0.0.1 65366 /tmp

    # Start the server with a max length +1 port (not supported)
    ./vs-ftp 127.0.0.1 653656 /tmp

    # Start the server with an invalid number of arguments
    ./vs-ftp 127.0.0.1 abcd
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

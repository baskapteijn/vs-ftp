#!/bin/bash
# Prerequisites
# Install doxygen (f.e. for Ubuntu use "sudo apt-get install doxygen")
# Install doxywizard (f.e. for Ubuntu use "sudo apt-get install doxygen-gui")

exit_on_error()
{
    arg1=$1

    echo "Failed to execute with error "$arg1". Exiting..."
    exit $arg1
}

# Generate doxygen documentation
{
    doxygen Doxyfile
} &> /dev/null
rc=$?; if [[ $rc != 0 ]]; then exit_on_error $rc; fi

# Open the default browser
{
    sensible-browser html/index.html &
} &> /dev/null
rc=$?; if [[ $rc != 0 ]]; then exit_on_error $rc; fi

echo "Script completed."

exit 0

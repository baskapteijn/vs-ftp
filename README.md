# vs-ftp
[![Build Status](https://travis-ci.org/baskapteijn/vs-ftp.svg?branch=master)](https://travis-ci.org/baskapteijn/vs-ftp)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/baskapteijn/vs-ftp.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/baskapteijn/vs-ftp/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/baskapteijn/vs-ftp.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/baskapteijn/vs-ftp/context:cpp)
[![codecov](https://codecov.io/gh/baskapteijn/vs-ftp/branch/master/graph/badge.svg)](https://codecov.io/gh/baskapteijn/vs-ftp)  
A Very Small FTP Server focused on simplicity and portability.

## Project mission statement
This project is intended to produce a very small, simple and portable FTP server that can be used on multiple (embedded)
platforms with minimal changes.  
The current focus is on Linux, with the intention to also make the code compatible for Arduino and optionally windows.  
The FTP server will be read-only and shall only support 1 anonymous user at a time.  
Clients used to verify correct operation are the Ubuntu supported tools 'ftp', 'lftp' and 'wget'.

## Prerequisites

This readme assumes a Linux based host machine.

### For building

* [CMake](https://cmake.org/) 3.7.2 or higher
* [GNU](https://gcc.gnu.org/) 6.3.0 C compiler or compatible

### For coverage

* [LCOV](http://ltp.sourceforge.net/coverage/lcov.php) 1.13 or compatible

## Building

From the build directory:
```bash
$ cd build
$ cmake -D CMAKE_BUILD_TYPE=Release ..
-- The C compiler identification is GNU 7.5.0
-- Check for working C compiler: /usr/bin/cc
-- Check for working C compiler: /usr/bin/cc -- works
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Detecting C compile features
-- Detecting C compile features - done
-- Configuring done
-- Generating done
-- Build files have been written to: /src/vs-ftp/build
$ make
[ 16%] Building C object CMakeFiles/vs-ftp.dir/src/linux/main.c.o
[ 33%] Building C object CMakeFiles/vs-ftp.dir/src/linux/io.c.o
[ 50%] Building C object CMakeFiles/vs-ftp.dir/src/common/vsftp_server.c.o
[ 66%] Building C object CMakeFiles/vs-ftp.dir/src/common/vsftp_commands.c.o
[ 83%] Building C object CMakeFiles/vs-ftp.dir/src/common/vsftp_filesystem.c.o
[100%] Linking C executable vs-ftp
[100%] Built target vs-ftp
```
To switch to a Debug build you can specify the following option in the CMake command line:
```bash
$ cmake -D CMAKE_BUILD_TYPE=Debug ..
$ make
```

## Coverage

Coverage generation with the bash script has only been tested on a Linux based host machine.  
Make sure that the coverage.sh has execute permission.

The coverage script will run a predefined set of tests to get as much coverage as possible.

From the coverage directory:

```bash
$ ./coverage.sh
...
Reading data file coverage.info
Found 6 entries.
Found common filename prefix "/src/vs-ftp/src"
Writing .css and .png files.
Generating output.
Processing file common/vsftp_server.c
Processing file common/vsftp_commands.c
Processing file common/vsftp_filesystem.c
Processing file linux/main.c
Processing file linux/io.c
Processing file linux/version.h
Writing directory view page.
Overall coverage rate:
  lines......: 92.3% (721 of 781 lines)
  functions..: 100.0% (55 of 55 functions)
  branches...: 63.9% (344 of 538 branches)
Script completed.
```
The default browser will automatically display the results.

## Usage

### Start an FTP server

```bash
$ ./vs-ftp 127.0.0.1 2021 /tmp
2020-07-22_18:47:37 vsftp_server.c[307]: Initializing server
2020-07-22_18:47:37 vsftp_server.c[339]: Starting server
2020-07-22_18:47:37 vsftp_server.c[124]: Socket 4 on port 2021 ready for connection...
```

A few notes on the programs arguments:

* The IP address must not be a hostname and must be the external IP address (ISP address) when it has to be accessible
  from the outside world.
* The port must be given, no default is assumed. For security reasons its advised to use a non-default, higher range
  port.
* The root path has to be an absolute path on the system. The path is allowed to contain symbolic links, since they will
  be resolved within the program.  

### Help menu

Any invalid input will cause an error to be printed including the help menu.

```bash
$ ./vs-ftp
    Invalid number of arguments
    
    Version 0.1.0
    
    Usage:
      vs-ftp <server ip> <port> <root path>
```

## Original project mission statement
This project is intended to produce a very small, simple and portable FTP server that can be used on multiple (embedded)
platforms with minimal changes.

This is how it is going to be accomplished:

* The server is going to be written in plain C (C99)
* Using only standard C libraries (optionally using POSIX if there is no other possibility for platform-independence)
* No (non-standard) build macro's or configurations
* Thread-less (polling) design to allow for bare-metal execution
* Static memory allocation only (no malloc/free)
* Minimum FTP implementation as declared in [RFC959 chapter 5.1](https://www.w3.org/Protocols/rfc959/5_Declarative.html)
* Transmission modes have to be decided upon [RFC959 chapter 3.4](https://www.w3.org/Protocols/rfc959/3_DataTransfer.html)
* Supported file operations have to be decided upon
* Single user (anonymous) conforming to [RFC1635](https://tools.ietf.org/html/rfc1635)
* Using function pointers and/or hooks for functionality that requires platform specific implementation
* Fully documented by means of Doxygen annotation


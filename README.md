# vs-ftp
A Very Small FTP Server focused on simplicity and portability.

## Project mission statement
This project is intended to produce a very small, simple and portable FTP server that can be used on multiple (embedded) platforms with minimal changes.

This is how it is going to be accomplished:

* The server is going to be written in plain C (C99)
* Intended to be used (and delivered) as a library
* Using only standard C libraries (optionally using POSIX if there is no other possibility for platform-independence)
* No (non-standard) build macro's or configurations
* Thread-less design to allow for bare-metal execution
* Static memory allocation only (no malloc/free)
* Minimum FTP implementation as declared in [RFC959 chapter 5.1](https://www.w3.org/Protocols/rfc959/5_Declarative.html)
* Single user (anonymous) conforming to [RFC1635](https://tools.ietf.org/html/rfc1635)
* Using function pointers and/or hooks for functionality that requires platform specific implementation
* Fully documented by means of Doxygen annotation


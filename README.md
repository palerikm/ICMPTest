# ICMPTest

This code provides an example on how ICMP can be used to perform an "in-band" traceroute of the media path without requiring special privileges. The code should work on OS-X, IOS and Linux. Test results from other platforms are most welcome.

The usual DISCLAIMER applies, code here is meant as an example to prove a point, not as an efficient implementation of sockets or error handling. 

Compiling should be as easy as ./bootstrap to create the ./configure script. Then do a ./configure and then make. Binary will be in src/icmptest. Run as src/icmptest <interface> <dst_ip> 
SATANIC — high-performance network stress testing tool.

Features:
- Raw TCP SYN flood with spoofed IPs
- TCP connection flood
- UDP flood
- Multi-threading (up to 5000 threads)
- Real-time statistics (PPS)

Requirements:
- Linux
- G++ (C++)
- Root

Build:

g++ -O3 -std=c++17 -pthread -o satanic satanic.cpp

Usage:

sudo ./satanic <target_ip> <port>

Example:

sudo ./satanic 192.168.1.100 80 500

Completing the process - CTRL+C.

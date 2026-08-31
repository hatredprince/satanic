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

Examples:
sudo ./satanic 192.168.1.100 80 500;
sudo ./satanic 93.184.216.34 443 2000;
sudo ./satanic 10.0.0.1 8080

Completing the process - CTRL+C.

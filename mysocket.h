// mysocket.h
#ifndef MYSOCKET_H
#define MYSOCKET_H

#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include "protocol.h"

class mySocket {
protected:
    int _sockfd;
    bool _isValid; // 标记 Socket 是否有效

    // 循环写入 (处理 TCP 发送缓冲)
    bool writen(const char* buffer, size_t n) const {
        size_t left = n;
        const char* ptr = buffer;
        while (left > 0) {
            ssize_t written = ::send(_sockfd, ptr, left, MSG_NOSIGNAL);
            if (written <= 0) return false;
            left -= written;
            ptr += written;
        }
        return true;
    }

    // 循环读取 (处理 TCP 粘包/拆包)
    bool readn(char* buffer, size_t n) {
        size_t left = n;
        char* ptr = buffer;
        while (left > 0) {
            ssize_t read = ::recv(_sockfd, ptr, left, 0);
            if (read <= 0) return false; // 0=对方关闭, -1=错误
            left -= read;
            ptr += read;
        }
        return true;
    }

public:
    // --- Constructors ---
    // 接管一个现有的 socket fd
    mySocket(int fd) : _sockfd(fd), _isValid(fd >= 0) {}

    // 禁止拷贝 (防止两个对象析构时关闭同一个 socket)
    mySocket(const mySocket&) = delete;
    mySocket& operator=(const mySocket&) = delete;

    // 允许移动构造 (将 socket 所有权转移给新对象)
    mySocket(mySocket&& other) noexcept {
        _sockfd = other._sockfd;
        _isValid = other._isValid;
        // 让源对象失效
        other._sockfd = -1; 
        other._isValid = false;
    }

    // --- Destructor ---
    // 析构时自动完成下线告知、关闭 socket
    ~mySocket() {
        if (_isValid && _sockfd >= 0) {
            send(Packet(MSG_EXIT, "")); 
            std::cout << "Socket " << _sockfd << " closed automatically." << std::endl;
        }
    }

    // --- Getters ---
    int getFd() const { return _sockfd; }
    bool isValid() const { return _isValid; }

    // --- Socket Utilities: Serialize & Send ---
    bool send(const Packet& pkt) const {
        if (!_isValid) return false;

        // 1. 序列化 Header (Host -> Network)
        PacketHeader netHeader;
        netHeader.type = htonl(pkt.header.type);
        netHeader.length = htonl(pkt.header.length);

        // 2. 发送 Header
        if (!writen((char*)&netHeader, sizeof(netHeader))) return false;

        // 3. 发送 Body
        if (pkt.header.length > 0) {
            if (!writen(pkt.body.c_str(), pkt.header.length)) return false;
        }
        return true;
    }

    // --- Socket Utilities: Recv & Deserialize ---
    // 返回 bool 表示连接是否依然正常
    bool recv(Packet& pkt) {
        if (!_isValid) return false;

        PacketHeader netHeader;
        // 1. 读取 Header
        if (!readn((char*)&netHeader, sizeof(netHeader))) {
            _isValid = false; // 标记失效
            return false;
        }

        // 2. 反序列化 Header (Network -> Host)
        pkt.header.type = ntohl(netHeader.type);
        pkt.header.length = ntohl(netHeader.length);

        // 3. 读取 Body
        if (pkt.header.length > 0) {
            std::vector<char> buffer(pkt.header.length);
            if (!readn(buffer.data(), pkt.header.length)) {
                _isValid = false;
                return false;
            }
            pkt.body.assign(buffer.data(), pkt.header.length);
        } else {
            pkt.body.clear();
        }
        return true;
    }

    // --- Operator Overloaded for Easier Use ---
    
    // 比较 Socket FD 是否相等
    bool operator==(const mySocket& other) const {
        return _sockfd == other._sockfd;
    }

    // 重载 >> 实现类似 cin >> pkt
    friend mySocket& operator>>(mySocket& sock, Packet& pkt) {
        sock.recv(pkt);
        return sock;
    }

    // 重载 << 实现类似 cout << pkt
    friend mySocket& operator<<(mySocket& sock, const Packet& pkt) {
        sock.send(pkt);
        return sock;
    }
    
    // 支持 if(sock) 检查状态
    operator bool() const {
        return _isValid;
    }
};

#endif
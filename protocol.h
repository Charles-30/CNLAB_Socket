// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <string>
#include <arpa/inet.h> 

#define SERVER_PORT 3032 

// --- 消息类型 ---
enum MsgType {
    MSG_CONNECT_SUCCESS = 0,
    REQ_TIME = 1, RSP_TIME = 2,
    REQ_NAME = 3, RSP_NAME = 4,
    REQ_LIST = 5, RSP_LIST = 6,
    REQ_SEND = 7, RSP_SEND = 8,
    MSG_EXIT = 9, MSG_TEXT = 10
};

#pragma pack(push, 1)
struct PacketHeader {
    uint32_t length; // 包体长度
    uint32_t type;   // 消息类型
};
#pragma pack(pop)

struct Packet {
    PacketHeader header;
    std::string body;
    
    Packet() { header.length = 0; header.type = 0; }
    Packet(uint32_t t, const std::string& b) {
        header.type = t;
        header.length = b.size();
        body = b;
    }
};
#endif
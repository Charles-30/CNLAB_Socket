#include <iostream>
#include <vector>
#include <thread>
#include <algorithm>
#include <mutex>
#include <csignal>
#include <sstream>
#include <glog/logging.h>
#include "mysocket.h"

// --- 全局变量 ---
int g_serverSocket = -1;
bool g_isRunning = true;
std::mutex g_clientMutex; 

struct ClientInfo {
    int id;
    int socket;
    std::string ip;
    int port;
};
std::vector<ClientInfo> g_clients;

// --- 信号处理 ---
void exitHandler(int signal) {
    if (signal == SIGINT) {
        LOG(INFO) << "Caught Ctrl+C. Shutting down...";
        g_isRunning = false;
        if (g_serverSocket >= 0) {
            close(g_serverSocket);
            g_serverSocket = -1;
        }
    }
}

// --- 功能模块实现 ---

// 1. 获取时间
std::string handleTime() {
    time_t now = time(0);
    char* dt = ctime(&now);
    if (dt[strlen(dt)-1] == '\n') dt[strlen(dt)-1] = '\0';
    return std::string(dt);
}

// 2. 获取名字
std::string handleName() {
    char hostname[256];
    if (gethostname(hostname, 256) == 0) {
        return std::string(hostname);
    }
    return "Unknown Server";
}

// 3. 获取列表 (线程安全)
std::string handleList() {
    std::string listStr = "ID\tIP Address\tPort\n";
    std::lock_guard<std::mutex> lock(g_clientMutex);
    for(const auto& c : g_clients) {
        listStr += std::to_string(c.id) + "\t" + c.ip + "\t" + std::to_string(c.port) + "\n";
    }
    return listStr;
}

// 4. 转发消息 (核心逻辑)
// 协议约定：客户端发来的 body 格式为 "TargetID Content" (例如 "1002 Hello World")
std::string handleForward(int srcId, const std::string& reqBody) {
    std::stringstream ss(reqBody);
    int targetId;
    std::string content;
    
    // 解析 ID 和 内容
    ss >> targetId;
    // 获取剩余部分作为消息内容 (处理含空格的消息)
    std::getline(ss, content); 
    if (!content.empty() && content[0] == ' ') content.erase(0, 1); // 去除开头空格

    if (ss.fail() || content.empty()) {
        return "Format Error. Usage: <TargetID> <Message>";
    }

    // 查找目标并转发
    std::lock_guard<std::mutex> lock(g_clientMutex);
    auto it = std::find_if(g_clients.begin(), g_clients.end(), 
        [targetId](const ClientInfo& c){ return c.id == targetId; });

    if (it != g_clients.end()) {
        // 目标存在，进行转发
        // 构造提示语: "[来自 1001]: Hello"
        std::string forwardMsg = "[From " + std::to_string(srcId) + "]: " + content;
        
        // 创建临时 socket 对象发送给目标
        mySocket targetSock(it->socket);
        targetSock << Packet(MSG_TEXT, forwardMsg);
        
        return "Message sent to Client " + std::to_string(targetId);
    } else {
        return "Target Client ID " + std::to_string(targetId) + " not found.";
    }
}

// --- 客户端处理线程 ---
void handleClient(int clientFd, int clientId, std::string ip, int port) {
    mySocket sock(clientFd);
    
    // 发送欢迎
    sock << Packet(MSG_CONNECT_SUCCESS, "Welcome! Your ID is " + std::to_string(clientId));

    Packet pkt;
    // 循环接收请求
    while (g_isRunning && (sock >> pkt)) {
        if (!sock) break; // 连接断开

        // 任务分发 (Dispatch)
        switch (pkt.header.type) {
            case REQ_TIME: {
                std::string res = handleTime();
                sock << Packet(RSP_TIME, res);
                LOG(INFO) << "Client #" << clientId << " REQ_TIME";
                break;
            }
            case REQ_NAME: {
                std::string res = handleName();
                sock << Packet(RSP_NAME, res);
                LOG(INFO) << "Client #" << clientId << " REQ_NAME";
                break;
            }
            case REQ_LIST: {
                std::string res = handleList();
                sock << Packet(RSP_LIST, res);
                LOG(INFO) << "Client #" << clientId << " REQ_LIST";
                break;
            }
            case REQ_SEND: {
                // 调用转发逻辑
                std::string res = handleForward(clientId, pkt.body);
                // 反馈结果给发送者
                sock << Packet(RSP_SEND, res);
                LOG(INFO) << "Client #" << clientId << " REQ_SEND: " << res;
                break;
            }
            case MSG_EXIT:
                LOG(INFO) << "Client #" << clientId << " sent EXIT.";
                goto DISCONNECT;
            default:
                LOG(WARNING) << "Unknown Packet Type: " << pkt.header.type;
        }
    }

DISCONNECT:
    // 移除客户端
    {
        std::lock_guard<std::mutex> lock(g_clientMutex);
        auto it = std::remove_if(g_clients.begin(), g_clients.end(), 
            [clientId](const ClientInfo& c){ return c.id == clientId; });
        g_clients.erase(it, g_clients.end());
    }
    // 手动关闭 Socket
    close(clientFd);
    LOG(INFO) << "Client #" << clientId << " Disconnected.";
}

int main(int argc, char* argv[]) {
    signal(SIGINT, exitHandler);
    google::InitGoogleLogging(argv[0]);
    FLAGS_log_dir = "log";
    FLAGS_alsologtostderr = true;

    // 创建监听 Socket
    g_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(g_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);
    
    if (bind(g_serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG(FATAL) << "Bind failed";
    }
    listen(g_serverSocket, 5);
    LOG(INFO) << "Server started on " << SERVER_PORT;

    int idCounter = 1000;

    while (g_isRunning) {
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        int clientSock = accept(g_serverSocket, (struct sockaddr*)&clientAddr, &len);
        
        if (clientSock < 0) {
            if (!g_isRunning) break;
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ip, INET_ADDRSTRLEN);
        int port = ntohs(clientAddr.sin_port);
        int newId = ++idCounter;

        LOG(INFO) << "New Client: #" << newId;

        {
            std::lock_guard<std::mutex> lock(g_clientMutex);
            g_clients.push_back({newId, clientSock, std::string(ip), port});
        }

        std::thread(handleClient, clientSock, newId, std::string(ip), port).detach();
    }
    
    google::ShutdownGoogleLogging();
    return 0;
}
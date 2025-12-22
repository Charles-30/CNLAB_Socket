#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <csignal>
#include <limits>
#include <glog/logging.h>
#include "mysocket.h"

std::atomic<bool> isRunning(true);
std::atomic<bool> isConnected(false);
int g_socket = -1; 
std::thread tRecv;

std::atomic<int> timeResponseCount(0); // 记录收到 RSP_TIME 的次数

// 信号处理
void exitHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n[System] Exiting..." << std::endl;
        isRunning = false;
        if (g_socket >= 0) close(g_socket);
        exit(0);
    }
}

// 接收线程
void receiveThread(int fd) {
    mySocket sock(fd);
    Packet pkt;
    while (isRunning && isConnected) {
        if (sock >> pkt) {
            std::cout << "\n--------------------------------\n";
            if (pkt.header.type == RSP_TIME) {
                // 每次收到时间响应，计数器加 1
                timeResponseCount++;
                // 打印简要信息，避免 100 行输出刷屏过快
                std::cout << "\r[Counter] Received Time Responses: " << timeResponseCount << std::flush << "\n";
                std::cout << "[Time]: " << pkt.body;
            }
            else if (pkt.header.type == RSP_NAME) std::cout << "[Name]: " << pkt.body;
            else if (pkt.header.type == RSP_LIST) std::cout << "[List]:\n" << pkt.body;
            else if (pkt.header.type == RSP_SEND) std::cout << "[Result]: " << pkt.body;
            else if (pkt.header.type == MSG_TEXT) std::cout << "[Message]: " << pkt.body;
            else if (pkt.header.type == MSG_CONNECT_SUCCESS) std::cout << "[Server]: " << pkt.body;
            else std::cout << "[Unknown]: " << pkt.body;
            std::cout << "\n--------------------------------\n>> " << std::flush;
        } else {
            if (isRunning && isConnected) std::cout << "\n[System] Server disconnected.\n>> " << std::flush;
            isConnected = false;
            break;
        }
    }
}

void showMenu() {
    if (!isConnected) {
        std::cout << "\n=== ZJU Client (Offline) ===\n";
        std::cout << "a) 连接服务器\n";
        std::cout << "g) 退出\n";
    } else {
        std::cout << "\n=== ZJU Client (Online) ===\n";
        std::cout << "b) 断开连接\n";
        std::cout << "c) 获取时间\n";
        std::cout << "d) 获取名字\n";
        std::cout << "e) 获取列表\n";
        std::cout << "f) 发送消息\n";
        std::cout << "g) 退出\n";
    }
    std::cout << ">> ";
}

int main(int argc, char* argv[]) {
    signal(SIGINT, exitHandler);
    google::InitGoogleLogging(argv[0]);
    FLAGS_log_dir = "log";

    while (isRunning) {
        showMenu();
        char choice;
        if (!(std::cin >> choice)) break;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 清除缓冲区的换行符

        if (!isConnected) {
            if (choice == 'a') {
                g_socket = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = htons(SERVER_PORT);
                inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

                if (connect(g_socket, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                    isConnected = true;
                    if (tRecv.joinable()) tRecv.join();
                    tRecv = std::thread(receiveThread, g_socket);
                } else {
                    std::cout << "Connect failed.\n";
                    close(g_socket);
                }
            } else if (choice == 'g') break;
        } 
        else {
            mySocket sender(g_socket); // 临时对象用于发送

            switch (choice) {
                case 'b': // 断开
                    sender << Packet(MSG_EXIT, "");
                    isConnected = false;
                    close(g_socket); // 关闭 socket 触发 recv 线程退出
                    g_socket = -1;
                    if (tRecv.joinable()) tRecv.join();
                    break;
                case 'c': {
                    std::cout << "[System] Starting 100 consecutive TIME requests..." << std::endl;
                    timeResponseCount = 0; // 每次测试前清零计数器
                    
                    for (int i = 0; i < 100; ++i) {
                        sender << Packet(REQ_TIME, "");
                    }
                    std::cout << "\n[System] 100 requests sent. Waiting for responses..." << std::endl;
                    break;
                }
                case 'd': sender << Packet(REQ_NAME, ""); break;
                case 'e': sender << Packet(REQ_LIST, ""); break;
                case 'f': {
                    std::string idStr, msg;
                    std::cout << "Target Client ID: ";
                    std::cin >> idStr;
                    std::cin.ignore(); // 清除 ID 后的空格/换行
                    std::cout << "Message Content: ";
                    std::getline(std::cin, msg);
                    
                    // 拼接格式 "ID Content"
                    sender << Packet(REQ_SEND, idStr + " " + msg);
                    break;
                }
                case 'g': 
                    sender << Packet(MSG_EXIT, "");
                    isRunning = false; 
                    break;
                default: std::cout << "Invalid option.\n";
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (g_socket >= 0) close(g_socket);
    if (tRecv.joinable()) tRecv.join();
    return 0;
}
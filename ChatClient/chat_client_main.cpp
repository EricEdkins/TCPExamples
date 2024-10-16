#pragma once

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <iostream>
#include <atomic>
#include <conio.h> // For _getch() to read input without immediate echoing
#include<sstream>
#include<chrono>
#include <iomanip>
#include <ctime>

#include "buffer.h"
#include <string>

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "8412"

struct PacketHeader
{
    uint32_t packetSize;
    uint32_t messageType;
};

struct ChatMessage
{
    PacketHeader header;
    uint32_t messageLength;
    std::string message;
};

std::atomic<bool> isRunning(true);

std::string getCurrentTimestamp() {
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    // Convert to local time
    std::tm now_tm; // Declare a tm variable
    localtime_s(&now_tm, &now_time_t); // Use localtime_s to fill the tm structure

    // Format the time as a string (HH:MM:SS)
    std::ostringstream oss;
    oss << std::put_time(&now_tm, "%H:%M:%S - ");
    return oss.str();
}

void receiveMessage(SOCKET socket)
{
    while (isRunning.load(std::memory_order_relaxed))
    {
        const int bufSize = 512;
        Buffer buffer(bufSize);
        int result = recv(socket, (char*)(&buffer.m_BufferData[0]), bufSize, 0);
        if (result > 0)
        {
            uint32_t packetSize = buffer.ReadUInt32LE();
            uint32_t messageType = buffer.ReadUInt32LE();


            // testing recieving time stamp but i think its better to just send it so everyone sees it
          /*  std::string time = getCurrentTimestamp();
            std::cout << "\t\t(" + time + ")";*/

            if (messageType == 1)
            {
                uint32_t messageLength = buffer.ReadUInt32LE();
                std::string msg = buffer.ReadString(messageLength);
                
                std::cout << "\r" << msg << "\n";  // Print message and move to a new line
            }
        }
        else if (result == 0)
        {
            std::cout << "Server closed the connection.\n";
            break;
        }
        else
        {
            printf("recv failed with error %d\n", WSAGetLastError());
            break;
        }
    }
}

void sendMessageToServer(SOCKET serverSocket, const std::string& message)
{

    ChatMessage chatMessage;
    // Combine the message with the timestamp
    chatMessage.message = message;
    chatMessage.messageLength = chatMessage.message.length();  // Update message length
    chatMessage.header.messageType = 1;
    chatMessage.header.packetSize = sizeof(PacketHeader) + sizeof(uint32_t) + chatMessage.messageLength;

    Buffer buffer(512);
    buffer.WriteUInt32LE(chatMessage.header.packetSize);
    buffer.WriteUInt32LE(chatMessage.header.messageType);
    buffer.WriteUInt32LE(chatMessage.messageLength);
    buffer.WriteString(chatMessage.message);

    send(serverSocket, (const char*)(&buffer.m_BufferData[0]), chatMessage.header.packetSize, 0);
}

void printHeader() {
    // Print the header
    const char* header = "Eric's Chat Room";
    int length = strlen(header);
    int borderLength = length + 10; // Additional space for border (2 stars + 2 spaces on each side + 2 for the stars)

    // Print top border
    for (int i = 0; i < borderLength; i++) {
        printf("*");
    }
    printf("\n");

    // Print empty line for thicker border
    for (int i = 0; i < borderLength; i++) {
        printf("*");
    }
    printf("\n");

    // Print header line with stars and spaces
    printf("**   %s   **\n", header);

    // Print empty line for thicker border
    for (int i = 0; i < borderLength; i++) {
        printf("*");
    }
    printf("\n");

    // Print bottom border
    for (int i = 0; i < borderLength; i++) {
        printf("*");
    }
    printf("\n");
}

void processInputAndSendMessage(SOCKET serverSocket, const std::string& username)
{
    std::string userInput;
    char ch;

    // Reading input character by character
    while (true)
    {
        
        std::string time = getCurrentTimestamp();
        ch = _getch();  // Get character without displaying it on console

        if (ch == '\r')  // Enter key
        {
            if (!userInput.empty())
            {
                if (userInput == "/exit")
                {
                    // Create a leave message
                    std::string leaveMessage = time + username + " has left the chat";

                    // Send the leave message to the server
                    sendMessageToServer(serverSocket, leaveMessage);

                    isRunning = false;
                    std::cout << "Exiting chat...\n";
                    break;
                }

                // Format message with username
                std::string messageToSend = time + "[" + username + "]: " + userInput;

                // Send the message to the server
                sendMessageToServer(serverSocket, messageToSend);

                // Clear the current input line and display the sent message
                printf("\r%s\n", messageToSend.c_str());

                // Reset userInput for the next message
                userInput.clear();
            }
        }
        else if (ch == '\b')  // Handle backspace
        {
            if (!userInput.empty())
            {
                userInput.pop_back();
                printf("\b \b");  // Move cursor back, print space to erase, move cursor back again
            }
        }
        else
        {
            userInput += ch;
            printf("%c", ch);  // Echo typed character
        }
    }
}

int main(int arg, char** argv)
{

    printHeader();

    // Initialize Winsock
    WSADATA wsaData;
    int result;

    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        printf("WSAStartup failed with error %d\n", result);
        return 1;
    }

    //printf("WSAStartup successfully!\n");

    struct addrinfo* info = nullptr;
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    result = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &info);
    if (result != 0)
    {
        printf("getaddrinfo failed with error %d\n", result);
        WSACleanup();
        return 1;
    }

    //printf("getaddrinfo was successful!\n");

    // Create the server socket
    SOCKET serverSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (serverSocket == INVALID_SOCKET)
    {
        printf("socket failed with error %d\n", WSAGetLastError());
        freeaddrinfo(info);
        WSACleanup();
        return 1;
    }

   // printf("socket created successfully!\n");



    std::cout << "Enter your name: ";
    std::string name;
    std::getline(std::cin, name);

    // Connect to the server
    result = connect(serverSocket, info->ai_addr, (int)info->ai_addrlen);
    if (result == INVALID_SOCKET)
    {
        printf("connect failed with error %d\n", WSAGetLastError());
        closesocket(serverSocket);
        freeaddrinfo(info);
        WSACleanup();
        return 1;
    }

   // printf("Connected to the server successfully!\n");

    std::cout << "Connected to the room as " << name << "...\n";
  
    std::string joinTime = getCurrentTimestamp();

    // Notify others that this user has joined
    ChatMessage joinMessage;
    joinMessage.message = joinTime + name + " has joined the chat";
    joinMessage.messageLength = joinMessage.message.length();
    joinMessage.header.messageType = 1;
    joinMessage.header.packetSize = joinMessage.messageLength + sizeof(joinMessage.messageLength) + sizeof(joinMessage.header.messageType) + sizeof(joinMessage.header.packetSize);

    Buffer jbuffer(512);
    jbuffer.WriteUInt32LE(joinMessage.header.packetSize);
    jbuffer.WriteUInt32LE(joinMessage.header.messageType);
    jbuffer.WriteUInt32LE(joinMessage.messageLength);
    jbuffer.WriteString(joinMessage.message);

    send(serverSocket, (const char*)(&jbuffer.m_BufferData[0]), joinMessage.header.packetSize, 0);

    std::thread receiveThread(receiveMessage, serverSocket);

    while (isRunning)
    {
        processInputAndSendMessage(serverSocket, name);
    }

    // Clean up after exiting the chat
    freeaddrinfo(info);
    shutdown(serverSocket, SD_BOTH);
    closesocket(serverSocket);

    isRunning = false;
    if (receiveThread.joinable())
        receiveThread.join();

    WSACleanup();

    return 0;
}

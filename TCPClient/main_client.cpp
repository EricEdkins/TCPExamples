#pragma once

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <iostream>

#include "string"
#include "buffer.h"

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

void receiveMessage(SOCKET socket)
{
	const int bufSize = 512;
	Buffer buffer(bufSize);

	while (true)
	{
		int result = recv(socket, (char*)(&buffer.m_BufferData[0]), bufSize, 0);
		if (result > 0) 
		{
			uint32_t packetSize = buffer.ReadUInt32LE();
			uint32_t messageType = buffer.ReadUInt32LE();

			// handle the message
			uint32_t messageLength = buffer.ReadUInt32LE();
			std::string msg = buffer.ReadString(messageLength);

			std::cout << msg << "\n";
			

		}
		else if (result == 0)
		{
			std::cout << "Server closed the connection.\n";
			break;
		}
		else {
			printf("recv failed with error %d\n", WSAGetLastError());
			break;
		}
	}
}

int main(int arg, char** argv)
{
	// Initiliaze Winsock
	WSADATA wsaData;
	int result;

	// Set version 2.2 with MAKEWORD(2,2)
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		printf("WSAStartup failed with error %d\n", result);
		return 1;
	}

	printf("WSAStartup successfully!\n");

	struct addrinfo* info = nullptr;
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));  // ensure we dont have garbage data
	hints.ai_family = AF_INET;			// IPv4
	hints.ai_socktype = SOCK_STREAM;	// Stream
	hints.ai_protocol = IPPROTO_TCP;	// TCP
	hints.ai_flags = AI_PASSIVE;

	result = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &info);
	if (result != 0)
	{
		printf("getaddrinfo failed with error %d\n", result);
		WSACleanup();
		return 1;
	}

	printf("getaddrinfo was successful!\n");

	// Create the server socket
	SOCKET serverSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (serverSocket == INVALID_SOCKET)
	{
		printf("socket failed with error %d\n", WSAGetLastError());
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("socket created successfully!\n");

	// Connect
	result = connect(serverSocket, info->ai_addr, (int)info->ai_addrlen);
	if (serverSocket == INVALID_SOCKET)
	{
		printf("connect failed with error %d\n", WSAGetLastError());
		closesocket(serverSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("Connect to the server successfully!\n");

	std::thread recieveThread(receiveMessage, serverSocket);

	ChatMessage message;
	message.message = "hello";
	message.messageLength = message.message.length();
	message.header.messageType = 1; // can use an enum 
	message.header.packetSize =
		message.message.length()				// 5 'hello' has 5 bytes in it
		+ sizeof(message.messageLength)			// 4 , uint32_t  is 4 bytes
		+ sizeof(message.header.messageType)	// 4 , uint32_t  is 4 bytes
		+ sizeof(message.header.packetSize);	// 4 , uint32_t  is 4 bytes

	// 5 + 4 + 4 + 4 = 17

	const int bufferSize = 512;
	Buffer buffer(bufferSize);

	// write our packet to the buffer
	buffer.WriteUInt32LE(message.header.packetSize); // should be 17
	buffer.WriteUInt32LE(message.header.messageType); // should be 1
	buffer.WriteUInt32LE(message.messageLength); // should be 5
	buffer.WriteString(message.message); // should be hello

	result = send(serverSocket, (const char*)(&buffer.m_BufferData[0]), message.header.packetSize, 0);
	if (result == SOCKET_ERROR)
	{
		printf("send failed with error %d\n", WSAGetLastError());
		closesocket(serverSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	system("Pause");
	
	closesocket(serverSocket);
	freeaddrinfo(info);
	WSACleanup();

	return 0;
}
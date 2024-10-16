#pragma once

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>
#include <string>
#include "buffer.h"

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "8412"


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

	result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &info);
	if (result != 0)
	{
		printf("getaddrinfo failed with error %d\n", result);
		WSACleanup();
		return 1;
	}

	printf("getaddrinfo was successful!\n");

	// Create the socket
	SOCKET listenSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (listenSocket == INVALID_SOCKET)
	{
		printf("socket failed with error %d\n", WSAGetLastError());
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("socket created successfully!\n");

	// Bind
	result = bind(listenSocket, info->ai_addr, (int)info->ai_addrlen);
	if (result == SOCKET_ERROR)
	{
		printf("bind failed with error %d\n", result);
		closesocket(listenSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("bind was successful!\n");
	

	// listen
	result = listen(listenSocket, SOMAXCONN);
	if (result == SOCKET_ERROR)
	{
		printf("listen failed with error %d\n", result);
		closesocket(listenSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("listen was successful!\n");

	// Accept (First blocking call)
	SOCKET newClientSocket = accept(listenSocket, NULL, NULL);
	if (newClientSocket == INVALID_SOCKET)
	{
		printf("Accept failed with error %d\n", WSAGetLastError());
		closesocket(listenSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("Client connected on Socket: %d", newClientSocket);

	while (true)
	{
		const int bufferSize = 512;
		Buffer buffer(bufferSize);

		result = recv(newClientSocket, (char*)(&buffer.m_BufferData[0]), bufferSize, 0);
		if (result == SOCKET_ERROR)
		{
			printf("recv failed with error %d\n", WSAGetLastError());
			closesocket(listenSocket);
			freeaddrinfo(info);
			WSACleanup();
			return 1;
		}

		printf("Received %d bytes from the client!\n", result);

		uint32_t packetSize = buffer.ReadUInt32LE();
		uint32_t messageType = buffer.ReadUInt32LE();

		if (messageType == 1)
		{
			// handle the message
			uint32_t messageLength = buffer.ReadUInt32LE();
			std::string msg = buffer.ReadString(messageLength);
			
			printf("PacketSize:%d\nMessageType:%d\nMessageLength:%d\nMessage:%s\n", packetSize, messageType, messageLength, msg.c_str());
		}
	}
	freeaddrinfo(info);
	closesocket(listenSocket);

	WSACleanup();

	return 0;
}
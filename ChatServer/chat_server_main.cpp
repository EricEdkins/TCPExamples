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

void broadcastMessage(SOCKET senderSocket, std::vector<SOCKET>& clients, const Buffer& buffer, uint32_t packetSize)
{
	for (SOCKET clientSocket : clients)
	{
		if (clientSocket != senderSocket)
		{
			send(clientSocket, (const char*)(&buffer.m_BufferData[0]), packetSize, 0);
		}
	}
}

int main(int arg, char** argv)
{
	// Initialize Winsock
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
	ZeroMemory(&hints, sizeof(hints));  // ensure we don't have garbage data
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

	// Create the server socket
	SOCKET listenSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (listenSocket == INVALID_SOCKET)
	{
		printf("socket failed with error %d\n", WSAGetLastError());
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("socket created successfully!\n");

	result = bind(listenSocket, info->ai_addr, (int)info->ai_addrlen);
	if (result == SOCKET_ERROR)
	{
		printf("bind failed with error %d\n", WSAGetLastError());
		closesocket(listenSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	printf("bind was successful!\n");

	// Listen for incoming connections
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

	std::vector<SOCKET> activeConnections;
	fd_set socketsReadyForReading;
	FD_ZERO(&socketsReadyForReading);

	// Set a timeout for select
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while (true)
	{
		FD_ZERO(&socketsReadyForReading);
		FD_SET(listenSocket, &socketsReadyForReading);

		for (SOCKET clientSocket : activeConnections)
		{
			FD_SET(clientSocket, &socketsReadyForReading);
		}

		int count = select(0, &socketsReadyForReading, NULL, NULL, &tv);

		if (count == 0)  // Timeout occurred
		{
			continue;
		}
		else if (count == SOCKET_ERROR)
		{
			printf("select failed with error %d\n", WSAGetLastError());
			continue;
		}

		// Check if there's a new connection
		if (FD_ISSET(listenSocket, &socketsReadyForReading))
		{
			SOCKET newClientSocket = accept(listenSocket, NULL, NULL);
			if (newClientSocket == INVALID_SOCKET)
			{
				printf("accept failed with error %d\n", WSAGetLastError());
			}
			else
			{
				activeConnections.push_back(newClientSocket);

				// Notify the new user about the number of active users
				ChatMessage userCountMessage;
				
				std::string userCountStr = "Welcome! There are currently " + std::to_string(activeConnections.size()) + " user(s) in the chat.\nType '/exit' to leave the chat.";
				userCountMessage.message = userCountStr;
				userCountMessage.messageLength = userCountMessage.message.length();
				userCountMessage.header.messageType = 1;
				userCountMessage.header.packetSize = sizeof(PacketHeader) + sizeof(uint32_t) + userCountMessage.messageLength;

				Buffer buffer(512);
				buffer.WriteUInt32LE(userCountMessage.header.packetSize);
				buffer.WriteUInt32LE(userCountMessage.header.messageType);
				buffer.WriteUInt32LE(userCountMessage.messageLength);
				buffer.WriteString(userCountMessage.message);

				send(newClientSocket, (const char*)(&buffer.m_BufferData[0]), userCountMessage.header.packetSize, 0);

				printf("Client connected. Total clients: %d\n", (int)activeConnections.size());
			}
		}

		// Handle incoming messages from clients
		for (size_t i = 0; i < activeConnections.size(); i++)
		{
			SOCKET clientSocket = activeConnections[i];

			if (FD_ISSET(clientSocket, &socketsReadyForReading))
			{
				const int bufSize = 512;
				Buffer buffer(bufSize);

				int result = recv(clientSocket, (char*)(&buffer.m_BufferData[0]), bufSize, 0);

				if (result == SOCKET_ERROR)
				{
					
					printf("Client disconnected.\n"); // user left ungracefully 
					//printf("recv failed with error %d\n", WSAGetLastError());
					closesocket(clientSocket);
					activeConnections.erase(activeConnections.begin() + i);
					i--;
					continue;
				}
				else if (result == 0)
				{
					//printf("Client disconnected.\n");
					closesocket(clientSocket);
					activeConnections.erase(activeConnections.begin() + i);
					i--;
					continue;
				}

				uint32_t packetSize = buffer.ReadUInt32LE();
				uint32_t messageType = buffer.ReadUInt32LE();

				if (messageType == 1)  // Chat message
				{
					uint32_t messageLength = buffer.ReadUInt32LE();
					std::string msg = buffer.ReadString(messageLength);

					printf("PacketSize: %d\nMessageType: %d\nMessageLength: %d\nMessage: %s\n", packetSize, messageType, messageLength, msg.c_str());

					// Broadcast the message to all clients except the sender
					broadcastMessage(clientSocket, activeConnections, buffer, packetSize);
				}

				FD_CLR(clientSocket, &socketsReadyForReading);
				count--;
			}
		}
	}

	// Clean up
	freeaddrinfo(info);
	closesocket(listenSocket);

	for (SOCKET clientSocket : activeConnections)
	{
		closesocket(clientSocket);
	}

	WSACleanup();

	return 0;
}

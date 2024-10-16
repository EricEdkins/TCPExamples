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

	// create our sets

	std::vector<SOCKET> activeConnections;

	FD_SET activeSockets;				// list of all the clients connections
	FD_SET socketsReadyForReading;		// list of all the clients ready to ready

	FD_ZERO(&activeSockets);              // Intializze the sets
	FD_ZERO(&socketsReadyForReading);

	// use a timeval to prevent select from waiting forever
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while (true)
	{
		FD_ZERO(&socketsReadyForReading);

		// add our listen socket to our set to check for new connection
		FD_SET(listenSocket, &socketsReadyForReading);

		// Add all our active connections to our ready to read
		for (int i = 0; i < activeConnections.size(); i++)
		{
			FD_SET(activeConnections[i], &socketsReadyForReading);
		}

		int count = select(0, &socketsReadyForReading, NULL, NULL, &tv);

		if (count == 0)
		{
			// Timevalue expired
			continue;
		}
		if (count == SOCKET_ERROR)
		{
			printf("select had an error %d\n", WSAGetLastError());
			continue;
		}

		// Loop through 
		for (int i = 0; i < activeConnections.size(); i++)
		{
			SOCKET socket = activeConnections[i];

			if (FD_ISSET(socket, &socketsReadyForReading))
			{
				// handle receiving data
				const int bufSize = 512;
				Buffer buffer(bufSize);

				int result = recv(socket, (char*)(&buffer.m_BufferData[0]), bufSize, 0);

				if (result == SOCKET_ERROR)
				{
					printf("recv failed with error %d\n", WSAGetLastError());
					closesocket(socket);
					FD_CLR(socket, &activeSockets);
					activeConnections.erase(activeConnections.begin() + i);
					i--;
					continue;
				}
				else if (result == 0)
				{
					printf("Client disconnect");
					closesocket(socket);
					FD_CLR(socket, &activeSockets);
					activeConnections.erase(activeConnections.begin() + i);
					i--;
					continue;
				}

				uint32_t packetSize = buffer.ReadUInt32LE();
				uint32_t messageType = buffer.ReadUInt32LE();

				if (messageType == 1)
				{
					// handle the message
					uint32_t messageLength = buffer.ReadUInt32LE();
					std::string msg = buffer.ReadString(messageLength);

					printf("PacketSize:%d\nMessageType:%d\nMessageLength:%d\nMessage:%s\n", packetSize, messageType, messageLength, msg.c_str());

					ChatMessage message;
					message.message = "Server received message from client";
					message.messageLength = message.message.length();
					message.header.messageType = 1; // can use an enum 
					message.header.packetSize =
						message.message.length()				// 5 'hello' has 5 bytes in it
						+ sizeof(message.messageLength)			// 4 , uint32_t  is 4 bytes
						+ sizeof(message.header.messageType)	// 4 , uint32_t  is 4 bytes
						+ sizeof(message.header.packetSize);	// 4 , uint32_t  is 4 bytes

					// 5 + 4 + 4 + 4 = 17
					Buffer bufferSend(bufSize);

					// write our packet to the buffer
					bufferSend.WriteUInt32LE(message.header.packetSize); // should be 17
					bufferSend.WriteUInt32LE(message.header.messageType); // should be 1
					bufferSend.WriteUInt32LE(message.messageLength); // should be 5
					bufferSend.WriteString(message.message); // should be hello

					for (int j = 0; j < activeConnections.size(); j++)
					{
						SOCKET outSocket = activeConnections[j];

						if (outSocket != listenSocket)
						{
							send(outSocket, (const char*)(&bufferSend.m_BufferData[0]), message.header.packetSize, 0);
						}
					}
				}

				FD_CLR(socket, &socketsReadyForReading);
				count--;
			}
		}

		// Handle any new connections
		// if count is STILL not 0, then we have a socket that is a new connection
		if (count > 0)
		{
			if (FD_ISSET(listenSocket, &socketsReadyForReading))
			{
				SOCKET newConnection = accept(listenSocket, NULL, NULL);
				if (newConnection == INVALID_SOCKET)
				{
					printf("accept failed with error %d\n", WSAGetLastError());
				}
				else
				{
					activeConnections.push_back(newConnection);
					FD_SET(newConnection, &activeConnections);
					FD_CLR(listenSocket, &socketsReadyForReading);

					printf("Client connect with socket: %d\n", (int)newConnection);
				}
			}
		}
	}

	freeaddrinfo(info);
	closesocket(listenSocket);
	
	for (int i = 0; i < activeConnections.size(); i++)
	{
		closesocket(activeConnections[i]);
	}

	WSACleanup();

	return 0;
}
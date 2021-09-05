#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <WinBase.h>
#include <iostream>
#include <thread>

int Init()
{
	WSADATA wsaData;

	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

	return result;
}

SOCKET CreateSocket(int protocol)
{
	SOCKET result = INVALID_SOCKET;

	int type = SOCK_DGRAM;
	if (protocol == IPPROTO_TCP)
		type = SOCK_STREAM;

	result = socket(AF_INET, type, protocol);

	if (result != INVALID_SOCKET && protocol == IPPROTO_TCP)
	{
		u_long mode = 1;

		int err = ioctlsocket(result, FIONBIO, &mode);

		if (err != NO_ERROR)
		{
			closesocket(result);
			result = INVALID_SOCKET;
		}
	}

	return result;
}

sockaddr_in* CreateAddress(const char* ip, int port)
{
	sockaddr_in* result = (sockaddr_in*)calloc(sizeof(*result), 1);

	result->sin_family = AF_INET;
	result->sin_port = htons(port);

	if (ip == NULL)
		result->sin_addr.S_un.S_addr = INADDR_ANY;

	else
	{
		result->sin_addr.S_un.S_addr = inet_addr(ip);

		if (result->sin_addr.S_un.S_addr == INADDR_NONE)
		{
			ADDRINFOA hint;
			PADDRINFOA pAddr;
			memset(&hint, 0, sizeof(ADDRINFOA));
			hint.ai_family = AF_INET;
			int err = getaddrinfo(ip, NULL, &hint, &pAddr);

			if (err != 0)
			{
				std::cout << "wrong DNS address!!! can't resolve!!!" << std::endl;
				return NULL;
			}
			else
			{
				result->sin_addr = ((SOCKADDR_IN*)pAddr->ai_addr)->sin_addr;
				freeaddrinfo(pAddr);
			}
		}
	}


	return result;
}


int Send(SOCKET sock, char* buffer, sockaddr_in* dest)
{
	int result = sendto(sock, buffer, strlen(buffer), 0, (sockaddr*)dest, sizeof(sockaddr_in));

	if (result == SOCKET_ERROR)
		return -1;
	else
		return result;
}



int SendTCP(SOCKET sock, const char* buffer, int bytes)
{
	int result = send(sock, buffer, bytes, 0);

	if (result == SOCKET_ERROR)
		return -1;
	else
		return result;
}



int Bind(SOCKET sock, sockaddr_in* addr)
{
	int size = sizeof(sockaddr_in);
	int result = bind(sock, (sockaddr*)addr, size);

	if (result == SOCKET_ERROR)
		return GetLastError();

	return 0;
}



int Listen(SOCKET sock, int backlog)
{
	int max = backlog;

	if (max < 1)
		max = SOMAXCONN;

	int result = listen(sock, max);

	if (result == SOCKET_ERROR)
		return GetLastError();

	return 0;
}

SOCKET Accept(SOCKET sock, sockaddr_in* incoming)
{
	int size = sizeof(sockaddr_in);

	SOCKET socket;

	socket = accept(sock, (sockaddr*)incoming, &size);

	if (socket == INVALID_SOCKET)
		return -1;

	return socket;
}



int Receive(SOCKET sock, char* buffer, int maxBytes)
{
	sockaddr sender;

	int size = sizeof(sockaddr);

	int bytes = recvfrom(sock, buffer, maxBytes, 0, &sender, &size);

	if (bytes == SOCKET_ERROR)
		return -1;

	return bytes;
}



int ReceiveTCP(SOCKET sock, char* buffer, int maxBytes)
{
	int bytes = recv(sock, buffer, maxBytes, 0);

	if (bytes == SOCKET_ERROR)
		return -1;

	return bytes;
}

int Connect(SOCKET sock, sockaddr_in* address)
{
	if (connect(sock, (sockaddr*)address, sizeof(sockaddr_in)) == SOCKET_ERROR)
		return WSAGetLastError();
	else
		return 0;
}



void Close(SOCKET sock, boolean now)
{
	if (now)
		closesocket(sock);
	else
		shutdown(sock, SD_SEND);
}

void Deinit()
{
	WSACleanup();
}

void stepnine(int clisocket)
{
	std::string temp;
	char buffer[1600];

	while (true)
	{
		memset(buffer, 0, 1600);

		int n = ReceiveTCP(clisocket, buffer, 1500);

		if (n == 0)
		{
			break;
		}
		else
		{
			temp += buffer;
			
			n = temp.find("\n\n");
			if (n != std::string::npos)
			{
				break;
			}
			n = temp.find("\r\n\r\n");
			if (n != std::string::npos)
			{
				break;
			}
		}
	}
	shutdown(clisocket, SD_RECEIVE);


	std::size_t hostloc = temp.find("Host:");

	std::size_t r = temp.find_first_of('\r', hostloc);

	std::string host = temp.substr(hostloc + 6, r - (hostloc + 6));

	SOCKET socket = CreateSocket(IPPROTO_TCP);

	sockaddr_in* sockaddr = CreateAddress(host.c_str(), 80);

	Connect(socket, sockaddr);

	std::string sendbuf = temp;

	int byte;

	while (true)
	{
		byte = SendTCP(socket, sendbuf.c_str(), (int)sendbuf.length());

		if (byte != -1)
		{
			break;
		}
	}

	std::string tempt;

	memset(buffer, 0, 1600);

	ReceiveTCP(socket, buffer, 1500);
	tempt += buffer;

	while (true)
	{
		byte = SendTCP(clisocket, buffer, (int)strlen(buffer));

		if (byte != -1)
		{
			break;
		}
	}

	while (true)
	{
		memset(buffer, 0, 1600);

		int n = ReceiveTCP(socket, buffer, 1500);

		if (n == 0)
		{
			break;
		}
		else
		{
			tempt += buffer;
			while (true)
			{
				byte = SendTCP(clisocket, buffer, (int)strlen(buffer));

				if (byte != -1)
				{
					break;
				}
			}
		}
	}

	shutdown(socket, SD_BOTH);
	shutdown(clisocket, SD_SEND);

	Close(clisocket, 1);
	Close(socket, 1);
}


int main(int argc, char* argv[])
{
	int portno;

	if (argv[1] == NULL)
	{
		std::cout << "no argument!!! require port between 0 to 65535" << std::endl;
		exit(1);
	}

	portno = atoi(argv[1]);

	if (portno < 0 || portno > 65535)
	{
		std::cout << "wrong port range!!! require port between 0 to 65535" << std::endl;
		exit(1);
	}

	Init();

	sockaddr_in* sockaddr = CreateAddress(NULL, portno);

	SOCKET socket = CreateSocket(IPPROTO_TCP);

	if (sockaddr == NULL)
	{
		Close(socket, 1);
		Deinit();
		exit(1);
	}

	//Connect(socket, sockaddr);
	Bind(socket, sockaddr);

	std::cout << "Listening on port " << portno << std::endl;
	Listen(socket, 5);

	int byte;
	sockaddr_in cliaddr;
	while (true)
	{
		byte = Accept(socket, &cliaddr);

		if (byte != -1) {
			std::thread thread(stepnine, byte);
			thread.detach();
		}

		Sleep(100);
	}

	return 0;
}
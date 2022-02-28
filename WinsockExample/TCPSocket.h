#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifdef __cplusplus
extern "C" {
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#ifdef __cplusplus
}
#endif

#ifndef __WGSOCKET_H__  
#define __WGSOCKET_H__  
#ifdef WIN32  
//#include <atlsocket.h>
#include <iostream>
#include <cstdlib>
//#define SD_RECEIVE 0  
//#define SD_SEND 1  
//#define SD_BOTH 2  
#pragma comment(lib, "ws2_32.lib")  
#else  
#include <iostream>
#define SOCKET_ERROR -1  
#define INVALID_SOCKET -1  
#define SHUT_RD 0 /* No more receptions.  */  
#define SHUT_WR 1 /* No more transmissions.  */  
#define SHUT_RDWR 2 /* No more receptions or transmissions.  */  
#endif 
#endif

#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <codecvt>
#include <locale>

enum SOCK_STATE {
	SOCK_OK = 0,
	SOCK_ERROR,
	SOCK_TYPE_NONE,
	SOCK_TYPE_SERVER,
	SOCK_TYPE_CLIENT
};

struct ClientInf {
	SOCKET Sock;
	std::string Name;

	ClientInf() {
		Sock = 0;
		Name = "";
	}

	ClientInf(SOCKET _Sock, std::string _Name){
		Sock = _Sock;
		Name = _Name;
	}
};

struct DataPacket {
	std::vector<char> Data;
	long long Len;
	long long RecvCounter;

	DataPacket() {
		Data = std::vector<char>();
		Len = RecvCounter = 0;
	}
};


class TCPSocket {
public:
	TCPSocket();
	~TCPSocket();

private:
	SOCKET mSocket;
	SOCK_STATE mSocketType;
	sockaddr_in mLocalAddr;
	sockaddr_in mServerAddr;
	unsigned int mMaxBufferSize;
	std::vector<ClientInf> mArySocketClients;
	unsigned int mLimitClient;

	std::vector<std::string> mAryStrLog;

	bool mFIsThreadAlive;
	bool mFShutdown;
	std::atomic<bool> mFShutdownCheck;
	//std::promise<void> mFShutdown;
	std::thread* mThreadServer;
	std::mutex mMutexThreadServer;

	std::thread* mThreadClient;
	std::mutex mMutexThreadClient;

	std::vector<char> mAryRecvMsg;

	void StartServerThread(/*std::future<void> FutureObj*/);
	void StartClientThread(/*std::future<void> FutureObj*/);
	void Clear();

	void SendToServer(char* Data, long long DataLen);
	void SendToClient(char* Data, long long DataLen, std::vector<ClientInf> Targets);
	void SendImplement(SOCKET s, const char* buf, long long len, int flags);
	void PackMessage(std::string Message, char** ppData, long long& Len);
	void UnpackMessage(std::vector<DataPacket> &listDataRecv, char* pattern, int patternLen);
	std::vector<int> DatacmpIndex(char* Data, int DataSize, char* Pattern, int PatternSize, int MaxCount = 0);

protected:
	virtual void RecvDataCallback(DataPacket packet);

public:
	SOCK_STATE InitServer(
		unsigned int Port,
		unsigned int MaxClient = 30,
		unsigned int MaxBufferSize = 256);
	SOCK_STATE InitClient(
		std::string IPAddress,
		unsigned int Port,
		unsigned int MaxBufferSize = 256);

	SOCK_STATE InitUdpServer(unsigned int Port);
	SOCK_STATE InitUdpClient(
		std::string IPAddress,
		unsigned int Port);

	SOCK_STATE StartServer(unsigned int MaxListenBacklog = 10);
	SOCK_STATE StartClient();
	SOCK_STATE Pause();
	SOCK_STATE Shutdown();

	SOCKET GetSocket();
	SOCK_STATE GetSocketType();

	void Send(std::string Message, std::vector<ClientInf> Targets = std::vector<ClientInf>());
	std::vector<char> FetchData();
};

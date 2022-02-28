// Fill out your copyright notice in the Description page of Project Settings.

#include "pch.h"
#include "UDPSocket.h"

UDPSocket::UDPSocket(WORD SockVersion /*= MAKEWORD(2, 2)*/) : SocketHandlerBase(SockVersion)
{
	Clear();

	mPacketPattern = "DATA_START";
}

UDPSocket::~UDPSocket()
{

}

bool UDPSocket::InitServer(unsigned short LocalPort)
{
	Clear();

	mSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (mSocket == INVALID_SOCKET) {
		//Util::OutputDebugMessage("ERROR: Fail to create TCP socket");
	}

	memset(&mLocalAddr, 0, sizeof(mLocalAddr));
	mLocalAddr.sin_family = AF_INET;
	mLocalAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	mLocalAddr.sin_port = htons(LocalPort);

	mSocketType = _SOCKET_PLUGIN_TYPE_SERVER;

	return true;
}

bool UDPSocket::StartServer(int MaxListenBacklog /*= 10*/)
{
	if (bind(mSocket, (sockaddr*)&mLocalAddr, sizeof(mLocalAddr)) != 0)
	{
		//printf("Bind failed with error code : %d", WSAGetLastError());
		//exit(EXIT_FAILURE);
	}

#ifdef _UNREAL
	mServerLiveLoopWorker = new ServerLiveLoopWorker(this);
	mServerLiveLoopWorker->Init("UDPSocketServerLiveLoop");
#else
	mThreadServer = std::thread(&UDPSocket::ServerLiveLoop, this);
#endif

	return true;
}

bool UDPSocket::InitClient(std::string TargetIP, unsigned short TargetPort)
{
	Clear();

	mSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (mSocket == INVALID_SOCKET) {
		//Util::OutputDebugMessage("ERROR: Fail to create TCP socket");
	}

	memset(&mExternalAddr, 0, sizeof(mExternalAddr));
	mExternalAddr.sin_family = AF_INET;
	mExternalAddr.sin_port = htons(TargetPort);
	mExternalAddr.sin_addr.S_un.S_addr = inet_addr(TargetIP.c_str());

	mSocketType = _SOCKET_PLUGIN_TYPE_CLIENT;

	return true;
}

bool UDPSocket::StartClient()
{
#ifdef _UNREAL
	mClientLiveLoopWorker = new ClientLiveLoopWorker(this);
	mClientLiveLoopWorker->Init("UDPSocketClientLiveLoop");
#else
	mThreadClient = std::thread(&UDPSocket::ClientLiveLoop, this);
#endif

	return true;
}

void UDPSocket::Send(std::string Message)
{
	char* packedMsg = nullptr;
	long long packedMshLen = 0;
	PackMessage(Message, &packedMsg, packedMshLen);
	SendImplement(mSocket, packedMsg, packedMshLen, 0, (sockaddr *)&mExternalAddr);


	if (packedMsg != nullptr) {
		delete packedMsg;
	}
}

void UDPSocket::SendRaw(byte *Raw, int RawLen)
{
	SendImplement(mSocket, (char*)Raw, RawLen, 0, (sockaddr *)&mExternalAddr);
}

void UDPSocket::Shutdown()
{
	if (mFIsThreadAlive) {
		mFShutdown = true;

#ifdef _UNREAL
		if (mSocketType == _SOCKET_PLUGIN_TYPE_SERVER && mServerLiveLoopWorker != nullptr) {
			mServerLiveLoopWorker->Stop();
			delete mServerLiveLoopWorker;
			mServerLiveLoopWorker = nullptr;
		}
		if (mSocketType == _SOCKET_PLUGIN_TYPE_CLIENT && mClientLiveLoopWorker != nullptr) {
			mClientLiveLoopWorker->Stop();
			delete mClientLiveLoopWorker;
			mClientLiveLoopWorker = nullptr;
		}
#else
		if (mSocketType == _SOCKET_PLUGIN_TYPE_SERVER && mThreadServer.joinable()) {
			mThreadServer.join();
		}
		if (mSocketType == _SOCKET_PLUGIN_TYPE_CLIENT && mThreadClient.joinable()) {
			mThreadClient.join();
		}
#endif

		shutdown(mSocket, SD_SEND);
		closesocket(mSocket);
	}

	mFIsThreadAlive = false;
}

void UDPSocket::ServerLiveLoop()
{
	if (mSocketType != _SOCKET_PLUGIN_TYPE_SERVER) {
		//Util::OutputDebugMessage("ERROR: This object is not a server");
		return;
	}

	mFIsThreadAlive = true;

	fd_set readfd;
	SOCKET maxfd;

	int activity = 0;
	timeval timeout;
	memset(&timeout, 0, sizeof(timeval));
	timeout.tv_usec = 50000;
	std::ostringstream oss;
	int count = 0;
	int addrlen = sizeof(mLocalAddr);
	BYTE *recvbuffer = new BYTE[_SOCKET_PLUGIN_MAX_BUFFER];

	while (!mFShutdown) {
		FD_ZERO(&readfd);
		FD_SET(mSocket, &readfd);
		maxfd = mSocket;

		select(maxfd + 1, &readfd, NULL, NULL, &timeout);

		if (FD_ISSET(mSocket, &readfd)) {		// a client send something
			ZeroMemory(recvbuffer, _SOCKET_PLUGIN_MAX_BUFFER);
			int valread = 0;	// read how many data
			//if ((valread = recv(mSocket, (char *)recvbuffer, _SOCKET_PLUGIN_MAX_BUFFER, 0)) > 0) {
			//	ServerRecvPacket(0, recvbuffer, valread);
			//}
			sockaddr_in sockaddrSource;
			int sockaddrLen = sizeof(sockaddr_in);
			memset(&sockaddrSource, 0, sockaddrLen);
			if ((valread = recvfrom(mSocket, (char *)recvbuffer, _SOCKET_PLUGIN_MAX_BUFFER, 0, (sockaddr*)&sockaddrSource, &sockaddrLen)) > 0) {
				LinkInfo connection;
				connection.address = sockaddrSource;
				ServerRecvPacket(&connection, recvbuffer, valread);
			}
		}
	}

	delete recvbuffer;
	mFShutdownCheck = true;
}

void UDPSocket::ClientLiveLoop()
{
	if (mSocketType != _SOCKET_PLUGIN_TYPE_CLIENT) {
		//Util::OutputDebugMessage("ERROR: This object is not a client");
		return;
	}

	mFIsThreadAlive = true;

	fd_set readfd;
	SOCKET maxfd;

	int activity = 0;
	timeval timeout;
	memset(&timeout, 0, sizeof(timeval));
	timeout.tv_usec = 50000;
	std::ostringstream oss;
	int count = 0;
	int addrlen = sizeof(mLocalAddr);
	BYTE *recvbuffer = new BYTE[_SOCKET_PLUGIN_MAX_BUFFER];

	while (!mFShutdown) {
		FD_ZERO(&readfd);
		FD_SET(mSocket, &readfd);
		maxfd = mSocket;

		select(maxfd + 1, &readfd, NULL, NULL, &timeout);

		if (FD_ISSET(mSocket, &readfd)) {		// a client send something
			ZeroMemory(recvbuffer, _SOCKET_PLUGIN_MAX_BUFFER);
			int valread = 0;	// read how many data
			sockaddr_in sockaddrSource;
			int sockaddrLen = sizeof(sockaddr_in);
			memset(&sockaddrSource, 0, sockaddrLen);
			if ((valread = recvfrom(mSocket, (char *)recvbuffer, _SOCKET_PLUGIN_MAX_BUFFER, 0, (sockaddr*)&sockaddrSource, &sockaddrLen)) > 0) {
				//printf("Data: %s\n", recvbuffer);				

				ClientRecvPacket(recvbuffer, valread);
			}
		}
	}

	delete recvbuffer;
	mFShutdownCheck = true;
}

void UDPSocket::ServerRecvPacket(LinkInfo* pSourceInfo, BYTE *Data, unsigned int BufferSize)
{
	mAryRecvData.insert(mAryRecvData.end(), Data, Data + BufferSize);

	std::string patternStr = mPacketPattern;
	int patternLen = patternStr.length();
	char* pattern = new char[patternLen];
	memcpy_s(pattern, patternLen, patternStr.c_str(), patternLen);

	UnpackMessage(pSourceInfo, pattern, patternLen);

	delete pattern;
}

void UDPSocket::ClientRecvPacket(BYTE *Data, unsigned int BufferSize)
{
	mAryRecvData.insert(mAryRecvData.end(), Data, Data + BufferSize);

	std::string patternStr = mPacketPattern;
	int patternLen = patternStr.length();
	char* pattern = new char[patternLen];
	memcpy_s(pattern, patternLen, patternStr.c_str(), patternLen);

	UnpackMessage(&mLinkedServer, pattern, patternLen);

	delete pattern;
}

void UDPSocket::RecvDataCallback(DataPacket packet)
{
	packet.Data.push_back(0);
	std::string msg = packet.Data.data();
	std::cout << msg << std::endl;
}

void UDPSocket::PackMessage(std::string Message, char** ppData, long long& Len)
{
	std::string patternStr = mPacketPattern;
	long long patternLen = patternStr.length();
	long long messageLen = Message.length();
	char messageLenInChar[8] = { 0 };
	memcpy_s(messageLenInChar, sizeof(messageLenInChar), &messageLen, sizeof(messageLen));

	Len = patternLen + 8 + messageLen;
	*ppData = new char[patternLen + 8 + messageLen];
	memcpy_s(
		*ppData,
		Len,
		patternStr.c_str(),
		patternLen);
	memcpy_s(
		*ppData + patternLen,
		Len - patternLen,
		messageLenInChar,
		8);
	memcpy_s(
		*ppData + patternLen + 8,
		Len - patternLen - 8,
		Message.c_str(),
		messageLen);
}

void UDPSocket::UnpackMessage(LinkInfo* pSourceInfo, char* Pattern, int PatternLen)
{
	int bufferIndex = mAryRecvData.size();
	std::vector<int> indices = DatacmpIndex((char*)&mAryRecvData[0], mAryRecvData.size(), Pattern, PatternLen);

	if (mMapProcessedPacket.find(pSourceInfo) == mMapProcessedPacket.end()) {
		mMapProcessedPacket.insert(std::pair<LinkInfo*, std::vector<DataPacket>>(pSourceInfo, std::vector<DataPacket>()));
	}

	std::vector<DataPacket> *pAryProcessedPacket = &mMapProcessedPacket[pSourceInfo];

	if (indices.size() > 0)
	{
		// data before index: append on previous array
		if (pAryProcessedPacket->size() > 0)
		{
			int subBufferLen = indices[0];
			char* subBuffer = new char[subBufferLen];
			memcpy_s(subBuffer, subBufferLen, &mAryRecvData[0], subBufferLen);
			DataPacket packet = pAryProcessedPacket->at(pAryProcessedPacket->size() - 1);
			packet.Data.insert(packet.Data.end(), subBuffer, subBuffer + subBufferLen);
			packet.RecvCounter += subBufferLen;

			delete subBuffer;
		}

		// data after index: create a new array
		long remainDataIndex = 0;
		for (int i = 0; i < indices.size(); i++)
		{
			if (indices[i] + PatternLen + 8 > bufferIndex)
			{
				remainDataIndex = indices[i];
				break;
			}

			// pick data length
			char* dataLenInChar = new char[8];
			memcpy_s(dataLenInChar, 8, &mAryRecvData[0] + indices[i] + PatternLen, 8);
			long long dataLen = 0;
			memcpy_s(&dataLen, 8, dataLenInChar, 8);

			delete dataLenInChar;

			// pick data
			long indexBegin = indices[i] + PatternLen + 8;
			long long indexExpect = indexBegin + dataLen;
			long indexEnd = i + 1 >= indices.size() ? (indexExpect > bufferIndex ? bufferIndex : indexExpect) : indices[i + 1];
			long subBufferLen = indexEnd - indexBegin;
			char* subBuffer = new char[indexEnd - indexBegin];
			memcpy_s(subBuffer, subBufferLen, &mAryRecvData[0] + indexBegin, subBufferLen);

			DataPacket packet;
			packet.Data.insert(packet.Data.end(), subBuffer, subBuffer + subBufferLen);
			packet.Len = dataLen;
			packet.RecvCounter += subBufferLen;
			memcpy_s(&packet.Owner, sizeof(LinkInfo), pSourceInfo, sizeof(LinkInfo));

			delete subBuffer;

			pAryProcessedPacket->push_back(packet);
			remainDataIndex = indexEnd;
		}

		long remainBufferLen = bufferIndex - remainDataIndex;
		char* remainBuffer = new char[remainBufferLen];
		memcpy_s(remainBuffer, remainBufferLen, &mAryRecvData[0] + remainDataIndex, remainBufferLen);
		mAryRecvData.clear();
		mAryRecvData.insert(mAryRecvData.end(), remainBuffer, remainBuffer + remainBufferLen);
		bufferIndex = remainBufferLen;

		delete remainBuffer;
	}

	// receive data callback
	for (int i = 0; i < pAryProcessedPacket->size(); i++) {
		DataPacket packet = pAryProcessedPacket->at(i);
		if (packet.RecvCounter == packet.Len)
		{
			std::future<void> asyncResult = std::async(std::launch::async, &UDPSocket::RecvDataCallback, this, packet);

			pAryProcessedPacket->erase(pAryProcessedPacket->begin() + i);
			i--;
		}
	}
}

void UDPSocket::SendImplement(SOCKET s, const char* buf, long long len, int flags, sockaddr *target)
{
	sendto(s, buf, len, flags, target, sizeof(sockaddr));
}


std::vector<int> UDPSocket::DatacmpIndex(char* Data, int DataSize, char* Pattern, int PatternSize, int MaxCount /*= 0*/)
{
	std::vector<int> output;

	if (PatternSize == 0)
		return output;
	if (DataSize < PatternSize)
		return output;

	for (int i = 0; i < DataSize; i++)
	{
		bool isFound = false;
		if (Data[i] == Pattern[0])
		{
			isFound = true;
			for (int j = 0; j < PatternSize; j++)
			{
				if (i + j >= DataSize)
				{
					isFound = false;
					break;
				}
				if (Data[i + j] != Pattern[j])
				{
					isFound = false;
					break;
				}
			}
		}
		if (isFound)
		{
			output.push_back(i);
			if (MaxCount <= 0)
				continue;
			else
			{
				if (output.size() == MaxCount)
					break;
			}
		}
	}

	return output;
}

void UDPSocket::Clear()
{
#ifdef _UNREAL
	if (mServerLiveLoopWorker != nullptr) {
		mServerLiveLoopWorker->Stop();
		delete mServerLiveLoopWorker;
		mServerLiveLoopWorker = nullptr;
	}
	if (mClientLiveLoopWorker != nullptr) {
		mClientLiveLoopWorker->Stop();
		delete mClientLiveLoopWorker;
		mClientLiveLoopWorker = nullptr;
	}
#else
	if (mSocketType == _SOCKET_PLUGIN_TYPE_SERVER && mThreadServer.joinable()) {
		mThreadServer.join();
	}
	if (mSocketType == _SOCKET_PLUGIN_TYPE_CLIENT && mThreadClient.joinable()) {
		mThreadClient.join();
	}
#endif

	Shutdown();

	if (mSocket != 0) {
		closesocket(mSocket);
	}
	mSocket = 0;
	mSocketType = _SOCKET_PLUGIN_TYPE_NONE;
	memset(&mLocalAddr, 0, sizeof(sockaddr_in));
	memset(&mExternalAddr, 0, sizeof(sockaddr_in));
	mFIsThreadAlive = false;
	mFShutdown = false;
	mFShutdownCheck = false;
}

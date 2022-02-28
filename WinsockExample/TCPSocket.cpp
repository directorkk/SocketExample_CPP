#include "pch.h"

#include "TCPSocket.h"

TCPSocket::TCPSocket()
{
	mThreadClient = nullptr;
	mThreadServer = nullptr;

	mFShutdown = false;
	mFShutdownCheck = false;

	Clear();
}

TCPSocket::~TCPSocket()
{
	
}


SOCK_STATE TCPSocket::InitServer(unsigned int Port, unsigned int MaxClient, unsigned int MaxBufferSize)
{
	WSADATA wsaData;
	
	int wsaret = WSAStartup(0x101, &wsaData);
	if (wsaret != 0) {
		mAryStrLog.push_back("WSADATA error");

		return SOCK_ERROR;
	}

	mLocalAddr.sin_family = AF_INET;
	mLocalAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	mLocalAddr.sin_port = htons((u_short)Port);
	
	mSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (mSocket == INVALID_SOCKET) {
		mAryStrLog.push_back("Socket error");

		return SOCK_ERROR;
	}

	mLimitClient = MaxClient;
	mMaxBufferSize = MaxBufferSize;

	mSocketType = SOCK_TYPE_SERVER;
	return SOCK_OK;
}
SOCK_STATE TCPSocket::StartServer(unsigned int MaxListenBacklog)
{
	if ((bind(mSocket, (sockaddr*)&mLocalAddr, sizeof(mLocalAddr))) != 0) {
		mAryStrLog.push_back("Bind error");

		return SOCK_ERROR;
	}

	if (listen(mSocket, MaxListenBacklog) != 0) {
		mAryStrLog.push_back("Listen error");

		return SOCK_ERROR;
	}

	int opt = true;
	if (setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
		mAryStrLog.push_back("Setsockopt error");

		return SOCK_ERROR;
	}

	mAryStrLog.push_back("Waiting for connection...");

	//std::future<void> futureObj = mFShutdown.get_future();
	mThreadServer = new std::thread(&TCPSocket::StartServerThread, this/*, std::move(futureObj)*/);

	return SOCK_OK;
}
void TCPSocket::StartServerThread(/*std::future<void> FutureObj*/)
{
	mFIsThreadAlive = true;

	std::vector<DataPacket> listDataRecv;

	fd_set readfd;
	SOCKET maxfd;
	sockaddr_in cliaddr;
	int addrlen = sizeof(mLocalAddr);
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;	// 10ms

	char* sendbuffer = new char[mMaxBufferSize + 1];
	char* recvbuffer = new char[mMaxBufferSize + 1];
	memset(sendbuffer, 0, mMaxBufferSize + 1);
	memset(recvbuffer, 0, mMaxBufferSize + 1);

	while (!mFShutdown/*FutureObj.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout*/) {
		FD_ZERO(&readfd);
		FD_SET(mSocket, &readfd);
		maxfd = mSocket;

		// put multi clients in readfd set for select
		for (int i = 0; i < mArySocketClients.size(); i++) {
			SOCKET fd = mArySocketClients.at(i).Sock;

			if (fd > 0) {
				FD_SET(fd, &readfd);
			}
			if (fd > maxfd) {
				maxfd = fd;
			}
		}

		// max file descripter+1, readset, writeset, expectset, timeout
		int activity = select((int)maxfd + 1, &readfd, NULL, NULL, &timeout);
		if ((activity < 0) && (errno != EINTR)) {
			mAryStrLog.push_back("Select error.");
		}

		if (FD_ISSET(mSocket, &readfd)) {
			SOCKET connect = accept(mSocket, (sockaddr*)&cliaddr, &addrlen);
			if (connect < 0) {
				mAryStrLog.push_back("Accept error.");
				continue;
			}
			wchar_t ip[16] = {0};
			InetNtop(cliaddr.sin_family, &cliaddr.sin_addr.S_un.S_addr, ip, sizeof(ip));
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			std::string rtn = converter.to_bytes(ip);
			std::cout << "New connection, fd: " << connect
				<< ", IP: " << rtn
				<< ", port: " << ntohs(cliaddr.sin_port) << std::endl;
			
			//sprintf_s(sendbuffer, mMaxBufferSize, "Welcome to the test server.");
			//send(connect, sendbuffer, (int)strlen(sendbuffer), 0);

			ClientInf clientInf(connect, std::string(""));
			mArySocketClients.push_back(clientInf);

			std::vector<ClientInf> tmpAryClientInf;
			Send("Welcome to the test server.", tmpAryClientInf);
		}

		for (int i = 0; i < mArySocketClients.size(); i++) {
			SOCKET fd = mArySocketClients.at(i).Sock;

			if (FD_ISSET(fd, &readfd)) {
				int countRead = 0;	// read how many data

				if ((countRead = recv(fd, recvbuffer, mMaxBufferSize, 0)) <= 0) {	// someone disconnected
					getpeername(fd, (sockaddr*)&cliaddr, &addrlen);
					std::cout << "disonnection, fd: " << fd
						<< ", IP: " << inet_ntoa(cliaddr.sin_addr)
						<< ", port: " << ntohs(cliaddr.sin_port) << std::endl;

					shutdown(fd, SD_SEND);

					mArySocketClients.erase(mArySocketClients.begin() + i);
					i--;
				}
				else {
					// ======== do something when recieved a line ========
					//strcpy_s(sendbuffer, mMaxBufferSize, recvbuffer);
					//send(fd, sendbuffer, strlen(sendbuffer), 0);
					std::lock_guard<std::mutex> lock(mMutexThreadServer);

					mAryRecvMsg.insert(mAryRecvMsg.end(), recvbuffer, recvbuffer + countRead);

					char patternStr[] = "DATA_START";
					int patternLen = sizeof(patternStr) - 1;
					char* pattern = new char[patternLen];
					memcpy_s(pattern, patternLen, patternStr, patternLen);
					UnpackMessage(listDataRecv, pattern, patternLen);

					delete pattern;

					// receive data callback
					for (int i = 0; i < listDataRecv.size(); i++) {
						DataPacket packet = listDataRecv[i];
						if (packet.RecvCounter == packet.Len)
						{
							std::future<void> asyncResult = std::async(std::launch::async, &TCPSocket::RecvDataCallback, this, packet);
							//listRecvDataCallbackResult.push_back(asyncResult);

							listDataRecv.erase(listDataRecv.begin() + i);
							i--;
						}
					}
					// ===================================================
				}
				memset(recvbuffer, 0, mMaxBufferSize + 1);
			}
		}
	}

	mFShutdownCheck = true;
	std::cout << "Server thread shutdown";
	mAryStrLog.push_back("Server thread shutdown");
}

SOCK_STATE TCPSocket::InitClient(std::string IPAddress, unsigned int Port, unsigned int MaxBufferSize)
{
	WSADATA wsaData;

	int wsaret = WSAStartup(0x202, &wsaData);
	if (wsaret != 0) {
		mAryStrLog.push_back("WSADATA error");

		return SOCK_ERROR;
	}
	
	hostent *hostinfo = gethostbyname(IPAddress.c_str());

	mServerAddr.sin_family = AF_INET;
	mServerAddr.sin_port = htons(Port);
	if (hostinfo == NULL) {
		mAryStrLog.push_back("Host set error");

		return SOCK_ERROR;
	}
	mServerAddr.sin_addr = *(struct in_addr *) hostinfo->h_addr;

	mSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (mSocket == INVALID_SOCKET) {
		mAryStrLog.push_back("Socket error");

		return SOCK_ERROR;
	}

	mMaxBufferSize = MaxBufferSize;
	mSocketType = SOCK_TYPE_CLIENT;

	return SOCK_OK;
}

SOCK_STATE TCPSocket::InitUdpServer(unsigned int Port)
{
	WSADATA wsaData;

	int wsaret = WSAStartup(0x101, &wsaData);
	if (wsaret != 0) {
		mAryStrLog.push_back("WSADATA error");

		return SOCK_ERROR;
	}

	mLocalAddr.sin_family = AF_INET;
	mLocalAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	mLocalAddr.sin_port = htons((u_short)Port);

	mSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (mSocket == INVALID_SOCKET) {
		mAryStrLog.push_back("Socket error");

		return SOCK_ERROR;
	}

	mSocketType = SOCK_TYPE_SERVER;
	return SOCK_OK;
}

SOCK_STATE TCPSocket::InitUdpClient(std::string IPAddress, unsigned int Port)
{
	WSADATA wsaData;

	int wsaret = WSAStartup(0x202, &wsaData);
	if (wsaret != 0) {
		mAryStrLog.push_back("WSADATA error");

		return SOCK_ERROR;
	}

	hostent *hostinfo = gethostbyname(IPAddress.c_str());

	mServerAddr.sin_family = AF_INET;
	mServerAddr.sin_port = htons(Port);
	if (hostinfo == NULL) {
		mAryStrLog.push_back("Host set error");

		return SOCK_ERROR;
	}
	mServerAddr.sin_addr = *(struct in_addr *) hostinfo->h_addr;

	mSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (mSocket == INVALID_SOCKET) {
		mAryStrLog.push_back("Socket error");

		return SOCK_ERROR;
	}

	mSocketType = SOCK_TYPE_CLIENT;

	return SOCK_OK;
}

SOCK_STATE TCPSocket::StartClient()
{
	if (connect(mSocket, (sockaddr*)&mServerAddr, sizeof(mServerAddr)) < 0) {
		mAryStrLog.push_back("Connect error");

		return SOCK_ERROR;
	}

	mAryStrLog.push_back("=== Connect success ===");

	//std::future<void> futureObj = mFShutdown.get_future();
	mThreadServer = new std::thread(&TCPSocket::StartClientThread, this/*, std::move(futureObj)*/);

	return SOCK_OK;
}

void TCPSocket::StartClientThread(/*std::future<void> FutureObj*/)
{
	mFIsThreadAlive = true;

	std::vector<DataPacket> listDataRecv;

	char* recvbuffer = new char[mMaxBufferSize + 1];
	memset(recvbuffer, 0, mMaxBufferSize + 1);

	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;	// 10ms

	while (!mFShutdown/*FutureObj.wait_for(std::chrono::microseconds(1)) == std::future_status::timeout*/) {		
		int countRead = 0;
		try
		{
#ifndef WIN32 
			signal(SIGPIPE, SIG_IGN);
#endif 
			fd_set readfd;
			FD_ZERO(&readfd);
			FD_SET(mSocket, &readfd);

			int activity = select((int)mSocket, &readfd, NULL, NULL, &timeout);
			if ((activity < 0) && (errno != EINTR)) {
				mAryStrLog.push_back("Select error.");
			}

			if (FD_ISSET(mSocket, &readfd)) {
				if ((countRead = recv(mSocket, recvbuffer, 256, 0)) > 0) {
					std::lock_guard<std::mutex> lock(mMutexThreadServer);

					mAryRecvMsg.insert(mAryRecvMsg.end(), recvbuffer, recvbuffer + countRead);

					char patternStr[] = "DATA_START";
					int patternLen = sizeof(patternStr) - 1;
					char* pattern = new char[patternLen];
					memcpy_s(pattern, patternLen, patternStr, patternLen);
					//UnpackMessage(listDataRecv, pattern, patternLen);

					delete pattern;

					// receive data callback
					for (int i = 0; i < listDataRecv.size(); i++) {
						DataPacket packet = listDataRecv[i];
						if (packet.RecvCounter == packet.Len)
						{
							std::future<void> asyncResult = std::async(std::launch::async, &TCPSocket::RecvDataCallback, this, packet);
							//listRecvDataCallbackResult.push_back(asyncResult);

							listDataRecv.erase(listDataRecv.begin() + i);
							i--;
						}
					}

				}
				memset(recvbuffer, 0, mMaxBufferSize + 1);
			}
		}
		catch (...)
		{
			countRead = SOCKET_ERROR;
		}

		if (countRead < 0)
		{
			// ERROR
			countRead = 0;
		}
	}

	mFShutdownCheck = true;
	std::cout << "Client thread shutdown";
	mAryStrLog.push_back("Client thread shutdown");
}

void TCPSocket::Clear()
{
	Shutdown();

	if (mSocket != 0) {
		closesocket(mSocket);
	}

	mSocket = 0;
	mSocketType = SOCK_TYPE_NONE;
	memset(&mLocalAddr, 0, sizeof(sockaddr_in));
	memset(&mServerAddr, 0, sizeof(sockaddr_in));
	mMaxBufferSize = 0;
	mArySocketClients.clear();
	mLimitClient = 0;
	mAryStrLog.clear();
	mFIsThreadAlive = false;
	mFShutdown = false;
	mFShutdownCheck = false;
}

SOCK_STATE TCPSocket::Pause()
{
	return SOCK_OK;
}

SOCK_STATE TCPSocket::Shutdown()
{	
	if (mFIsThreadAlive) {
		//mFShutdown.set_value();
		mFShutdown = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));

		if (mSocketType == SOCK_TYPE_SERVER && mThreadServer->joinable()) {
			mThreadServer->join();
			delete mThreadServer;
		}
		if (mSocketType == SOCK_TYPE_CLIENT && mThreadServer->joinable()) {
			mThreadServer->join();
			delete mThreadClient;
		}
		closesocket(mSocket);
		shutdown(mSocket, SD_SEND);
		shutdown(mSocket, SD_RECEIVE);
	}

	return SOCK_OK;
}

SOCKET TCPSocket::GetSocket()
{
	return mSocket;
}

SOCK_STATE TCPSocket::GetSocketType()
{
	return mSocketType;
}

void TCPSocket::SendToServer(char* Data, long long DataLen)
{
	SendImplement(mSocket, Data, DataLen, 0);
}

void TCPSocket::SendToClient(char* Data, long long DataLen, std::vector<ClientInf> Targets)
{
	std::vector<ClientInf>* pArySocketClients = &Targets;
	if (Targets.size() == 0) {
		pArySocketClients = &mArySocketClients;
	}
	for (int i = 0; i < pArySocketClients->size(); i++) {
		SOCKET client = pArySocketClients->at(i).Sock;
		SendImplement(client, Data, DataLen, 0);
	}
}
void TCPSocket::PackMessage(std::string Message, char** ppData, long long& Len)
{
	std::string pattern = "DATA_START";
	long long patternLen = pattern.length();
	long long messageLen = Message.length();
	char messageLenInChar[8] = { 0 };
	memcpy_s(messageLenInChar, sizeof(messageLenInChar), &messageLen, sizeof(messageLen));

	Len = patternLen + 8 + messageLen;
	*ppData = new char[patternLen + 8 + messageLen];
	memcpy_s(
		*ppData, 
		Len,
		pattern.c_str(),
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

void TCPSocket::UnpackMessage(std::vector<DataPacket> &listDataRecv, char* pattern, int patternLen)
{
	int bufferIndex = mAryRecvMsg.size();
	std::vector<int> indices = DatacmpIndex(&mAryRecvMsg[0], mAryRecvMsg.size(), pattern, patternLen);

	if (indices.size() > 0)
	{
		// data before index: append on previous array
		if (listDataRecv.size() > 0)
		{
			int subBufferLen = indices[0];
			char* subBuffer = new char[subBufferLen];
			memcpy_s(subBuffer, subBufferLen, &mAryRecvMsg[0], subBufferLen);
			DataPacket packet = listDataRecv[listDataRecv.size() - 1];
			packet.Data.insert(packet.Data.end(), subBuffer, subBuffer + subBufferLen);
			packet.RecvCounter += subBufferLen;

			delete subBuffer;
		}

		// data after index: create a new array
		long remainDataIndex = 0;
		for (int i = 0; i < indices.size(); i++)
		{
			if (indices[i] + patternLen + 8 > bufferIndex)
			{
				remainDataIndex = indices[i];
				break;
			}

			// pick data length
			char* dataLenInChar = new char[8];
			memcpy_s(dataLenInChar, 8, &mAryRecvMsg[0] + indices[i] + patternLen, 8);
			long long dataLen = 0;
			memcpy_s(&dataLen, 8, dataLenInChar, 8);

			delete dataLenInChar;

			// pick data
			long indexBegin = indices[i] + patternLen + 8;
			long long indexExpect = indexBegin + dataLen;
			long indexEnd = i + 1 >= indices.size() ? (indexExpect > bufferIndex ? bufferIndex : indexExpect) : indices[i + 1];
			long subBufferLen = indexEnd - indexBegin;
			char* subBuffer = new char[subBufferLen];
			memset(subBuffer, 0, subBufferLen);
			memcpy_s(subBuffer, subBufferLen, &mAryRecvMsg[0] + indexBegin, subBufferLen);

			DataPacket packet;
			packet.Data = std::vector<char>();
			packet.Data.insert(packet.Data.end(), subBuffer, subBuffer + subBufferLen);
			packet.Len = dataLen;
			packet.RecvCounter += subBufferLen;

			delete subBuffer;

			listDataRecv.push_back(packet);
			remainDataIndex = indexEnd;
		}

		long remainBufferLen = bufferIndex - remainDataIndex;
		char* remainBuffer = new char[remainBufferLen];
		memcpy_s(remainBuffer, remainBufferLen, &mAryRecvMsg[0] + remainDataIndex, remainBufferLen);
		mAryRecvMsg.clear();
		mAryRecvMsg.insert(mAryRecvMsg.end(), remainBuffer, remainBuffer + remainBufferLen);
		bufferIndex = remainBufferLen;

		delete remainBuffer;
	}
}

std::vector<int> TCPSocket::DatacmpIndex(char* Data, int DataSize, char* Pattern, int PatternSize, int MaxCount /*= 0*/)
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

void TCPSocket::RecvDataCallback(DataPacket packet)
{
	packet.Data.push_back(0);
	std::string msg = packet.Data.data();
	std::cout << msg << std::endl;
}

void TCPSocket::SendImplement(SOCKET s, const char * buf, long long len, int flags)
{
	send(s, buf, (int)len, flags);
}

void TCPSocket::Send(std::string Message, std::vector<ClientInf> Targets)
{
	char* packedMsg = nullptr;
	long long packedMshLen;
	PackMessage(Message, &packedMsg, packedMshLen);

	switch (mSocketType)
	{
	case SOCK_TYPE_NONE:
		break;
	case SOCK_TYPE_SERVER:
		SendToClient(packedMsg, packedMshLen, Targets);
		break;
	case SOCK_TYPE_CLIENT:
		SendToServer(packedMsg, packedMshLen);
		break;
	default:
		break;
	}

	if (packedMsg != nullptr) {
		delete packedMsg;
	}
}


std::vector<char> TCPSocket::FetchData()
{
	std::lock_guard<std::mutex> lock(mMutexThreadServer);

	std::vector<char> rtn;
	rtn.resize(mAryRecvMsg.size());
	std::copy(mAryRecvMsg.begin(), mAryRecvMsg.end(), rtn.begin());

	mAryRecvMsg.clear();

	return rtn;
}

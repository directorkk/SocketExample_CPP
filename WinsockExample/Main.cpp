#include "pch.h"
#include "TCPSocket.h"
//#include "UdpSocket.h"



int main()
{
	TCPSocket* sock = new TCPSocket();
	//UDPSocket *sock = new UDPSocket();
	
	sock->InitServer(20255);
	sock->StartServer();
	//sock->InitClient("127.0.0.1", 20255);
	//sock->StartClient();


	int i = 0;
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		// receive
		std::vector<char> msg = sock->FetchData();
		if (msg.size() > 0) {
			std::string strMsg(msg.begin(), msg.end());
			std::cout << strMsg.c_str() << std::endl;
		}

		// send
		//sock->Send("TEST");		
	}
	
	delete sock;

	return 0;
}


#include "RoboCatPCH.h"

#include <thread>
#include <iostream>
#include <string>
#include <sstream>
#include <mutex>

// Problem: Game Loop
//
// updateInput(); (make sure to not block here!)
//		conn->Receive(); !!! This blocks !!!
//			Two solutions:
//				- Non-Blocking Mode
//					On Receive(), check for -10035; that means "nothings wrong, but I didn't receive any data either"
// update();
// render();
// goto beginning;

bool shouldQuit = false; //global quit
//typedef list <std::pair<SocketAddress, std::string>> ClientList;
//ClientList clientlist;

std::pair<std::string, SocketAddress> userName;

std::string currentDateAndTime() 
{
	//https://en.cppreference.com/w/cpp/chrono/c/strftime
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now); // not the best but it works
	
	strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

	return buf;
}

void DoTcpServer()
{
	// Create socket
	TCPSocketPtr listenSocket = SocketUtil::CreateTCPSocket(SocketAddressFamily::INET);
	if (listenSocket == nullptr)
	{
		SocketUtil::ReportError("Creating listening socket");
		ExitProcess(1);
	}

	//listenSocket->SetNonBlockingMode(true);

	LOG("%s", "Listening socket created");

	// Bind() - "Bind" socket -> tells OS we want to use a specific address

	SocketAddressPtr listenAddress = SocketAddressFactory::CreateIPv4FromString("0.0.0.0:8080");
	if (listenAddress == nullptr)
	{
		SocketUtil::ReportError("Creating listen address");
		ExitProcess(1);
	}

	if (listenSocket->Bind(*listenAddress) != NO_ERROR)
	{
		SocketUtil::ReportError("Binding listening socket");
		// This doesn't block!
		ExitProcess(1);
	}

	LOG("%s", "Bound listening socket");

	// Blocking function call -> Waits for some input; halts the program until something "interesting" happens
	// Non-Blocking function call -> Returns right away, as soon as the action is completed

	// Listen() - Listen on socket -> Non-blocking; tells OS we care about incoming connections on this socket
	if (listenSocket->Listen() != NO_ERROR)
	{
		SocketUtil::ReportError("Listening on listening socket");
		ExitProcess(1);
	}

	LOG("%s", "Listening on socket");

	// Accept() - Accept on socket -> Blocking; Waits for incoming connection and completes TCP handshake

	LOG("%s", "Waiting to accept connections...");
	SocketAddress incomingAddress;
	TCPSocketPtr connSocket = listenSocket->Accept(incomingAddress);
	while (connSocket == nullptr)
	{
		connSocket = listenSocket->Accept(incomingAddress);
		// SocketUtil::ReportError("Accepting connection");
		// ExitProcess(1);
	}

	LOG("Accepted connection from %s", incomingAddress.ToString().c_str());
	// receive initial connected client username
	std::string defaultName = "anon";
	char buffer[4096];
	int32_t bytesReceived = connSocket->Receive(buffer, 4096);
	std::string receivedUsername(buffer, bytesReceived);

	if (bytesReceived == 0)
	{
		receivedUsername = defaultName; //if no input, default to 'anon'
	}

	std::cout << "Enter a username: ";
	std::string clientUsername;
	std::getline(std::cin, clientUsername);
	connSocket->Send(clientUsername.c_str(), clientUsername.length()); // Send server username
	std::cout << "\033[2J\033[1;1H";


	userName = std::make_pair(receivedUsername,incomingAddress);
	std::cout << incomingAddress.ToString() << " sets username to: " << userName.first << "\n";
	//bool quit = false;
	std::thread receiveThread([&connSocket, &incomingAddress]() // don't use [&] :)
	{ 
		while (!shouldQuit)
		{
			char buffer[4096];
			int32_t bytesReceived = connSocket->Receive(buffer, 4096);
			if (bytesReceived <= 0) // Handle connection terminated
			{
				std::cout << "Client '" << userName.first << "' disconnected. Exiting.\n\n";
				shouldQuit = true;
				connSocket->CloseSocket();
				connSocket = nullptr; 
				break;
			}
			if (bytesReceived < 0)
			{
				SocketUtil::ReportError("Receiving");
				return;
			}

			std::string receivedMsg(buffer, bytesReceived);
			std::cout << "[" << currentDateAndTime() << "] " << userName.first << " says: "<< receivedMsg << "\n\n";
		}
		return;
	});

	std::thread sendMessage([&]()
		{
			while (!shouldQuit)
			{

				std::string messageContent;
				std::cout << "Message: ";
				std::getline(std::cin, messageContent);

				//if (messageContent == "/exit") // check for exit command
				//{
				//	std::cout << "Disconnecting from the server...\n";
				//	clientSocket->CloseSocket();
				//	shouldQuit = true;
				//	return;
				//}
				connSocket->Send(messageContent.c_str(), messageContent.length());
			}
			connSocket->CloseSocket();
			return;
		});
	sendMessage.join();
	receiveThread.join();
}

void DoTcpClient(std::string port)
{
	// Create socket
	TCPSocketPtr clientSocket = SocketUtil::CreateTCPSocket(SocketAddressFamily::INET);
	if (clientSocket == nullptr)
	{
		SocketUtil::ReportError("Creating client socket");
		ExitProcess(1);
	}

	LOG("%s", "Client socket created");

	// Bind() - "Bind" socket -> tells OS we want to use a specific address

	std::string address = StringUtils::Sprintf("127.0.0.1:%s", port.c_str());
	SocketAddressPtr clientAddress = SocketAddressFactory::CreateIPv4FromString(address.c_str());
	if (clientAddress == nullptr)
	{
		SocketUtil::ReportError("Creating client address");
		ExitProcess(1);
	}

	if (clientSocket->Bind(*clientAddress) != NO_ERROR)
	{
		SocketUtil::ReportError("Binding client socket");
		// This doesn't block!
		ExitProcess(1);
	}

	LOG("%s", "Bound client socket");

	// Connect() -> Connect socket to remote host

	SocketAddressPtr servAddress = SocketAddressFactory::CreateIPv4FromString("127.0.0.1:8080");
	if (servAddress == nullptr)
	{
		SocketUtil::ReportError("Creating server address");
		ExitProcess(1);
	}


	if (clientSocket->Connect(*servAddress) != NO_ERROR)
	{
		SocketUtil::ReportError("Connecting to server");
		ExitProcess(1);
	}

	//Set client username prior to joining server
	std::cout << "Enter a username: ";
	std::string clientUsername;
	std::getline(std::cin, clientUsername);
	clientSocket->Send(clientUsername.c_str(), clientUsername.length()); // Send client username
	std::cout << "\033[2J\033[1;1H";

	std::cout << "Connected to the server! Welcome, " << clientUsername << "!\n";
	//while (true)												//Used for early testing of send/receive
	//{
	//	std::string msg("Hello server! How are you?");
	//	clientSocket->Send(msg.c_str(), msg.length());
	//	std::this_thread::sleep_for(std::chrono::seconds(1));
	//}

	std::thread sendMessage([&]()
	{
		while (!shouldQuit)
		{

			std::string messageContent;
			std::cout << "Message: ";
			std::getline(std::cin, messageContent);

			//if (messageContent == "/exit") // check for exit command
			//{
			//	std::cout << "Disconnecting from the server...\n";
			//	clientSocket->CloseSocket();
			//	shouldQuit = true;
			//	return;
			//}
			clientSocket->Send(messageContent.c_str(), messageContent.length());
		}
		clientSocket->CloseSocket();
		return;
	});

	std::thread receiveThread([&clientSocket]() // don't use [&] :)
		{
			while (!shouldQuit)
			{
				char buffer[4096];
				int32_t bytesReceived = clientSocket->Receive(buffer, 4096);
				if (bytesReceived <= 0) // Handle connection terminated
				{
					std::cout << "Client '" << userName.first << "' disconnected. Exiting.\n\n";
					shouldQuit = true;
					clientSocket->CloseSocket();
					clientSocket = nullptr;
					break;
				}
				if (bytesReceived < 0)
				{
					SocketUtil::ReportError("Receiving");
					return;
				}

				std::string receivedMsg(buffer, bytesReceived);
				std::cout << "[" << currentDateAndTime() << "] " << userName.first << " says: " << receivedMsg << "\n\n";
			}
			return;
		});

	sendMessage.join();
}

#if _WIN32
int main(int argc, const char** argv)
{
	UNREFERENCED_PARAMETER(argc);
	UNREFERENCED_PARAMETER(argv);
#else
const char** __argv;
int __argc;
int main(int argc, const char** argv)
{
	__argc = argc;
	__argv = argv;
#endif

	// WinSock2.h
	//    https://docs.microsoft.com/en-us/windows/win32/api/winsock/


	SocketUtil::StaticInit();

	bool isServer = StringUtils::GetCommandLineArg(1) == "server";

	if (isServer)
	{
		// Server code ----------------
		//		Want P2P -- we'll get to that :)
		DoTcpServer();
	}
	else
	{
		// Client code ----------------
		DoTcpClient(StringUtils::GetCommandLineArg(2));
	}

	SocketUtil::CleanUp();

	return 0;
}

//Tic Tac Toe
//User1 connects. Waits for User2 to connect. Tic Tac Toe plays. 
//If User3 connects, wait for a User 4 to connect. Pair up matching players as they connect
//Want to be able to handle multiple games at once

#include <iostream>
#include <stdio.h>
#include <string.h> //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h> //close
#include <arpa/inet.h> //close
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros

#define TRUE 1
#define FALSE 0
#define PORT 8888
#define MAXCLIENTS 10

struct Game
{
	int playerOne;
	int playerTwo;
	int moveTurn; //1 if playerOnes turn, 2 if playerTwos turn
	int board[9];
};

void printGames(Game games[]);
bool othersockets(Game g, int valr, struct sockaddr_in addre, int adrlen);

int main(int argc, char *argv[])
{
	int opt = TRUE;
	int master_socket, addrlen, new_socket, activity, i, valread, sd;
	int max_sd;
	struct sockaddr_in address;
	Game games[MAXCLIENTS]; //Used to keep track of which players are paired up together

	//Set to True if we have an odd number of players which would mean someone is waiting
	//for a game, false otherwise
	bool playerWaiting = false;

	const char *message;

	//Data Buffer for receivig incoming moves
	char buffer[1025]; //Only need 2 characters. Example move is 12 - first row second column
	
	//set of socket descriptors
	fd_set readfds;

	//Welcome message
	const char *welcome = "Welcome to Tic Tac Toe!\r\n";

	//Initialize all client sockets and boards to 0
	for(i = 0; i < MAXCLIENTS; i++)
	{
		games[i].playerOne = -1;
		games[i].playerTwo = -1;
		games[i].moveTurn = 1;
		for(int j = 0; j < MAXCLIENTS; j++)
		{
			games[i].board[j] = 0;
		}
	}

	//Create a master socket
	//AF_INTE is ipv4, SOCK_STREAM is TCP
	if((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	//Set master socket to allow multiple connections
	//This helps in manipulating options for the socket referred
	//by the file descriptor sockfd. This is completely optional, 
	//but it helps in reuse of address and port. Prevents error 
	//such as: “address already in use”.
	if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	//Type of Socket Created
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );
	
	//Bind the socet to local host port
	//After creation of the socket, bind function binds the 
	//socket to the address and port number specified in 
	//addr(custom data structure). 
	if(bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	std::cout << "Listener on port: " << PORT << std::endl;

	//Try to specify maximum of 5 pending connections for the master socket
	//It puts the server socket in a passive mode, where it waits 
	//for the client to approach the server to make a connection.
	//The backlog, defines the maximum length to which the queue
	//of pending connections for sockfd may grow. 
	//If a connection request arrives when the
	//queue is full, the client may receive an error with an indication
	//of ECONNREFUSED
	if(listen(master_socket, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	//Accept the incoming connection
	addrlen = sizeof(address);
	puts("Waiting for connections ...");
	
	while(TRUE)
	{
		//Clear the socket set
		FD_ZERO(&readfds);
		
		//Add master socket to set
		FD_SET(master_socket, &readfds);
		max_sd = master_socket;

		//Add player one child sockets to set
		for(i = 0; i < MAXCLIENTS; i++)
		{
			//Socket Descriptor
			sd = games[i].playerOne;

			//If valid socket descriptor then add to read list
			if(sd > 0)
				FD_SET(sd, &readfds);

			//Highest file descriptor number, needed for select function
			if(sd > max_sd)
				max_sd = sd;
		}		

		//Add player two child sockets to set
		for(i = 0; i < MAXCLIENTS; i++)
		{
			//Socket Descriptor
			sd = games[i].playerTwo;

			//If valid socket descriptor then add to read list
			if(sd > 0)
				FD_SET(sd, &readfds);

			//Highest file descriptor number, needed for select function
			if(sd > max_sd)
				max_sd = sd;
		}

		//Wait for an activity on one of the sockets. Timeout is NULL
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
		if((activity < 0) && (errno != EINTR))
		{
			std::cout << "select error" << std::endl;
		}

		//If something happened on he master socket then it is an incoming connection
		if(FD_ISSET(master_socket, &readfds))
		{
			
			if((new_socket = accept(master_socket,
						(struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
			{
				perror("accept");
				exit(EXIT_FAILURE);
			}

			std::cout << "New connection, socket fd is: " << new_socket << ", ip is: " <<
				inet_ntoa(address.sin_addr) << ", port is: " <<
				ntohs (address.sin_port) << std::endl;

			//Send new connection greeting message
			if(send(new_socket, welcome, strlen(welcome), 0) != strlen(welcome))
			{
				perror("send");
			}
			puts("Welcome message sent successfully");

			//Set up their game
			if(!playerWaiting)
			{
				for(i = 0; i < MAXCLIENTS; i++)
				{
					if(games[i].playerOne == -1)
					{
						games[i].playerOne = new_socket;
						break;
					}
				}
				playerWaiting = true;
				message = "Waiting for an opponent...\r\n";
				send(new_socket, message, strlen(message), 0);
			}
			else //Pair them up with someone who is waiting
			{
				int newPlayerIndex;
				for(i = 0; i < MAXCLIENTS; i++)
				{
					if(games[i].playerOne != -1)
					{
						games[i].playerTwo = new_socket;
						newPlayerIndex = i;
						break;
					}
					//Someone may have disconnected mid game which could cause this
					//to be necessary. Otherwise they could sit without an opponent forever. 
					else if(games[i].playerTwo != -1)
					{
						games[i].playerOne = new_socket;
						newPlayerIndex = i;
						break;
					}
				}	
				//We can start the game. Send them the current board
				playerWaiting = false;
				message = "Opponent found! Starting game...\r\n";
				send(games[newPlayerIndex].playerOne, message, strlen(message), 0);
				send(games[newPlayerIndex].playerTwo, message, strlen(message), 0);
			}
		}

		//Else its some IO operation on some other socket
		//This means players in a current game are making a move
		for(i = 0; i < MAXCLIENTS; i++)
		{	
			playerWaiting = othersockets(games[i], valread, address, addrlen);
		}
	}
	return 0;
}

//Returns new value for player waiting if someone disconnects
bool othersockets(Game g, int valr, struct sockaddr_in addre, int adrlen)
{
	bool pw = false;
	int *socDesc;
	int *oppSocDesc;
	char buf[1025];
	for(int i = 0; i <= 1; i++)
	{
		if(i == 0)
		{
			socDesc = &g.playerOne;
			oppSocDesc = &g.playerTwo;
		}
		else
		{
			socDesc = &g.playerTwo;
			oppSocDesc = &g.playerOne;
		}
		//Check if it was for closing and also read the incoming message
		if((valr = read(*socDesc, buf, 1024)) == 0)
		{
			//Somebody disconnected, get details and print
			getpeername(*socDesc, (struct sockaddr*)&addre, \
				(socklen_t*)&adrlen);
			std::cout << "Host disconnected, ip: " << inet_ntoa(addre.sin_addr) <<
			"port: " << ntohs(addre.sin_port) << std::endl;

			//Inform their opponent that they left
			const char *msg = "Your opponent has disconnected. You win!\nWaiting for another player...\r\n";
			send(*oppSocDesc, msg, strlen(msg), 0);
			pw = true;


			//Close the socket and mark as 0 in list for reuse
			close(*socDesc);
			if(*socDesc == g.playerOne)
			{
				close(g.playerOne);
				g.playerOne = -1;
			}
			else
			{
				close(g.playerTwo);
				g.playerTwo = -1;
			}
		}
		else
		{

		}
	}
	return pw;
}

int getIndexOfOpponent(int curSocket, Game games[], int cs[], int ng, int nc)
{
	return 0;
}

void printConnections(int cs[])
{
	std::cout << "Current Connections" << std::endl;
	for(int i = 0; i < MAXCLIENTS; i++)
	{
		std::cout << i << ": " << cs[i] << std::endl;
	}
}

void printGames(Game games[])
{
	std::cout << "Current Games" << std::endl;
	for(int i = 0; i < MAXCLIENTS; i++)
	{
		std::cout << i << std::endl;
		std::cout << "indexOne: " << games[i].playerOne << std::endl;
		std::cout << "indexTwo: " << games[i].playerTwo << std::endl;
		std::cout << std::endl;
	}
}









































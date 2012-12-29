//============================================================================
// Name        : drawdown_server.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <mysql/mysql.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <poll.h>

#define TRUE   1
#define FALSE  0
#define PORT 8889
#define BUFFSIZE 255

using namespace std;

struct sockaddr_in address;
int master_socket;
bool server_online = false;
int test_user_count = 1;

struct player{
	int socket;
	int database_id;
	bool online;
	string first_name;
	string profile_picture;
	int points;
	int wins;
	int diamonds;
	int sos;
};
struct request{
	int socket;
	player* player_info;
	string action;
	vector<string> token;
};
vector<request> database_requests;
vector<request> game_requests;

vector<player*> easy_queue;
vector<player*> medium_queue;
vector<player*> hard_queue;
vector<player> players;


void display_msg(string from, string msg) {
	cout << "[" << from << "] " << msg << endl;
}
void* connectionThread(void *) {
	display_msg("CONNECTION", "Waiting for clients...");
	int addrlen = sizeof(address);
	vector<pollfd> poll_sockets;
	pollfd main_poll;
	main_poll.fd = master_socket;
	main_poll.events = POLLIN;
	poll_sockets.push_back(main_poll);

	pollfd *poll_sockets_ptr;
	int num_of_poll_sockets;
	int poll_activity;
	while (server_online) {
		poll_sockets_ptr = &poll_sockets[0];
		num_of_poll_sockets = poll_sockets.size();
		poll_activity = poll(poll_sockets_ptr, num_of_poll_sockets, 0);
		if (poll_activity < 0) {
			perror("poll");
		}
		if (poll_sockets[0].revents & POLLIN) {
			int new_socket;
			if ((new_socket = accept(master_socket, (struct sockaddr*) &address,
					(socklen_t*) &addrlen)) < 0) {
				perror("accept");
			} else {
				pollfd add_socket;
				add_socket.fd = new_socket;
				add_socket.events = POLLIN;
				poll_sockets.push_back(add_socket);
				num_of_poll_sockets = poll_sockets.size();
				display_msg("CONNECTION", "Client connected!");
				cout << "HOST IP Address: " << inet_ntoa(address.sin_addr)
						<< ":" << ntohs(address.sin_port) << endl;
			}
		}
		for (int i = 0; i < num_of_poll_sockets; i++) {
			int s = poll_sockets[i].fd;
			if (i != 0) {
				if (poll_sockets[i].revents & POLLIN) {
					char buffer[BUFFSIZE];
					if ((read(s, buffer, BUFFSIZE)) == 0) {
						display_msg("CONNECTION", "Client disconnected!");
						cout << "HOST IP Address: "
								<< inet_ntoa(address.sin_addr) << ":"
								<< ntohs(address.sin_port) << endl;
						poll_sockets.erase(poll_sockets.begin() + i);
					} else {
						char *msg_tok;
						vector<string> client_messages;
						int num_of_messages;
						msg_tok = strtok(buffer, ";");
						while (msg_tok != NULL) {
							client_messages.push_back(msg_tok);
							num_of_messages = client_messages.size();
							msg_tok = strtok(NULL, ";");
						}
						if(num_of_messages == 4){
							if(client_messages[0] == "LOGIN"){
								request add_request;
								add_request.action = client_messages[0];
								add_request.token.push_back(client_messages[1]);
								add_request.token.push_back(client_messages[2]);
								database_requests.push_back(add_request);
							}
						}
					}
				}
			}
		}
		num_of_poll_sockets = poll_sockets.size();
	}
	return 0;
}
bool loginPlayer(player* player_object, string username, string password){
	player_object->first_name = "TestUser";
	player_object->database_id = test_user_count;
	player_object->online = true;

	player_object->profile_picture = "TestUser.png";
	player_object->diamonds = 50;
	player_object->points = 0;
	player_object->wins = 0;
	player_object->sos = 0;
	test_user_count++;
	return true;
}
void* databaseRequestThread(void*){
	while(server_online){
		if(database_requests.size() > 0){
			if(database_requests[0].action == "LOGIN"){
				player create_player;
				if(loginPlayer(&create_player, database_requests[0].token[0], database_requests[0].token[1]) == false){

				}
				create_player.socket = database_requests[0].socket;
				players.push_back(create_player);
				cout << "[DATABASE] Client " << database_requests[0].socket << " has logged in as " << create_player.first_name << endl;
			}
			database_requests.erase(database_requests.begin());
		}
	}
	return 0;
}
void initServerSocket() {
	master_socket = socket(AF_INET, SOCK_STREAM, 0);
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);
	if (bind(master_socket, (struct sockaddr*) &address, sizeof(address)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	if ((listen(master_socket, 20)) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	server_online = true;
	display_msg("SERVER", "is now online!");
}
bool close_server() {
	display_msg("SERVER", "has closed.");
	close(master_socket);
	return true;
}

int main(int argc, char* argv[]) {
	initServerSocket();
	pthread_t t1, t2;
	pthread_create(&t1, NULL, connectionThread, NULL);
	pthread_create(&t2, NULL, databaseRequestThread,NULL);
	pthread_join(t2, NULL);
	pthread_join(t1, NULL);
	if (close_server()) {
		return 0;
	}

}

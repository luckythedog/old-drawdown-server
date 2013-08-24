#define _GNU_SOURCE
#include <stdio.h>
#include <iostream>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#include <time.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <vector>

#include <sstream>
#include <fstream>

#include <poll.h>
#include "server_socket.h"

#define PORT 6666
#define BUFFSIZE 255
#define ROOM_SIZE 2
#define ROOM_TIMEOUT 10
#define ROOM_ROUND_TIME 90
#define DATABASE_NAME "drawdown_db"


using namespace std;
server master_socket(PORT);
bool server_online = false;
pthread_mutex_t players_lock;
pthread_mutex_t drawing_lock;
pthread_mutex_t room_lock;
pthread_cond_t save_cond;

struct coordinatePoint {
	bool isNewLine;
	int id;
	int x;
	int y;
};
struct drawing_data {
	int id;
	string word;
	vector<coordinatePoint> data;
};
struct player {
	int database_id;
	int socket;
	bool in_queue;
	bool online;
	string display_name;
	string profile_picture;
	int value_wins;
	int value_sos;
	int value_points;
	int value_diamonds;
	bool in_game;
	bool in_room;
	vector<int> already_seen;
};
struct room {
	bool players_ready[ROOM_SIZE];
	player* players_info[ROOM_SIZE];
	int status;
	//0 GETTING PLAYERS IN GETTING READY, 1 IN GAME, 2 DONE
	drawing_data drawing;
	string data;
	int winner;
	int queue_number;
	time_t room_created;
	time_t game_started;
	time_t last_time;
	int pot;
	//Guessing string
	vector<string> guess_word;
	vector<int> guess_player;
	//Number of winner;

};

struct request {
	int socket;
	string action;
	vector<string> token_s;
	vector<int> token_i;
	player* player_info;
};
vector<player*> players;
vector<request> database_requests;
vector<request> queue_requests;
vector<request> room_requests;
vector<drawing_data> newbies_drawings;
vector<drawing_data> regulars_drawings;
vector<drawing_data> highrollers_drawings;
vector<vector<player*> > main_queue;
vector<room*> rooms;
int pot_value[9] = { 1, 3, 5, 10, 15, 20, 50, 75, 100 };

int sendall(int s, char *buf, int *len) {
	int total = 0; // how many bytes we've sent
	int bytesleft = *len; // how many we have left to send
	int n;

	while (total < *len) {
		n = send(s, buf + total, bytesleft, 0);
		if (n == -1) {
			break;
		}
		total += n;
		bytesleft -= n;
	}

	*len = total; // return number actually sent here

	return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}
void* connectionThread(void*) {
	vector<pollfd> poll_sockets;
	pollfd* poll_sockets_ptr;
	int poll_activity;
	pollfd add_socket;
	add_socket.fd = master_socket.fd;
	add_socket.events = POLLIN;
	poll_sockets.push_back(add_socket);
	while (server_online) {
		poll_sockets_ptr = &poll_sockets[0];
		poll_activity = poll(poll_sockets_ptr, poll_sockets.size(), -1);
		if (poll_activity < 0) {
			perror("poll");
		}
		if (poll_sockets[0].revents & POLLIN) {
			int new_socket;
			struct sockaddr_in client_address;
			int client_addrlen = sizeof(client_address);

			if ((new_socket = accept(master_socket.fd,
					(struct sockaddr*) &client_address,
					(socklen_t*) &client_addrlen)) < 0) {
				perror("accept");
			} else {
				pollfd add_socket;
				add_socket.fd = new_socket;
				add_socket.events = POLLIN;
				poll_sockets.push_back(add_socket);
				cout << "Client " << new_socket << " has connected! ("
						<< inet_ntoa(client_address.sin_addr) << ":"
						<< ntohs(client_address.sin_port) << ")" << endl;
				char* send_message = "SUCCESSFUL_CONNECTION;\0";
				if (send(new_socket, send_message, strlen(send_message), 0)
						!= strlen(send_message)) {
					perror("send");
				}

			}

		}
		for (int i = 0; i < poll_sockets.size(); i++) {
			if (i != 0) {
				int s = poll_sockets[i].fd;
				if (poll_sockets[i].revents & POLLIN) {
					char buffer[BUFFSIZE];
					if ((read(poll_sockets[i].fd, buffer, BUFFSIZE)) != 0) {
						vector<string> client_messages;
						char* msg_tok = strtok(buffer, ";");
						while (msg_tok != NULL) {
							client_messages.push_back(msg_tok);
							msg_tok = strtok(NULL, ";");
						}
						cout << "HEY LISTEN!" << client_messages[0] << endl;
						if (client_messages[0] == "LOGIN") {
							request add_request;
							add_request.action = client_messages[0];
							add_request.token_s.push_back(client_messages[1]);
							add_request.token_s.push_back(client_messages[2]);
							add_request.socket = s;
							database_requests.push_back(add_request);
						}
						if (client_messages[0] == "REQUEST_PROFILE") {
													request add_request;
													add_request.action = client_messages[0];
													add_request.socket = s;
													database_requests.push_back(add_request);
												}
						if (client_messages[0] == "REGISTER") {
							request add_request;
							add_request.action = client_messages[0];
							add_request.token_s.push_back(client_messages[1]); //EMAIL
							add_request.token_s.push_back(client_messages[2]); //DISPLAY_NAME
							add_request.token_s.push_back(client_messages[3]); //PASSWORD
							add_request.socket = s;
							database_requests.push_back(add_request);
						}
						if (client_messages[0] == "JOIN_QUEUE") {
							request add_request;
							add_request.action = client_messages[0];
							add_request.socket = s;
							add_request.token_i.push_back(
									atoi(client_messages[1].c_str()));
							queue_requests.push_back(add_request);
						}
						if (client_messages[0] == "REQUEST_DRAWING") {
							request add_request;
							add_request.action = client_messages[0];
							add_request.socket = s;
							room_requests.push_back(add_request);

						}
						if (client_messages[0] == "READY") {
							request add_request;
							add_request.action = client_messages[0];
							add_request.socket = s;
							room_requests.push_back(add_request);
						}
						if (client_messages[0] == "GUESS") {
							cout << "Guess sent!" << endl;
							request add_request;
							add_request.action = client_messages[0];
							add_request.token_s.push_back(client_messages[1]);
							add_request.socket = s;
							room_requests.push_back(add_request);
						}
						//		if(client_messages[0] == "REGISTER_FACEBOOK"){
						//		request add_request;
						//	add_request.action = client_messages[0];
						//add_request.token_s.push_back(client_messages[1]); //FACEBOOK ID
						// add_request.socket = s;
						// database_requests.push_back(add_request);
						//	}
						//	if(client_messages[0] == "LOGIN_FACEBOOK"){
						//	request add_request;
						//		    				add_request.action = client_messages[0];
						//		    				add_request.token_s.push_back(client_messages[1]); //FACEBOOK ID REQUIRED
						//		    				add_request.token_s.push_back(client_messages[2]); //DISPLAY_NAME MALLABLE
						//		    				add_request.token_s.push_back(client_messages[3]); //PROFILE PICTURE MALLABLE TAKEN FROM FACEBOOK
						//		}

					} else {
						pthread_mutex_lock(&players_lock);
						for (int pls = 0; pls < players.size(); pls++) {
							if (players[pls]->socket == s) {
								players[pls]->online = false;
								players[pls]->socket = -1;
								cout << "Player " << players[pls]->display_name
										<< " has gone offline!" << endl;
								break;
							}
						}
						pthread_mutex_unlock(&players_lock);
						cout << "Client " << s << " has disconnected!" << endl;
						poll_sockets.erase(poll_sockets.begin() + i);
						close(s);
					}
				}
			}
		}

	}
	return 0;
}
void registerPlayer(int socket, string email, string display_name,
		string password) {
//handle resgistration send successful register, client logins itself
}
player* loginPlayer(int socket, string email, string password) {
	player* create_player = new player;
	create_player->display_name = email;
	create_player->profile_picture =
			"http://1.bp.blogspot.com/-5n_E6MRifss/UP4FZEI4fII/AAAAAAAAAD8/kfySLAXO_3c/s1600/victor+icon.jpg";
	create_player->online = true;
	create_player->in_game = false;
	create_player->in_room = false;
	create_player->in_queue = false;
	create_player->value_diamonds = 50;
	create_player->value_wins = 0;
	create_player->value_sos = 5;
	create_player->value_points = 2000;
	create_player->socket = socket;
	return create_player;

}
void* databaseThread(void*) {
	while (server_online) {
		if (database_requests.size() > 0) {
			bool isPlayerLoggedIn = false;
			int playerIndex;
			pthread_mutex_lock(&players_lock);
			for (int i = 0; i < players.size(); i++) {
				if (players[i]->socket == database_requests[0].socket) {
					isPlayerLoggedIn = true;
					playerIndex = i;
					break;
				}
			}
			pthread_mutex_unlock(&players_lock);
			if (database_requests[0].action == "LOGIN" && !isPlayerLoggedIn) {

				player* add_player = loginPlayer(database_requests[0].socket,
						database_requests[0].token_s[0],
						database_requests[0].token_s[1]);
				players.push_back(add_player);
				cout << "Player " << add_player->display_name
						<< " has logged in! (Socket "
						<< database_requests[0].socket << ")" << endl;
				char* send_message = "SUCCESSFUL_LOGIN;\0";
				int send_message_len = strlen(send_message);
				sendall(database_requests[0].socket, send_message,
						&send_message_len);

			}
			if(database_requests[0].action == "REQUEST_PROFILE" && isPlayerLoggedIn){
				stringstream login_ss;
					login_ss << players[playerIndex]->display_name << ";"
							<< players[playerIndex]->profile_picture << ";"
							<< players[playerIndex]->value_wins << ";"
							<< players[playerIndex]->value_points << ";"
							<< players[playerIndex]->value_diamonds << ";"
							<< players[playerIndex]->value_sos << ";";
					string login_import = login_ss.str();
					//Format of player sent data.. DISPLAY_NAME;PROFILE_PICTURE;VALUE_WINS;VALUE_POINTS;VALUE_DIAMONDS;VALUE_SOS;
					char* send_message = &login_import[0];
					int send_message_len = strlen(send_message);
					sendall(database_requests[0].socket, send_message,
							&send_message_len);
			}
			if (database_requests[0].action == "REGISTER"
					&& !isPlayerLoggedIn) {
				registerPlayer(database_requests[0].socket,
						database_requests[0].token_s[0],
						database_requests[0].token_s[1],
						database_requests[0].token_s[2]);
			}
			database_requests.erase(database_requests.begin());
		}

	}
	return 0;
}
void* queueThread(void*) {
	for (int i = 0; i < 9; i++) {
		vector<player*> row;
		main_queue.push_back(row);
	}
	while (server_online) {
		if (queue_requests.size() > 0) {

			int playerIndex;
			pthread_mutex_lock(&players_lock);
			for (int i = 0; i < players.size(); i++) {
				if (players[i]->socket == queue_requests[0].socket) {
					playerIndex = i;
				}
			}
			pthread_mutex_unlock(&players_lock);

			if (queue_requests[0].action == "JOIN_QUEUE") {
				if (!players[playerIndex]->in_queue) {
					players[playerIndex]->in_queue = true;
					main_queue[queue_requests[0].token_i[0]].push_back(
							players[playerIndex]);
					char* send_message = "SUCCESSFUL_QUEUE;\0";
					if (send(players[playerIndex]->socket, send_message,
							strlen(send_message), 0) != strlen(send_message)) {
						perror("send");
					}

				} else {
					char* send_message = "ALREADY_IN_QUEUE;\0";
					if (send(players[playerIndex]->socket, send_message,
							strlen(send_message), 0) != strlen(send_message)) {
						perror("send");
					}

				}

			}
			queue_requests.erase(queue_requests.begin());
		}
		//CHECK FOR DISCONNECT!
		for (int c = 0; c < 9; c++) {
			if (main_queue[c].size() > 0) {
				for (int i = 0; i < 9; i++) {
					for (int b = 0; b < main_queue[i].size();) {
						if (!main_queue[i][b]->online
								&& main_queue[i][b]->in_queue) {
							cout << "Queue " << i << " has removed player "
									<< main_queue[i][b]->display_name
									<< " due to being offline." << endl;
							pthread_mutex_lock(&players_lock);
							main_queue[i][b]->in_queue = false;
							main_queue[i].erase(main_queue[i].begin() + b);
							pthread_mutex_unlock(&players_lock);
						} else {
							b++;
						}
					}
				}
				//ROOM THEM UP!
				for (int i = 0; i < 9; i++) {
					if (main_queue[i].size() >= ROOM_SIZE) {
						room* create_room = new room;
						stringstream room_data;
						create_room->status = 0;
						create_room->queue_number = i;
						create_room->pot = pot_value[i];
						room_data << "START_ROOM;";
						room_data << "INIT_PLAYER_DATA;";

						for (int b = 0; b < ROOM_SIZE; b++) {
							room_data << "START_DATA;";
							room_data << main_queue[i][0]->display_name << ";"
									<< main_queue[i][0]->profile_picture << ";";
							room_data << "END_DATA;";
							main_queue[i][0]->in_room = true;
							main_queue[i][0]->in_queue = false;
							create_room->players_info[b] = main_queue[i][0];
							create_room->players_ready[b] = false;
							main_queue[i].erase(main_queue[i].begin());
						}
						room_data << "END_PLAYER_DATA;";
						room_data << create_room->pot << ";";
						room_data << "SUCCESSFUL_ROOM;" << endl;
						create_room->room_created = time(NULL);
						string player_data = room_data.str();
						char* send_message = &player_data[0];
						int send_message_len = strlen(send_message);
						for (int b = 0; b < ROOM_SIZE; b++) {
							sendall(create_room->players_info[b]->socket,
									send_message, &send_message_len);
						}

						//FAKE DRAWING_DATA
						drawing_data fake_drawing;
						coordinatePoint fake_point;
						fake_point.id = 0;
						fake_point.x = 50;
						fake_point.y = 50;
						fake_drawing.data.push_back(fake_point);
						fake_drawing.data.push_back(fake_point);
						fake_drawing.word = "fake";
						create_room->drawing = fake_drawing;
						//END FAKE DRAWING DATA

						//PLAYER HAS TO REQUEST_DATA AFTER RECEIVING SUCCESSFUL_ROOM;
						stringstream data;
						data << "INIT_DRAWING_DATA;";
						for (int b = 0; b < create_room->drawing.data.size();
								b++) {
							data << create_room->drawing.data[b].id << ";";
							if (!create_room->drawing.data[b].isNewLine) {
								data << create_room->drawing.data[b].x << ";";
								data << create_room->drawing.data[b].y << ";";
							} else {
								data << "NL;";
								data << "NL;";
							}
						}
						data << "END_DRAWING_DATA;";
						create_room->data = data.str();
						rooms.push_back(create_room);
					}
				}
			}
		}

	}
	return 0;
}

void* roomThread(void*) {
	time_t current_time;
	while (server_online) {
		current_time = time(NULL);
		if (room_requests.size() > 0) {
			int playerIndex;
			int roomIndex;
			bool foundRoom = false;
			pthread_mutex_lock(&players_lock);
			for (int i = 0; i < players.size(); i++) {
				if (players[i]->socket == room_requests[0].socket) {
					playerIndex = i;
				}
			}
			for (int i = 0; i < rooms.size(); i++) {
				for (int b = 0; b < ROOM_SIZE; b++) {
					if (rooms[i]->players_info[b]->socket
							== room_requests[0].socket) {
						roomIndex = i;
						foundRoom = true;
					}
				}
			}
			pthread_mutex_unlock(&players_lock);
			if (room_requests[0].action == "REQUEST_DRAWING") {
				if (players[playerIndex]->in_room && foundRoom) {
					char* send_message = &rooms[roomIndex]->data[0];
					int send_message_len = strlen(send_message);
					sendall(players[playerIndex]->socket, send_message,
							&send_message_len);

				}

			}
			if (room_requests[0].action == "READY") {
				if (players[playerIndex]->in_room && foundRoom && rooms[roomIndex]->status == 0) {
					for (int b = 0; b < ROOM_SIZE; b++) {
						if (players[playerIndex]->socket
								== rooms[roomIndex]->players_info[b]->socket) {
							if (rooms[roomIndex]->status == 0) {
								rooms[roomIndex]->players_ready[b] = true;
								cout << "Player ready!" << endl;
							}
						}
					}
					for (int b = 0; b < ROOM_SIZE; b++) {
						cout << "Player ready " << b << " "
								<< rooms[roomIndex]->players_ready[b] << endl;
					}
				}
			}
			if(room_requests[0].action == "GUESS"){
				if(players[playerIndex]->in_game && foundRoom && rooms[roomIndex]->status == 1){
					for(int b=0; b<ROOM_SIZE;b++){
						if(rooms[roomIndex]->players_info[b]->socket == players[playerIndex]->socket){
							rooms[roomIndex]->guess_word.push_back(room_requests[0].token_s[0]);
												rooms[roomIndex]->guess_player.push_back(b);
						}
					}
				}
			}
			room_requests.erase(room_requests.begin());
		}
		if(rooms.size() > 0){
		for(int i=0; i<rooms.size();){
		            if(rooms[i]->status == 0){
		                time_t current_time;
		                current_time = time(NULL);
		                bool close = false;
		                for(int b=0; b<ROOM_SIZE; b++){
		                    if(rooms[i]->players_info[b]->online == false){
		                        rooms[i]->players_info[b]->in_room = false;
		                        close = true;
		                    }
		                }
		                if((current_time-rooms[i]->room_created) >= ROOM_TIMEOUT){
		                    cout << "Room timeout!" << endl;
		                    close = true;
		                }
		                if(close){
		                    for(int b=0; b<ROOM_SIZE; b++){
		                        if(rooms[i]->players_info[b]->online == true){
		                            char* send_message = "SUCCESSFUL_QUEUE;\0";
		                            if( send(rooms[i]->players_info[b]->socket, send_message, strlen(send_message), 0) != strlen(send_message) ){ perror("send");}
		                            cout << "Room " << i << " has disbanded and player added to queue!" << endl;
		                            rooms[i]->players_info[b]->in_queue = true;
		                            rooms[i]->players_info[b]->in_room = false;
		                            main_queue[rooms[i]->queue_number].push_back(rooms[i]->players_info[b]);
		                        }
		                    }
		                    pthread_mutex_lock(&room_lock);
		                    delete rooms[i];
		                    rooms.erase(rooms.begin()+i);
		                    pthread_mutex_unlock(&room_lock);
		                }else{
		                    i++;
		                }
		            }else{
		                i++;
		            }
		        }

		 for(int i=0; i<rooms.size(); i++){
		            if(rooms[i]->status == 0){
		            bool isRoomReady = true;
		            for(int b=0; b<ROOM_SIZE; b++){
		                if(rooms[i]->players_ready[b] == 0){
		                    isRoomReady = false;
		                }
		            }
		            if(isRoomReady){
		                for(int b=0; b<ROOM_SIZE; b++){
		                    char* send_message = "START_GAME;\0";
		                    if( send(rooms[i]->players_info[b]->socket, send_message, strlen(send_message), 0) != strlen(send_message) ){ perror("send");}
		                    rooms[i]->game_started = time(NULL);
		                    rooms[i]->players_info[b]->in_game = true;
		                    rooms[i]->players_info[b]->in_room = false;
		                }
		                cout << "Room ready to go! All players ready!" << endl;
		                rooms[i]->status = 1;
		            }
		            }
		        }
		        //GUESS DRAWING!

		        for(int i=0; i<rooms.size(); i++){
		            if(rooms[i]->status == 1){
		                time_t current_time = time(NULL);
		                time_t product_time = current_time-rooms[i]->game_started;
		                int game_state = 0;
		                //0 No winner still in progress
		                //1 WINNER
		                //2 TIMEOUT
		            int winner_id;
		            if(rooms[i]->guess_word.size() >0){
		                cout << "Processing guess!" << endl;
		                string word_guessed = rooms[i]->guess_word[0];
		                string word_is = rooms[i]->drawing.word;
		                char* char_word_guessed = &word_guessed[0];
		                char* char_word_is = &word_is[0];
		                cout << "Comparing!" << endl;
		                if((strcasecmp(char_word_guessed, char_word_is)) == 0){
		                    //WON!
		                    game_state = 1;
		                    winner_id = rooms[i]->guess_player[0];
		                }
		                cout << "Finished Comparing!" << endl;
		                rooms[i]->guess_word.erase(rooms[i]->guess_word.begin());
		                rooms[i]->guess_player.erase(rooms[i]->guess_player.begin());
		            }
		                if(product_time >= ROOM_ROUND_TIME){
		                    game_state = 2;
		                }else{
		                    if(rooms[i]->last_time != current_time){
		                        rooms[i]->last_time = current_time;
		                    stringstream time;
		                    time << "TIME;" << current_time-rooms[i]->game_started << ";";
		                    string time_import = time.str();
		                    char* data_sending = &time_import[0];
		                    int data_sending_len = strlen(data_sending);
		                          for(int b=0; b<ROOM_SIZE; b++){
		                    sendall(rooms[i]->players_info[b]->socket, data_sending, &data_sending_len);
		                          }
		                    }

		                }
		                if(game_state == 2){
		                    stringstream timeout_data;
		                    timeout_data << "TIMEOUT;" << "WORD;" << rooms[i]->drawing.word << ";"<< "POT_VALUE;" << rooms[i]->pot << ";";
		                    string timeout_data_import = timeout_data.str();
		                    for(int b=0; b<ROOM_SIZE; b++){

		                        rooms[i]->players_info[b]->in_game = false;
		                        rooms[i]->status = 2;
		                        rooms[i]->winner = -1;
		                        char* data_sending = &timeout_data_import[0];
		                        int data_sending_len = strlen(data_sending);
		                        sendall(rooms[i]->players_info[b]->socket, data_sending, &data_sending_len);
		                    }
		                    //TIMEOUT

		                }
		            if(game_state == 1){
		                rooms[i]->status = 2;
		                rooms[i]->winner = winner_id;
		                stringstream loser_data;
		                stringstream winner_data;
		                //WINNER PROCESSING
		                winner_data << "WIN;" << "WORD;" << rooms[i]->drawing.word << ";" << "POT_VALUE;" << rooms[i]->pot << ";";
		                //LOSER STRINGSTREAM PROCESSING
		                loser_data << "LOSE;" << "WORD;" << rooms[i]->drawing.word << ";";
		                loser_data << "POT_VALUE;";
		                loser_data << rooms[i]->pot << ";";
		                loser_data << "WINNING_PLAYER_START;";
		                loser_data << rooms[i]->players_info[winner_id]->display_name << ";" << rooms[i]->players_info[winner_id]->profile_picture << ";";
		                loser_data << "WINNING_PLAYER_END;";
		                                            string loser_data_import = loser_data.str();
		                              string winner_data_import = winner_data.str();

		                for(int b=0; b<ROOM_SIZE; b++){
		                    if(b != winner_id){
		                        //LOSER!
		                        rooms[i]->players_info[b]->in_game = false;
		                        rooms[i]->players_info[b]->value_diamonds -= (rooms[i]->pot/5);
		                        if(rooms[i]->players_info[b]->online == true){
		                         //Send losing message!

		                            char* data_sending = &loser_data_import[0];
		                            int data_sending_len = strlen(data_sending);
		                            sendall(rooms[i]->players_info[b]->socket, data_sending, &data_sending_len);
		                        }
		                    }else{
		                        //WINNER!
		                        rooms[i]->players_info[b]->in_game = false;
		                        rooms[i]->players_info[b]->value_diamonds += rooms[i]->pot;

		                        char* data_sending = &winner_data_import[0];
		                        int data_sending_len = strlen(data_sending);
		                        sendall(rooms[i]->players_info[b]->socket, data_sending, &data_sending_len);

		                    }
		                }
		            }
		            }
		        }

		        //ROOM Clean up

		        for(int i=0; i<rooms.size();){
		            if(rooms[i]->status == -2){
		                pthread_mutex_lock(&room_lock);
		                delete rooms[i];
		                rooms.erase(rooms.begin()+i);
		                pthread_mutex_unlock(&room_lock);
		            }else{
		                i++;
		            }
		        }
	}


	}
	return 0;
}
void* saveThread(void*){
    while(server_online){
        for(int i=0; i<players.size();){
            if(!players[i]->online && !players[i]->in_queue && !players[i]->in_room && !players[i]->in_game){
                //saved players!

                pthread_mutex_lock(&players_lock);
                cout << "Saved sweeped player: " << players[i]->display_name << endl;
                delete players[i];
                players.erase(players.begin()+i);
                pthread_mutex_unlock(&players_lock);
            }else{
                i++;
            }
        }
    }
    return 0;
}
bool loadDrawingData() {
	pthread_mutex_lock(&drawing_lock);
	int drawing_id = 0;
	bool allow;
	newbies_drawings.clear();
	regulars_drawings.clear();
	highrollers_drawings.clear();
	cout << "Loading drawings.. " << endl;
	vector<string> filenames;
	vector<int> level;
	//0 - EASY
	//1 - MEDIUM
	//2 - HARD
	fstream filename_opener;
	filename_opener.open("server_drawing_filenames.txt");

	if (filename_opener.is_open()) {
		stringstream filename_ss;
		string filename_data_import;
		char* filename_data_chopped;
		vector<string> filename_tbo;
		filename_ss << filename_opener.rdbuf();
		filename_data_import = filename_ss.str();
		filename_data_chopped = &filename_data_import[0];
		char* filename_data_tok = strtok(filename_data_chopped, "; \n");
		while (filename_data_tok != NULL) {
			filename_tbo.push_back(filename_data_tok);
			filename_data_tok = strtok(NULL, "; \n");
		}
		for (int b = 0; b < filename_tbo.size(); b += 2) {
			filenames.push_back(filename_tbo[b]);
			level.push_back(atoi(filename_tbo[b + 1].c_str()));
		}
		allow = true;

	} else {
		allow = false;
		cout << "Could not open server_drawing_filenames.txt!" << endl;
		return false;
	}
	if (allow) {
		for (int i = 0; i < filenames.size(); i++) {
			cout << "Starting on " << filenames[i] << endl;
			string file_to_open = filenames[i];
			fstream drawing_data_holder;
			drawing_data_holder.open(file_to_open.c_str());
			stringstream drawing_ss;
			string drawing_data_import;
			char* drawing_data_chopped;
			vector<string> data_tbo;
			if (drawing_data_holder.is_open()) {
				drawing_ss << drawing_data_holder.rdbuf();
				drawing_data_import = drawing_ss.str();
				drawing_data_chopped = &drawing_data_import[0];
				char* drawing_data_tok = strtok(drawing_data_chopped, ";\n");
				while (drawing_data_tok != NULL) {
					data_tbo.push_back(drawing_data_tok);
					drawing_data_tok = strtok(NULL, ";\n");
				}
				bool parsing = true;
				int vc = 0;
				vector<drawing_data> drawings;
				drawing_data new_drawing;
				while (parsing) {
					if (data_tbo[vc] == "INIT_DRAWING_DATA") {
						vc++;
					}
					if (data_tbo[vc] == "START_DATA") {
						new_drawing.id = drawing_id;
						new_drawing.word = data_tbo[vc + 1];
						new_drawing.data.clear();
						vc += 2;

					}
					if (data_tbo[vc] != "INIT_DRAWING_DATA"
							&& data_tbo[vc] != "START_DATA"
							&& data_tbo[vc] != "END_DATA"
							&& data_tbo[vc] != "END_DRAWING_DATA") {
						coordinatePoint new_point;
						new_point.id = atoi(data_tbo[vc].c_str());
						new_point.x = atoi(data_tbo[vc + 1].c_str());
						new_point.y = atoi(data_tbo[vc + 2].c_str());
						new_point.isNewLine = false;
						if (data_tbo[vc + 1] == "NL") {
							new_point.isNewLine = true;
							new_point.x = -27015;
							new_point.y = -27015;
						}
						new_drawing.data.push_back(new_point);
						vc += 3;

					}
					if (data_tbo[vc] == "END_DATA") {
						drawings.push_back(new_drawing);
						drawing_id++;
						vc++;

					}
					if (data_tbo[vc] == "END_DRAWING_DATA") {
						if (level[i] == 0) {
							for (int c = 0; c < drawings.size(); c++) {
								newbies_drawings.push_back(drawings[c]);

							}

						}
						if (level[i] == 1) {
							for (int c = 0; c < drawings.size(); c++) {
								regulars_drawings.push_back(drawings[c]);

							}

						}
						if (level[i] == 2) {
							for (int c = 0; c < drawings.size(); c++) {
								highrollers_drawings.push_back(drawings[c]);

							}
						}
						parsing = false;
					}
				}
			} else {
				cout << "Failed to open " << filenames[i] << endl;
			}

		}
	}
	cout << "Size of Newbies Drawings: " << newbies_drawings.size() << endl;
	cout << "Size of Regulars Drawings: " << regulars_drawings.size() << endl;
	cout << "Size of Highrollers Drawings: " << highrollers_drawings.size()
			<< endl;
	pthread_mutex_unlock(&drawing_lock);
	return true;
}
bool close_server() {

	close(master_socket.fd);
	cout << "Server is now offline!" << endl;
	return true;
}
void* consoleThread(void*) {
	while (server_online) {
		string command;
		while (cin >> command) {
			if (command == "update_drawings") {
				loadDrawingData();
			}
		}
	}
	return 0;
}

int main(int argc, char *argv[]) {
	if (!master_socket.start()) {
		perror("start");
		exit(EXIT_FAILURE);
	}
	// if(!loadDrawingData()){
	//   exit(EXIT_FAILURE);

	//}
	pthread_t t1, t2, t3, t4, t5;
	cout << "Server is now online!" << endl;
	server_online = true;
	if (pthread_mutex_init(&drawing_lock, NULL) != 0) {
		perror("mutexlock");
		exit(EXIT_FAILURE);
	}
	if (pthread_mutex_init(&players_lock, NULL) != 0) {
		perror("mutexlock");
		exit(EXIT_FAILURE);
	}
	if (pthread_mutex_init(&room_lock, NULL) != 0) {
		perror("mutexlock");
		exit(EXIT_FAILURE);
	}
	if (pthread_cond_init(&save_cond, NULL) != 0) {
		perror("condlock");
		exit(EXIT_FAILURE);
	}
	pthread_create(&t1, NULL, connectionThread, NULL);
	pthread_create(&t2, NULL, databaseThread, NULL);
	pthread_create(&t3, NULL, queueThread, NULL);
	pthread_create(&t4, NULL, roomThread, NULL);
	pthread_create(&t5, NULL, saveThread, NULL);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	pthread_join(t3, NULL);
	pthread_join(t4, NULL);
	pthread_join(t5, NULL);
	if (close_server()) {
		return 0;
	}
}


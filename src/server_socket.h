//
//  createSocket.h
//  drawdown_server
//
//  Created by Kevin Trinh on 1/7/13.
//  Copyright (c) 2013 Kevin Trinh. All rights reserved.
//
#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>


#ifndef server_socket_h
#define server_socket_h
class server{

public:
    int port;
    int fd;
    server(int port) { this->port = port; this->fd = socket(AF_INET, SOCK_STREAM,0);} // Constructor
    bool start();
};


#endif

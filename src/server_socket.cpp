//
//  createSocket.cpp
//  drawdown_server
//
//  Created by Kevin Trinh on 1/7/13.
//  Copyright (c) 2013 Kevin Trinh. All rights reserved.
//

#include "server_socket.h"

bool server::start(){
    struct sockaddr_in address;
    address.sin_port = htons(this->port);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    if((bind(this->fd, (struct sockaddr*) &address, sizeof(address)))<0){
        return false;
        perror("bind");
    }
    if((listen(this->fd, 20))<0){
        return false;
        perror("listen");
    }
    return true;
}
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>

#define ERROR 1
#define SUCCESS 0 
#define SELECT_ERROR -1
#define MSGBUFSIZE 256

struct my_socket {
    int sockfd;
    struct sockaddr_in addr;
    my_socket(int fd, sockaddr_in addr): sockfd(fd), addr(addr) {}
};

struct my_socket create_send_socket(char* group, int port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(SUCCESS);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(group);
    addr.sin_port = htons(port);
    return my_socket(fd, addr);
}

struct my_socket create_receive_socket(char* group, int port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
        perror("socket");
        exit(SUCCESS);
    }
    
    // allow multiple sockets to use the same PORT number
    u_int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) < 0){
        perror("Reusing ADDR failed");
        exit(SUCCESS);
    }

    // set up destination address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender
    addr.sin_port = htons(port);

    // bind to receive address
    if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(SUCCESS);
    }

    // use setsockopt() to request that the kernel join a multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)) < 0){
        perror("setsockopt");
        exit(SUCCESS);
    }

    return my_socket(fd, addr);
}

void print_users(std::vector<std::string>& users) {
    for (auto i : users) {
        std::cout << i << std::endl;
    }
    std::cout << std::endl;
    users.clear();
}

void receive(const my_socket& receive_socket, char* msgbuf) {
    unsigned int addrlen = sizeof(receive_socket.addr);
    int nbytes = recvfrom(receive_socket.sockfd, msgbuf, MSGBUFSIZE, 0, (struct sockaddr *) &receive_socket.addr, &addrlen);

    if (nbytes < 0) {
        perror("recvfrom");
        exit(ERROR);
    }
}

int main(int argc, char *argv[]) {
    char* group = argv[1]; 
    int port = atoi(argv[2]);
    const char *message = argv[3];

    time_t delay = 0.1;

    my_socket send_socket = create_send_socket(group, port);
    my_socket receive_socket = create_receive_socket(group, port);

    fd_set read_flag;
    struct timeval tv;

    time_t t = time(nullptr), start = time(nullptr);

    char msgbuf[MSGBUFSIZE];
    int nbytes = 0;

    std::vector<std::string> users;

    while (true) {

        if (time(nullptr) - t > delay) {
            nbytes = sendto(send_socket.sockfd, message, strlen(message), 0, (struct sockaddr*) &send_socket.addr, sizeof(send_socket.addr));
            if (nbytes < 0) {
                perror("sendto");
                close(receive_socket.sockfd);
                close(send_socket.sockfd);
                return ERROR;
            }
            t = time(nullptr);
        }

        tv.tv_sec = delay;
        tv.tv_usec = 0;

        FD_ZERO(&read_flag);
        FD_SET(receive_socket.sockfd, &read_flag);
      
        int res_select = select(receive_socket.sockfd + 1, &read_flag, nullptr, nullptr, &tv);

        if (res_select == -1) {
            perror("select");
            close(receive_socket.sockfd);
            close(send_socket.sockfd);
            return ERROR;
        }

        if (FD_ISSET(receive_socket.sockfd, &read_flag) > 0) {
            receive(receive_socket, msgbuf);

            if (std::count(users.begin(), users.end(), std::string(msgbuf)) == 0) {
                users.push_back(std::string(msgbuf));
            }
            msgbuf[0] = 0;
        }
        
        if (time(nullptr) - start > 3 * delay) {
            print_users(users);
            start = time(nullptr);
        }
    }

    close(receive_socket.sockfd);
    close(send_socket.sockfd);
    return SUCCESS;
}

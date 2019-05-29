#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdint.h>
#include "md5.h"
#include <regex>
#include <iostream>
#include <thread>

#define PORT "32000"  // порт, на который будут приходить соединения

#define BACKLOG 10     // как много может быть ожидающих соединений

#define MAXDATASIZE 64

#define HEADERSIZE 4

#define HASHSIZE 16

bool CheckMsg(char* msg, size_t msglen) {
    bool flag = true;
    uint32_t* arr = md5(msg, msglen);

    size_t k = 1;
    for (int i = 3; i >= 0; i--) {
        uint8_t* q=(uint8_t *)&arr[i];
        for (int j = 3; j >= 0; j--) {
            if ((msg[msglen + HASHSIZE - k]+256)%256 != q[j]){
                flag = false;
                break;
            }
            k++;
        }
    }
    return flag;
}

void ListeningSocket(int new_fd) {
    char headBuf[HEADERSIZE + HASHSIZE];
    char buf[MAXDATASIZE + HASHSIZE];
    memset(headBuf, 0, HEADERSIZE + HASHSIZE);
    int packeges = 0;
    //sleep(1);
    while (true) {
        recv(new_fd, headBuf, HEADERSIZE + HASHSIZE, 0);
        std::string head(headBuf);
        packeges = atoi(head.substr(0, HEADERSIZE-1).c_str());
        if (CheckMsg(headBuf, HEADERSIZE)) {
            std::cout << "Packages: " << packeges << std::endl;
            send(new_fd, "1", 1, 0);
            break;
        }
        else {
            if (send(new_fd, "0", 1, 0) == -1)
                perror("send");
        }
    }

    for (int i = 0; i < packeges; i++) {
        while (true) {
            memset(headBuf, 0, HEADERSIZE + HASHSIZE);
            if (recv(new_fd, headBuf, HEADERSIZE + HASHSIZE, 0) == -1)
            {
                perror("recv");
            }
//            for (int ivan = 0; ivan < HEADERSIZE + HASHSIZE; ivan++) {
//                std::cout << int (headBuf[ivan]);
//            }
            //sleep(10);
            if (CheckMsg(headBuf, HEADERSIZE)) {
                if (send(new_fd, "1", 1, 0) == -1)
                    perror("send");
                break;
            }
            else {
                if (send(new_fd, "0", 1, 0) == -1)
                    perror("send");
            }
            //std::cout << "хуй";

        }
        int length = atoi(headBuf);
        std::cout << "Package length: " << length << std::endl;
        while (true) {
            memset(buf, 0, MAXDATASIZE + HASHSIZE);
            if (recv(new_fd, buf, length + HASHSIZE, 0) == -1)
            {
                std::cout << "хуй\n";
                perror("recv");
                break;
            }
//            for (int i = 0; i < length + HASHSIZE; i++) {
//                std::cout << int(buf[i]) << '\n';
//            }
            if (CheckMsg(buf, length)) {
                if (send(new_fd, "1", 1, 0) == -1)
                    perror("send");
                break;

            }
            else {
                if (send(new_fd, "0", 1, 0) == -1)
                    perror("send");
            }
        }
        std::cout << "Package: ";
        for (int i = 0; i < length; i++) {
            std::cout << buf[i];
        }
        std::cout << std::endl;

    }
    close(new_fd);
}

void sigchld_handler(int s)
{
    while(waitpid(-1, nullptr, WNOHANG) > 0);
}

// получаем адрес сокета, ipv4 или ipv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int sockfd, new_fd;  // слушаем на sock_fd, новые соединения - на new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // информация об адресе клиента
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %sn", gai_strerror(rv));
        return 1;
    }

    // цикл через все результаты, чтобы забиндиться на первом возможном
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (!p)  {
        fprintf(stderr, "server: failed to bindn");
        return 2;
    }

    freeaddrinfo(servinfo); // всё, что можно, с этой структурой мы сделали

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // обрабатываем мёртвые процессы
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...n");


    char buf[MAXDATASIZE];
    char headBuf[HEADERSIZE];
    while (true) {
        memset(buf, 0 , MAXDATASIZE);
        memset(headBuf, 0, HEADERSIZE);
        sin_size = sizeof their_addr;
        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);
        std::thread thread(ListeningSocket, new_fd);
        thread.join();
    }


    return 0;
}


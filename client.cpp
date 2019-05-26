#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "md5.h"
#include <arpa/inet.h>
#include <iostream>
#include <vector>
#include <time.h>


#define PORT "32000" // Порт, к которому подключается клиент

#define MAXDATASIZE 64 // максимальное число байт, принимаемых за один раз
#define HEADERSIZE 4
#define HASHSIZE 16

#define IP_ADDR "127.0.0.1"


 /* reverse:  переворачиваем строку s на месте */
 void reverse(char s[])
 {
     int i, j;
     char c;

     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
 }

void itoa(int n, char s[])
 {
     int i, sign;

     if ((sign = n) < 0)  /* записываем знак */
         n = -n;          /* делаем n положительным числом */
     i = 0;
     do {       /* генерируем цифры в обратном порядке */
         s[i++] = n % 10 + '0';   /* берем следующую цифру */
     } while ((n /= 10) > 0);     /* удаляем */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
 }

std::vector<char> MakeMsg(const char* msg, int len) {
    char *_msg = new char[len];
    memcpy(_msg, msg, len);

    // benchmark
    // int i;
    // for (i = 0; i < 1000000; i++) {
    uint32_t* arr = md5(_msg, len);
    // }

    uint8_t *q;

    char* buf = (char*)malloc((len+HASHSIZE)*sizeof (char));
    memset(buf, 0, (len+HASHSIZE)*sizeof (char));
    memcpy(buf, msg, (len+HASHSIZE)*sizeof (char));
    int k = 1;
    for (int i = 3; i >= 0; i--) {
        q=(uint8_t *)&arr[i];
        for (int j = 3; j >= 0; j--) {
            buf[(len+HASHSIZE) - k] = q[j];
            k++;
        }
    }


    free(arr);
    std::vector<char> result;
    for (int i = 0; i < len+HASHSIZE; i++) {
        result.push_back(buf[i]);
    }
    free(buf);
    return result;
}

void sendMSG(int sockfd, std::string msg) {
    int packeges = (msg.size()) / MAXDATASIZE;
    std::vector<char> msgVec;
    if ((msg.size()) % MAXDATASIZE)
        packeges++;
    char HeadBuf[HEADERSIZE + HASHSIZE];
    memset(HeadBuf, 0, HEADERSIZE + HASHSIZE);
    itoa(packeges, HeadBuf);
    msgVec = MakeMsg(HeadBuf, HEADERSIZE);
    std::string _msg;
    for (auto &iter : msgVec) {
        //std::cout << int(iter) << '|';
        _msg.push_back(iter);
    }
    char bufbool[1];
    bufbool[0] = 0;
    while (!bufbool[0]) {
        send(sockfd, _msg.c_str(), HEADERSIZE + HASHSIZE, 0);
        if (recv(sockfd, bufbool, 1, 0) == -1)
        {
            perror("recv");
        }
        bufbool[0] = atoi(bufbool);
        //std::cout << bufbool[0];
    }
    bufbool[0] = 0;
    usleep(100);
    for (int i = 0; i < packeges; i++) {
        if (i < packeges - 1) {
            if (send(sockfd, "0064", HEADERSIZE, 0) == -1)
            {
                perror("recv");
                break;
            }
            usleep(100);
            if (send(sockfd, msg.substr(i * MAXDATASIZE, (i + 1) * MAXDATASIZE).c_str(),
                     MAXDATASIZE, 0) == -1) {
                perror("recv");
                break;
            }
        }
        else {
            if (msg.size() % MAXDATASIZE) {
                memset(HeadBuf, 0, HEADERSIZE + HASHSIZE);
                itoa(msg.size() % MAXDATASIZE, HeadBuf);
                msgVec = MakeMsg(HeadBuf, HEADERSIZE);
                std::string _msg;
                for (auto &iter : msgVec) {
                    //std::cout << int(iter) << '|';
                    _msg.push_back(iter);
                }
                while (!bufbool[0]) {
                    if (send(sockfd,  _msg.c_str(), HEADERSIZE + HASHSIZE, 0) == -1) {
                        perror("recv");

                    }
                    if (recv(sockfd, bufbool, 1, 0) == -1)
                    {
                        perror("recv");
                    }
                    bufbool[0] = atoi(bufbool);
                }

                usleep(100);
                if (send(sockfd,
                         msg.substr(i * MAXDATASIZE, i * MAXDATASIZE + msg.size() % MAXDATASIZE).c_str(),
                         msg.size() % MAXDATASIZE,
                         0) == -1) {
                    perror("recv");
                    break;
                }
            }
            else {
                if (send(sockfd, "64", HEADERSIZE, 0) == -1)
                {
                    perror("recv");
                    break;
                }
                usleep(100);
                if (send(sockfd, msg.substr(i * MAXDATASIZE, (i + 1) * MAXDATASIZE).c_str(),
                         MAXDATASIZE, 0) == -1) {
                    perror("recv");
                    break;
                }
            }

       }
       usleep(100);
    }
}





// получение структуры sockaddr, IPv4 или IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(IP_ADDR, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Проходим через все результаты и соединяемся к первому возможному
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connectn");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // эта структура больше не нужна
    //send(sockfd, msg, MAXDATASIZE, 0);
    /*std::string strbuf;
    std::cout << "Packages: ";
    std::cin >> strbuf;
    send(sockfd, strbuf.c_str(), HEADERSIZE, 0);
    int packeges = atoi(strbuf.c_str());
    while(true){
        if (packeges <= 0)
            break;
        std::cout << "Package length: ";
        std::cin >> strbuf;
        if (send(sockfd, strbuf.c_str(), HEADERSIZE, 0) == -1)
        {
            perror("recv");
            break;
        }
        int length = atoi(strbuf.c_str());
        std::cout << "Package: ";
        std::cin >> strbuf;
        if (send(sockfd, strbuf.c_str(), length, 0) == -1)
        {
            perror("recv");
            break;
        }
        packeges--;
    }*/
    memset(buf, 0, MAXDATASIZE);
    std::string msg;
    std::cin >> msg;

    sendMSG(sockfd, msg);

    close(sockfd);
    return 0;
}

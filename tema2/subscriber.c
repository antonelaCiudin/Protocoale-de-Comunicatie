#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "utils.h"
#include "helpers.h"

void usage(char *file)
{
    fprintf(stderr, "Usage: %s server_address server_port\n", file);
    exit(0);
}

double myPow(int a, int b) {
    double result = 1;
    for(int i = 0; i < b; i++)
        result *= 10;
    return (double)(a/result);
}

void printMessage(struct messageUDP message) {
    char sign;
    double myShort, myFloat;
    uint32_t myInt, mod;
    uint8_t pow;

    printf("%s:%d - ", message.ip, message.port);
    printf("%s - ", message.topic);
    if (message.typeDate == 0) {
        printf("INT - ");
        sign = message.buffer[0];
        myInt = ntohl((*(uint32_t *)(message.buffer + 1)));
        if (sign == 1)
            myInt = -myInt;
        printf("%d\n", myInt);
    }
    if (message.typeDate == 1) {
        printf("SHORT REAL - ");
        // uint16_t myInt = (*(uint16_t *)(message.buffer));
        myShort = ntohs((*(uint16_t *)(message.buffer)));
        myShort /= 100;
        printf("%.2lf\n", myShort);
    }
    if (message.typeDate == 2) {
        printf("FLOAT - ");
        sign = message.buffer[0];
        mod = ntohl((*(uint32_t *)(message.buffer + 1)));
        pow = (uint8_t)(message.buffer[5]);
        myFloat = myPow(mod, pow);
        if (sign == 1)
            myFloat = -myFloat;
        printf("%.16g\n", myFloat);
    }
    if (message.typeDate == 3) {
        printf("STRING - %s\n", message.buffer);
    }
}

void closeClient(int socket, fd_set readFDS, fd_set tmpFDS, int servSocket) {
    FD_CLR(socket, &readFDS);
    FD_CLR(socket, &tmpFDS);
    FD_CLR(servSocket, &readFDS);
    FD_CLR(servSocket, &tmpFDS);
    close(servSocket);
}

void sendSubReq(char command[70], int commLen, int servSocket) {
    int n;
    char *topic = malloc(50);
    messageTCP message;

    for (int i = 10; i < commLen - 2; i++)
        topic[i - 10] = command[i];

    message.type = SUBSCRIBE;
    strncpy(message.topicName, topic, 50);
    message.SF = atoi(&command[commLen - 2]);

    n = send(servSocket, &message, sizeof(messageTCP), 0);
    DIE( n < 0, "sent");

    printf("subscribed %s\n", topic);
}

void sendUnsubReq(char command[70], int commLen, int servSocket) {
    int n;
    char *topic = malloc(50);
    messageTCP message;

    for (int i = 12; i < commLen; i++)
        topic[i - 12] = command[i];

    message.type = UNSUBSCRIBE;
    strcpy(message.topicName, topic);
    message.SF = -1;

    n = send(servSocket, &message, sizeof(messageTCP), 0);
    DIE( n < 0, "sent");

    printf("unsubscribed %s\n", topic);
}

int main(int argc, char *argv[])
{
    char command[70];
    int commLen;
    int servSocket, n, i, ret;
    struct sockaddr_in servAddr;

    struct messageUDP newUdpMess;

    fd_set readFDS;
    fd_set tmpFDS;

    int fdmax;

    FD_ZERO(&tmpFDS);
    FD_ZERO(&readFDS);

    if (argc < 4) {
        usage(argv[0]);
    }

    servSocket = socket(AF_INET, SOCK_STREAM, 0);
    DIE(servSocket < 0, "socket");

    FD_SET(servSocket, &readFDS);
    fdmax = servSocket;
    FD_SET(STDIN_FILENO, &readFDS);

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(atoi(argv[3]));
    ret = inet_aton(argv[2], &servAddr.sin_addr);
    DIE(ret == 0, "inet_aton");

    ret = connect(servSocket, (struct sockaddr*) &servAddr, sizeof(servAddr));
    DIE(ret < 0, "connect");

    n = send(servSocket, argv[1], strlen(argv[1]), 0);
    DIE( n < 0, "sent");

    while (1) {
    // citeste comanda
        tmpFDS = readFDS;

        ret = select(fdmax + 1, &tmpFDS, NULL, NULL, NULL);
        DIE(ret < 0, "Select socket");

        for ( i = 0; i <= fdmax; i++){
            if (FD_ISSET(i, &tmpFDS)){
                if ( i == STDIN_FILENO){
                    // se citeste de la tastatura
                    fgets(command, COMM_LEN - 1, stdin);
                    commLen = strlen(command) - 1;

                    //se ichide clientul
                    if (strncmp (command, "exit", 4) == 0){
                        closeClient(i, readFDS, tmpFDS, servSocket);

                        return 0;

                        n = send(servSocket, command, strlen(command), 0);
                        DIE( n < 0, "sent");
                    }
                    else if (strncmp(command, "subscribe", 9) == 0) {
                        sendSubReq(command, commLen, servSocket);
                    }
                    else if (strncmp(command, "unsubscribe",11) == 0) {
                        sendUnsubReq(command, commLen, servSocket);
                    }
                    else {
                        printf("Invalid command\n");
                    }
                }
                //s-a primit un mesaj udp de la server
                else if (i == servSocket) {
                    ret = recv(servSocket, &newUdpMess, sizeof(newUdpMess), 0);
                    DIE( ret < 0, "recv");

                    if (ret == 0) {
                        closeClient(i, readFDS, tmpFDS, servSocket);

                        return 0;
                    }
                    else if (strncmp(newUdpMess.topic, "error", 5) == 0){
                        printf("The id is already in use!\n");
                        closeClient(i, readFDS, tmpFDS, servSocket);

                        return 0;
                    }
                    //se primeste un mesaj udp de la server
                    else {
                        printMessage(newUdpMess);
                    }
                }
            }
        }
    }
    close(servSocket);

    return 0;
}
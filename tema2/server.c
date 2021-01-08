#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "utils.h"
#include "helpers.h"

client *connectedClients;
topic *topics;
int topSize, cliSize;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

void addTopic(char topicName[50]) {
	//tabela de mesaje udp e goala, se aloca memorie
	if (topics == NULL) {
		topSize = 1;
		topics = malloc(sizeof(topic));
	}
	//in caz contrar, se realoca memorie
	else {
		topSize++;
		topics = realloc(topics,
			sizeof(topic)*topSize);
	}

	strcpy(topics[topSize - 1].topicName, topicName);
	topics[topSize - 1].subscribers = NULL;
}

//se adauga un client in lista de clienti online
int connectClient(char clientID[10], int cliSocket) {
	//daca exista asa client in lista de 
	for (int i = 0; i < cliSize; i++) {
		if (strcmp(connectedClients[i].clientID, clientID) == 0) {
			if (connectedClients[i].online == 0) {
				connectedClients[i].online = 1;
				connectedClients[i].socket = cliSocket;
				return 1;
			}
			else 
				return 0;
		}
	}
	//tabela de clienti tcp e goala, se aloca memorie
	if (connectedClients == NULL) {
		cliSize = 1;
		connectedClients = malloc(sizeof(client));
	}
	//in caz contrar, se realoca memorie
	else {
		cliSize++;
		connectedClients = realloc(connectedClients,
			sizeof(client)*cliSize);
	}

	//se adauga clientul
	for (int i = 0; i < 10; i++)
		connectedClients[cliSize - 1].clientID[i] = clientID[i];
	connectedClients[cliSize - 1].socket = cliSocket;
	connectedClients[cliSize - 1].online = 1;

	return 1;
}

void disconnectClient(int cliSocket) {
	int i;

	for (i = 0; i < cliSize; i++) {
		if (connectedClients[i].socket == cliSocket) {
			connectedClients[i].online = 0;
		}
	}
}

char *findClientID(int socket) {
	char *id = malloc(10);
	for (int i = 0; i < cliSize; i++)
		if (connectedClients[i].socket == socket) {
			strcpy(id, connectedClients[i].clientID);
			return id;
		}
	return NULL;
}

void updateTopics(int socket, messageTCP *message, char clientID[10]) {
	int i;

	//se cauta topicul in tabela
	//daca asa topic exista, se adauga sau se sterge clientul
	if (topics != NULL) {
		for (i = 0; i < topSize; i++) {
			if (strcmp(topics[i].topicName, message->topicName) == 0) {
				if (message->type == SUBSCRIBE) {
					if (contains(topics[i].subscribers, clientID) == 0)
						topics[i].subscribers = addCliToList(clientID, 
							message->SF, topics[i].subscribers);
					return;
				}
				else if (message->type == UNSUBSCRIBE) {
					topics[i].subscribers = deleteClient(clientID, topics[i].subscribers);
					return;
				}
			}
		}
	}

	//daca topicul nu exista, se adauga si clientul
	if (message->type == SUBSCRIBE) {
		addTopic(message->topicName);
		topics[topSize - 1].subscribers = addCliToList(clientID, 
			message->SF, topics[topSize].subscribers);
	}
}

void closeSockets(int fdmax, int socketTCP, int socketUDP,
	fd_set *readFDS, fd_set *tmpFDS) {
	char *message = NULL;
	int n;

	for (int i = fdmax; i >= 0; i--) {
		if (FD_ISSET(i, tmpFDS)) {
    		if (i != socketTCP && i != socketUDP && i != STDIN_FILENO) {
        		n = send(i, &message, sizeof(message), 0);
            	DIE(n < 0, "sent");
            }

            FD_CLR(i, readFDS);
            FD_CLR(i, tmpFDS);
            close(i);
        }
    }
    FD_ZERO(readFDS);
    FD_ZERO(tmpFDS);
}

client *findConnClient(char clientID[10]) {
	for (int i = 0; i < cliSize; i++) {
		if (strcmp(connectedClients[i].clientID, clientID) == 0)
			return &connectedClients[i];
	}
	return NULL;
}

void sendUdpToClient(struct messageUDP message) {
	int topLength;

	for (int i = 0; i < topSize; i++) {
		topLength = strlen(topics[i].topicName);
		if (strncmp(topics[i].topicName, 
			message.topic, topLength) == 0) {

			//se parcurge lista de abonati la topic
			clientsList tmpSubscribers = topics[i].subscribers;

			while(tmpSubscribers != NULL) {
				//se cauta clientul in tabela de clienti online
				client *client = findConnClient(tmpSubscribers->clientID);

				//daca clientul e online, se trimite mesajul
				if (client->online == 1) {
                    message.next = NULL;
					send(client->socket, &message, sizeof(message), 0);

				}
				//daca se doreste, se pastreaza mesajul pentru cand clientul revine
				else if (client->online == 0 && tmpSubscribers->SF == 1) {
					tmpSubscribers->messToSend = addMessToList(message, tmpSubscribers->messToSend);
				}
				tmpSubscribers = tmpSubscribers->next;
			}
			return;
		}

	}
}

void sendRestUDP(int socket) {
	for (int i = 0; i < topSize; i++) {
		clientsList tmpSubscribers = topics[i].subscribers;

		//se cauta clientul in topic
		while(tmpSubscribers != NULL) {
			//se verifica daca s-a gasit clientul cu asa id
			if (strcmp(tmpSubscribers->clientID, findClientID(socket)) == 0) {
				if (tmpSubscribers->SF == 1) {
					//se trimit mesajele
					while(tmpSubscribers->messToSend != NULL) {
						send(socket, (tmpSubscribers->messToSend), sizeof(struct messageUDP), 0);

						tmpSubscribers->messToSend = tmpSubscribers->messToSend->next;
					}
				}
			}
			tmpSubscribers = tmpSubscribers->next;
		}
	} 
}

int main(int argc, char *argv[])
{
	char buffer[BUFLEN];
	char command[10];
	int n, i, ret;
	int socketUDP, socketTCP, portno, newsocket;
	struct sockaddr_in servAddr, cliAddr;
	socklen_t cliLen;
	struct messageUDP newUdpMess;
	messageTCP *newTcpMess;

	fd_set readFDS;	// multimea de citire folosita in select()
	fd_set tmpFDS;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea readFDS
	
	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (readFDS) si multimea temporara (tmpFDS)
	FD_ZERO(&readFDS);
	FD_ZERO(&tmpFDS);

	socketUDP = socket(AF_INET, PF_INET, 0);
	DIE(socketUDP < 0, "socketUDP");

	socketTCP = socket(AF_INET, SOCK_STREAM, 0);
	DIE(socketTCP < 0, "socketTCP");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(portno);
	servAddr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(socketUDP, (struct sockaddr *) &servAddr, sizeof(struct sockaddr));
	DIE(ret < 0, "udp_bind");
	ret = bind(socketTCP, (struct sockaddr *) &servAddr, sizeof(struct sockaddr));
	DIE(ret < 0, "tcp_bind");
	ret = listen(socketTCP, MAX_CLIENTS);
	DIE(ret < 0, "tcp_listen");

	// se adauga fiecare socket in multimea readFDS
	FD_SET(STDIN_FILENO, &readFDS);
	FD_SET(socketUDP, &readFDS);
	FD_SET(socketTCP, &readFDS);
	fdmax = socketTCP;

	while (1) {
		tmpFDS = readFDS; 
		
		ret = select(fdmax + 1, &tmpFDS, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmpFDS)) {
				if (i == STDIN_FILENO) {
					// se citeste de la tastatura
                    memset(command, 0, BUFLEN);
                    fgets(command, BUFLEN - 1, stdin);

                    if (strncmp (command, "exit", 4) == 0){
                		closeSockets(fdmax, socketTCP, socketUDP,
                			&readFDS, &tmpFDS);
                    	
                    	return 0;
                    }
                    else
                    	printf("Invalid command\n");
				}
				else if (i == socketUDP) {
					memset(&buffer, 0, sizeof(buffer));
					ret = recvfrom(socketUDP, buffer, BUFLEN, 0, (struct sockaddr*)&cliAddr, &cliLen);

					memcpy(newUdpMess.topic, buffer, 50);
					strcpy(newUdpMess.ip, inet_ntoa(cliAddr.sin_addr));
					if (strlen(&buffer[51]) == 1500)
						strcat(buffer, "\0");
					memcpy(newUdpMess.buffer, &buffer[51], 1501);
					newUdpMess.typeDate = buffer[50];
					newUdpMess.port = ntohs(cliAddr.sin_port);
					
					//se trimite la clientii tcp
					sendUdpToClient(newUdpMess);
				}
				else if (i == socketTCP) {
					// se accepta noua conexiune tcp
					cliLen = sizeof(cliAddr);
					newsocket = accept(socketTCP, (struct sockaddr *) &cliAddr, &cliLen);
					DIE(newsocket < 0, "accept");

					// se adauga clientul nou in 
					FD_SET(newsocket, &readFDS);
					if (newsocket > fdmax) { 
						fdmax = newsocket;
					}

					char clientID[10];
					memset(clientID, 0, 10);

					ret = recv(newsocket, &clientID, 10, 0);
					ret = connectClient(clientID, newsocket);

					//se inchide conexiunea daca exista client online cu asa id
					if (ret == 0) {
						char notification[BUFLEN];
						strcpy(notification, "error");
						strcat(notification, clientID);
						n = send(newsocket, &notification, sizeof(notification), 0);
                        DIE( n < 0, "sent");
						close(newsocket);
						FD_CLR(newsocket, &readFDS);
					}

					else {
						sendRestUDP(newsocket);

						printf("New client (%s) connected from %s:%d\n", clientID,
							inet_ntoa(cliAddr.sin_addr), ntohs(cliAddr.sin_port));
					}
				}
				else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						// conexiunea s-a inchis
						printf("Client (%s) disconnected\n", findClientID(i));
						close(i);

						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &readFDS);

						//se sterge din lista de clienti online
						disconnectClient(i);
					} else {
						newTcpMess = (messageTCP *)buffer;

						updateTopics(i, newTcpMess, findClientID(i));
					}


				}
			}
		}
	}

	close(socketUDP);
	close(socketTCP);

	return 0;
}

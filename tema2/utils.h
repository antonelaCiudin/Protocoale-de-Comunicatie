#ifndef UTILS_H
#define UTILS_H

typedef struct clientTCP *clientsList;
typedef struct messageUDP *messagesList;

struct clientTCP
{
  char clientID[10];
  int SF;
  messagesList messToSend;
  clientsList next;
};

struct messageUDP
{
	char topic[50];
	char typeDate;
	char buffer[1501];
	char ip[20];
	int port;
	messagesList next;
};

typedef struct {
	char topic[50];
	char typeDate;
	char buffer[1500];
} messageUDP;

typedef struct {
	int type;
	char topicName[50];
	int SF;
} messageTCP;

typedef struct {
	char clientID[10];
	int socket;
	int online;
} client;

typedef struct {
	char topicName[50];
	clientsList subscribers;
} topic;

extern clientsList addCliToList(char clientID[10], int SF, clientsList l);
extern clientsList deleteClient(char clientID[10], clientsList l);
extern int contains(clientsList l, char clientID[10]);
extern clientsList freeCliList(clientsList l);
extern messagesList addMessToList(struct messageUDP message, messagesList l);
extern messagesList removeMess(messagesList l);
extern messagesList freeMessList(messagesList l);

#endif

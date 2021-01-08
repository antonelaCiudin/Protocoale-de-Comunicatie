#include "utils.h"
#include <stdlib.h>

clientsList addCliToList(char clientID[10], int SF, clientsList l)
{
	clientsList temp = malloc(sizeof(struct clientTCP));
	strcpy(temp->clientID, clientID);
	temp->SF = SF;
	temp->messToSend = NULL;
	temp->next = l;

	return temp;
}

clientsList deleteClient(char clientID[10], clientsList l) {
	clientsList temp = l, aux;

	if (temp == NULL)
		return l;

	if (strcmp(temp->clientID, clientID) == 0)
		return temp->next;

	while (temp->next != NULL) {
		if (strcmp(temp->next->clientID, clientID) == 0) {
			aux = temp->next;
			temp->next = temp->next->next;
			free(aux);
			return l;
		}
		temp = temp->next;
	}
	return l;
}

int contains(clientsList l, char clientID[10]) {
	clientsList temp = l;

	while(temp != NULL) {
		if(strcmp(temp->clientID, clientID) == 0)
			return 1;
		temp = temp->next;
	}
	return 0;
}

clientsList freeCliList(clientsList l)
{
	clientsList temp = l->next; 
	free(l);
	return temp;
}

messagesList addMessToList(struct messageUDP message, messagesList l)
{
	// print_message(message);
	messagesList temp = malloc(sizeof(struct messageUDP));

	strcpy(temp->topic, message.topic);
	temp->typeDate = message.typeDate;
	memcpy(temp->buffer, message.buffer, 1500);
	strcpy(temp->ip, message.ip);
	temp->port = message.port;
	temp->next = l;

	return temp;
}

messagesList removeMess(messagesList l) {
	if (l == NULL)
		return l;
	messagesList aux = l;
	l = l->next;
	free(aux);
	return l;
}

messagesList freeMessList(messagesList l)
{
	messagesList temp = l->next; 
	free(l);
	return temp;
}


#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "parson.h"
#include "helpers.h"
#include "requests.h"

#define HOST "3.8.116.10"
#define PORT 8080

//trimite mesaj de tip get
char *sendGetMessage(char *url, char *cookie, char *auth) {
	int socket;
	char *message, *servResponse;
	socket = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

	message = compute_get_request(HOST, url, NULL, cookie, auth);
	send_to_server(socket, message);
	servResponse = receive_from_server(socket);
	printf("%s\n", servResponse);

	close(socket);

	return servResponse;
}

//trimite mesaj de tip post
char *sendPostMessage(char *jsonString, char *url, char *auth) {
	int socket;
	char *message, *servResponse;
	socket = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

	message = compute_post_request(HOST, url, "application/json", jsonString, NULL, auth);
	send_to_server(socket, message);
	servResponse = receive_from_server(socket);
	printf("%s\n", servResponse);

	close(socket);

	return servResponse;
}

//trimite mesaj de tip delete
char *sendDeleteMessage(char *url, char *auth) {
	int socket;
	char *message, *servResponse;
	socket = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

	message = compute_delete_request(HOST, url, "application/json", auth);
	send_to_server(socket, message);
	servResponse = receive_from_server(socket);
	printf("%s\n", servResponse);

	close(socket);

	return servResponse;
}

//creeaza obiectul json cu username si password
JSON_Value *getUsernPassw() {
	char username[100], password[100];
	printf("username=");
	scanf("%s", username);
	printf("password=");
	scanf("%s", password);
	JSON_Value *jsonValue = json_value_init_object();
	JSON_Object *jsonObject = json_value_get_object(jsonValue);
	json_object_set_string(jsonObject, "username", username);
	json_object_set_string(jsonObject, "password", password);

	return jsonValue;
}

//alipeste id-ul citit de la tastaura la url
char *getURL() {
	printf("id=");
	char *id = malloc(10);
	scanf("%s", id);
	char *url = malloc(50);
	strcpy(url, "/api/v1/tema/library/books/");
	strcat(url, id);
	free(id);

	return url;
}

//formeaza un string din obiectul json care contine 
//informatia despre o carte noua
char *getBookInfo() {
	char *title = malloc(100), *author = malloc(50), 
		*genre = malloc(50), *page_count = malloc(10), 
		*publisher = malloc(50), reader[10];

	fgets(reader, 10, stdin);
	printf("title=");
	fgets(title, 100, stdin);
	title = strtok(title, "\n");
	printf("author=");
	fgets(author, 100, stdin);
	author = strtok(author, "\n");
	printf("genre=");
	fgets(genre, 100, stdin);
	genre = strtok(genre, "\n");
	printf("page_count=");
	fgets(page_count, 50, stdin);
	page_count = strtok(page_count, "\n");
	printf("publisher=");
	fgets(publisher, 100, stdin);
	publisher = strtok(publisher, "\n");

	JSON_Value *jsonValue = json_value_init_object();
	JSON_Object *jsonObject = json_value_get_object(jsonValue);
	json_object_set_string(jsonObject, "title", title);
	json_object_set_string(jsonObject, "author", author);
	json_object_set_string(jsonObject, "genre", genre);
	json_object_set_number(jsonObject, "page_count", (double)atoi(page_count));
	json_object_set_string(jsonObject, "publisher", publisher);

	free(title);
	free(author);
	free(genre);
	free(page_count);
	free(publisher);

	return (json_serialize_to_string(jsonValue));
}

//mesaj pentru eroarea de autentificare
void errorAuth() {
	printf("\nYou do not have access!\n\n");
}

//mesaj pentru eroarea de logare
void errorLogin() {
	printf("\nYou are not logged in!\n\n");
}

int main(int argc, char const *argv[])
{
	char *command = malloc(50);
	char *servResponse;
	char *cookie = NULL;
	char *token = NULL;
	char *p;

	while(1) {
		//se citeste comanda de la tastatura
		memset(command, 0, 50);
		scanf("%s", command);
		if (strcmp(command, "register") == 0) {
			sendPostMessage(json_serialize_to_string(getUsernPassw()), 
				"/api/v1/tema/auth/register", NULL);
		}
		else if (strcmp(command, "login") == 0) {
			if (cookie != NULL) {
				printf("\nYou are already logged in to an account!\n\n");
				continue;
			}
			servResponse = sendPostMessage(json_serialize_to_string(getUsernPassw()),
				"/api/v1/tema/auth/login", NULL);

			//se extrage cookie-ul din raspunsul serverului
			p = strstr(servResponse, "Set-Cookie");
			if(p != NULL) {
				p += 12;
				cookie = strtok(p, " ");
			}	
		}
		else if (strcmp(command, "enter_library") == 0) {
			if (cookie == NULL) {
				errorLogin();
				continue;
			}
			servResponse = sendGetMessage("/api/v1/tema/library/access", cookie, NULL);

			//se extrage token-ul din raspunsul serverului
			p = strstr(servResponse, "{");
			JSON_Value *jsonValue = json_parse_string(p);
			JSON_Object *jsonObject = json_value_get_object(jsonValue);
			token = (char*)json_object_get_string(jsonObject, "token");
		}
		else if (strcmp(command, "get_books") == 0) {
			if (cookie == NULL) {
				errorLogin();
				continue;
			}
			if (token == NULL) {
				errorAuth();
				continue;
			}
			servResponse = sendGetMessage("/api/v1/tema/library/books", NULL, token);
		}
		else if (strcmp(command, "get_book") == 0) {
			if (cookie == NULL) {
				errorLogin();
				continue;
			}
			if (token == NULL) {
				errorAuth();
				continue;
			}
			
			servResponse = sendGetMessage(getURL(), NULL, token);
		}
		else if (strcmp(command, "add_book") == 0) {
			if (cookie == NULL) {
				errorLogin();
				continue;
			}
			if (token == NULL) {
				errorAuth();
				continue;
			}
			servResponse = sendPostMessage(getBookInfo(),
				"/api/v1/tema/library/books", token);
		}
		else if (strcmp(command, "delete_book") == 0) {
			if (cookie == NULL) {
				errorLogin();
				continue;
			}
			if (token == NULL) {
				errorAuth();
				continue;
			}
			servResponse = sendDeleteMessage(getURL(), token);
		}
		else if (strcmp(command, "logout") == 0) {
			servResponse = sendGetMessage("/api/v1/tema/auth/logout", cookie, NULL);
			cookie = NULL;
			token = NULL;
		}
		else if (strcmp(command, "exit") == 0) {
			break;
		}
		else {
			printf("\nInvalid command!\n\n");
		}
	}

	return 0;
}
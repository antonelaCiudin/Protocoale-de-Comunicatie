#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "utils.c"

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		1600	// dimensiunea maxima a calupului de date
#define COMM_LEN	70	//dimensiunea maxima a unei comenzi
#define MAX_CLIENTS	5	// numarul maxim de clienti in asteptare
#define SUBSCRIBE 1
#define UNSUBSCRIBE 0

#endif

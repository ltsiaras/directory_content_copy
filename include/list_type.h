#ifndef __LIST__TYPE__
#define __LIST__TYPE__
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>

typedef struct data
{
	int socket;
	char *path;
	pthread_mutex_t *client_mtx;
}data_t;

int equal(data_t Elem1, char *str);	
int lesser(data_t Elem1, data_t Elem2);
int greater(data_t Elem1, data_t Elem2);

void set_value (data_t *target, data_t source);
void set_value_string(data_t *target, char **string);
void write_value(data_t data);
#endif

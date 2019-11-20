#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "../include/list_type.h"


void set_value (data_t *target, data_t source)
{
	int len = strlen(source.path) + 1;
	target->socket = source.socket;
	target->path = malloc(len);
	strcpy(target->path, source.path);
	target->client_mtx = source.client_mtx;
}

void write_value(data_t data)
{
	printf("%s\n", data.path);
	printf("%p\n", data.client_mtx);
}

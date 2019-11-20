#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <malloc.h>
#include <pthread.h>
#include <errno.h>
#include "../include/list.h"

#define MAX_CONN 15
#define perror2(s,e) fprintf(stderr, "%s: %s\n", s, strerror(e))


typedef struct queue_t
{
	data_t *data;
	int start;
	int end;
	int count;
	int size;
}queue_t;

pthread_mutex_t queue_mtx;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;
queue_t queue;


void initialize(queue_t *queue, int size)
{
	queue->start = 0;
	queue->end = -1;
	queue->count = 0;
	queue->size = size;
	queue->data = malloc(size*sizeof(data_t));
}

void free_queue(queue_t *queue)	/*Free queue resources*/
{
	free(queue->data);
	queue = NULL;
}
void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

int write_all(int fd, void *buff, size_t size)	//writes all size bytes
{
    int sent, n;
    for (sent = 0; (unsigned) sent < size; sent += n) 
	{
        if ((n = write(fd, (char*) buff + sent, size - sent)) == -1)
            return -1;
		if (n == 0)
			return sent;
    }
    return sent;
}

int read_all(int fd, void *buff, size_t size)	//reads all size bytes
{
    int get, n;
    for (get = 0; (unsigned) get < size; get += n) 
	{
        if ((n = read(fd, (char*) buff + get, size - get)) == -1)
            return -1;
		if (n == 0)
			return get;
    }
    return get;
}

void rec_read(int socket, char *pathname, pthread_mutex_t *mtx, info_p *list, data_t *data)	//recursive reading
{
	int rdir = 0;
	long name_max;
	DIR *dir = NULL;
	struct dirent *entry = NULL, *result;
	if ((name_max = pathconf(pathname, _PC_NAME_MAX)) == -1)
	{
		perror("Can't set name");
		exit(4);
	}

	if ((entry = malloc(offsetof(struct dirent, d_name) + name_max + 1)) == NULL)
	{
		perror("Can't allocate memory");
		exit(1);
	}

	if ((dir = opendir(pathname)) == NULL)
	{
		perror("Can't open directory");
		exit(2);	
	}

	while (1)
	{
		
		if ((rdir = readdir_r(dir, entry, &result)) == -1)
		{
			perror("Can't read directory");
			exit(2);
		}
	
		if (result == NULL)
			break;

		char *d_name;

		d_name = result->d_name;

		if (result->d_type & DT_DIR)
		{
		
			if (strcmp(result->d_name, ".") != 0 && strcmp(result->d_name, "..") != 0)
			{
				int len, path_length;
				char path[name_max];
 
				len = strlen(pathname);
				if (pathname[len - 1] == '/')
                	path_length = snprintf(path, name_max, "%s%s", pathname, d_name);
				else
					path_length = snprintf(path, name_max, "%s/%s", pathname, d_name);

                if (path_length >= name_max) 
				{
                    fprintf (stderr, "Path length is too long.\n");
                    exit (EXIT_FAILURE);
                }

                rec_read(socket, path, mtx, list, data);
            }
        }
		else
		{
			int len, error, path_length;
            char path[name_max];

            len = strlen(pathname);
            if (pathname[len - 1] == '/')
            	path_length = snprintf (path, name_max, "%s%s", pathname, d_name);
			else
				path_length = snprintf(path, name_max, "%s/%s", pathname, d_name);

			if (path_length >= name_max) 
			{
                fprintf (stderr, "Path length is too long.\n");
                exit (EXIT_FAILURE);
            }
			list_node_p current = NULL;
			data->socket = socket;
			data->path = path;
			data->client_mtx = mtx;
			LIST_insert(list, *data, current, &error);
			LIST_last(*list, &current, &error);
		}
	}

	if (dir != NULL)
		closedir(dir);
	free(entry);
}

void place(queue_t *queue, data_t *data)	//places data to queue
{
	pthread_mutex_lock(&queue_mtx);
	while (queue->count >= queue->size)	//waits on condition
	{
		printf(">> Queue is Full\n");
		pthread_cond_wait(&cond_nonfull, &queue_mtx);
	}
	queue->end = (queue->end + 1) % queue->size;
	queue->data[queue->end].socket = data->socket;
	queue->data[queue->end].path = malloc(strlen(data->path) + 1);
	strcpy(queue->data[queue->end].path, data->path);
	queue->data[queue->end].client_mtx = data->client_mtx;
	queue->count++;
	pthread_mutex_unlock(&queue_mtx);
}

void obtain(queue_t *queue, data_t *data)	//gets data from queue
{
	pthread_mutex_lock(&queue_mtx);
	while (queue->count <= 0)	//waits on condition
	{
		printf(">> Queue is Empty\n");
		pthread_cond_wait(&cond_nonempty, &queue_mtx);

	}
	data->socket = queue->data[queue->start].socket;
	data->path = malloc(strlen(queue->data[queue->start].path) + 1);
	strcpy(data->path, queue->data[queue->start].path);
	data->client_mtx = queue->data[queue->start].client_mtx;
	free(queue->data[queue->start].path);
	queue->start = (queue->start + 1) % queue->size;
	queue->count--;
	pthread_mutex_unlock(&queue_mtx);
}

void *first_line_thread_function(void *ptr)	//threads for 1st communication with client
{
	int error, socket, length, nread, nwrite;
	char *path;
	pthread_mutex_t *client_mtx = malloc(sizeof(pthread_mutex_t));	//mutex unique to client

	if ((error = pthread_detach(pthread_self())))
	{
		perror2("pthread_detach", error);
		exit(1);
	}

	socket = *(int*) ptr;
	free(ptr);
	if ((error = pthread_mutex_init(client_mtx, 0)) != 0)
	{
		perror2("mutex initialization error", error);
		exit(3);
	}

	if ((error = pthread_mutex_lock(client_mtx)) != 0)
	{
		perror2("mutex lock error", error);
		exit(3);
	}

	if ((nread = read_all(socket, &length, sizeof(int))) == -1)
	{
		perror("read from socket");
		exit(3);
	}

	path = malloc(length);
	if ((error = (nread = read_all(socket, path, length))) == -1)
	{
		perror2("read from socket", error);
		exit(3);
	}
	if ((error = pthread_mutex_unlock(client_mtx)) != 0)
	{
		perror2("mutex unlock error", error);
		exit(3);
	}
	info_p list = NULL;
	list_node_p current = NULL;
	data_t data;
	
	list = LIST_create();
	printf("[Thread:  %ld]:  About to scan directory %s\n", pthread_self(), path);
	rec_read(socket, path, client_mtx, &list, &data);
	free(path);

	if ((error = pthread_mutex_lock(client_mtx)) != 0)
	{
		perror2("mutex lock error", error);
		exit(3);
	}
	/*writes number of files*/
	if ((nwrite = write_all(socket, &list->size, sizeof(int))) == -1)
	{
		perror2("write error", -1);
		exit(2);
	}
	if ((error = pthread_mutex_unlock(client_mtx)) != 0)
	{
		perror2("mutex unlock error", error);
		exit(3);
	}
	LIST_traverse(list);
	while (list->size > 0)	//Put item from list to queue
	{
		LIST_first(list, &current, &error);
		printf("[Thread:  %ld]:  Adding file %s to the queue\n", pthread_self(), current->data.path);
		place(&queue, &current->data);
		pthread_cond_broadcast(&cond_nonempty);
		LIST_delete(&list, &current, &error);
	}
	pthread_exit(0);
}

void *worker_threads_function(void *ptr)	//worker threads
{
	
	int error, fd, length, i, nread, nwrite;
	long page_size = sysconf(_SC_PAGESIZE);
	char buff[page_size];
	if ((error = pthread_detach(pthread_self())))
	{
		perror2("pthread_detach", error);
		exit(1);
	}

	data_t data;
	while (1)
	{
		obtain(&queue, &data);
		printf("Worker thread will break\n");
		printf("[Thread:  %ld]:  Received task: <%s, %d>\n", pthread_self(), data.path, data.socket); 
		pthread_cond_broadcast(&cond_nonfull);

		/*Sending task to client*/
		if ((error = pthread_mutex_lock(data.client_mtx)) != 0)
		{
			perror2("mutex lock error", error);
			exit(3);
		}
		length = strlen(data.path) + 1;

		/*Writing path length to client*/
		if ((error = (nwrite = write_all(data.socket, &length, sizeof(int)))) == -1)
		{
			perror2("writing to sockect", error);
			exit(3);
		}

		if ((error = (nwrite = write_all(data.socket, data.path, length))) == -1)
		{
			perror2("writing to sockect", error);
			exit(3);
		}
		
		/*Writing actual file*/
		if ((error = (fd = open(data.path, O_RDONLY))) == -1)
		{
			perror2("open file failed", error);
			exit(3);
		}
		i = 1;
		while ((nread = read_all(fd, buff, page_size)) > 0)
		{
			if ((error = (nwrite = write_all(data.socket, &i, sizeof(int)))) == -1)
			{
				perror2("writing to socket", error);
				exit(3);
			}

			/*Number of bytes to read*/
			if ((error = (nwrite = write_all(data.socket, &nread, sizeof(int)))) == -1)
			{
				perror2("writing to socket", error);
				exit(3);
			}			
			
			if ((error = (nwrite = write_all(data.socket, buff, nread))) == -1)
			{
				perror2("writing to socket", error);
				exit(3);
			}
			memset(buff, 0, strlen(buff));
		}
		close(fd);
		free(data.path);
		printf("Finished sending file to client\n");
		i = 0;
		/*sends to client to stop reading*/
		if ((error = (nwrite = write_all(data.socket, &i, sizeof(int)))) == -1)
		{
			perror2("writing to socket", error);
			exit(3);
		}
		if ((nread = read_all(data.socket, &i, sizeof(int))) == -1)
		{
			perror2("reading from socket", nread);
			exit(3);
		}
		if ((error = pthread_mutex_unlock(data.client_mtx)) != 0)
		{
			perror2("mutex unlock error", error);
			exit(3);
		}
		/*Client has no other files*/
		if (i == 0)
		{
			if ((error = pthread_mutex_destroy(data.client_mtx)) != 0)
			{
				perror2("error in destroying client mutex", error);
				exit(3);	
			}
			close(data.socket);
		}
		memset(buff, 0, strlen(buff));
	}
	
	pthread_exit(0);
}

int main(int argc, char *argv[])
{

	extern char *optarg;
	extern int optind;

    int ch, pflag = 0, sflag = 0, qflag = 0, error = 0, port, sock, newsock, thread_pool_size, queue_size, i;
	static char usage[] = "usage: %s -p <port> -s <thread_pool_size> -q <queue_size>\n";
	pthread_t *worker_threads, first_line_threads;

    struct sockaddr_in server;
    struct sockaddr *serverptr=(struct sockaddr *)&server;

    while((ch = getopt(argc, argv, "p:s:q:")) != -1)
		switch(ch)
		{
			case 'p':
				pflag = 1;
				port = atoi(optarg);
				break;
			case 's':
				sflag = 1;
				thread_pool_size = atoi(optarg);
				break;
			case 'q':
				qflag = 1;
				queue_size = atoi(optarg);
				break;
			case '?':
				error = 1;
				break;
		}

		if (pflag == 0)
		{
			fprintf(stderr, "%s: missing -p option\n", argv[0]);
			fprintf(stderr, usage, argv[0]);
			exit(1);
		}
		else if (sflag == 0)
		{
			fprintf(stderr, "%s: missing -s option\n", argv[1]);
			fprintf(stderr, usage, argv[1]);
			exit(1);
		}
		else if (qflag == 0)
		{
			fprintf(stderr, "%s: missing -q option\n", argv[2]);
			fprintf(stderr, usage, argv[2]);
			exit(1);
		}  
		else if (error) 
		{ 
			fprintf(stderr, usage, argv[0]); 
			exit(1); 
		}

	initialize(&queue, queue_size);
	/*Initialize mutexes and condition variables*/
	pthread_mutex_init(&queue_mtx, 0);
	if (pthread_cond_init(&cond_nonfull, 0) != 0)
	{
		perror("failed to initialize condition variable");
		exit(2);
	}
	if (pthread_cond_init(&cond_nonempty, 0) != 0)
	{
		perror("failed to initialize condition variable");
		exit(2);
	}
	worker_threads = malloc(thread_pool_size*sizeof(pthread_t));
	/*Creating worker threads*/
	for (i = 0; i < thread_pool_size; i++)
		pthread_create(&worker_threads[i], 0, worker_threads_function, 0);

    /* Create socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        perror_exit("socket");
    server.sin_family = AF_INET;       /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);      /* The given port */

    /* Bind socket to address */
    if (bind(sock, serverptr, sizeof(server)) < 0)
        perror_exit("bind");

	printf("Server's parameters are:\n");
	printf("		port: %d\n", port);
	printf("		thread_pool_size: %d\n", thread_pool_size);
	printf("		queue_size: %d\n", queue_size);

    /* Listen for connections */
    if (listen(sock, MAX_CONN) < 0) 
		perror_exit("listen");

    printf("Listening for connections to port %d\n", port);

    while (1) 
	{
        /* accept connection */
    	if ((newsock = accept(sock, NULL, NULL)) < 0) 
			perror_exit("accept");

		int *socket = malloc(sizeof(int));
		*socket = newsock;
		pthread_create(&first_line_threads, 0, first_line_thread_function, socket);
    }
	pthread_mutex_destroy(&queue_mtx);
	free_queue(&queue);
	free(worker_threads);
	close(sock);
	return 0;
}

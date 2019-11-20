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
#include <sys/stat.h>
#include <malloc.h>
#include <errno.h>

void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}


int write_all(int fd, void *buff, size_t size)
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

int read_all(int fd, void *buff, size_t size)
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

void make_dir(char *name)	//makes directories
{
	int mdir;
	if ((mdir = mkdir(name, 0755)) == -1)
	{
		if (errno != EEXIST)
		{
			perror("Can't create directory");
			exit(1);
		}
	}
}

void parse_string(char *path)	//parses given path
{
	char *str;
	int i, len;
	len = strlen(path) + 1;
	str = malloc(len);
	for (i = 0; i < len; i++)
	{
		if (path[0] == '/')
		{
			str[0] = '/';
			continue;
		}
		else if (path[i] != '/')
			str[i] = path[i];
		else if (path[i] == '/')
		{
			make_dir(str);
			str[i] = path[i];
		}
	}
	free(str);
}

int main(int argc, char *argv[]) 
{
    int ch, iflag = 0, pflag = 0, dflag = 0, error = 0, server_port, sock, length, num_of_files, i, fd, j, nread, nwrite;
    char *buff = NULL, *path, *server_name, *directory;
	static char usage[] = "usage: %s -i <server_ip> -p <server_port> -d <directory>\n";

    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
    
	while((ch = getopt(argc, argv, "i:p:d:")) != -1)
		switch(ch)
		{
			case 'i':
				iflag = 1;
				server_name = optarg;
				break;
			case 'p':
				pflag = 1;
				server_port = atoi(optarg);
				break;
			case 'd':
				dflag = 1;
				directory = optarg;
				break;
			case '?':
				error = 1;
				break;
		}

		if (iflag == 0)
		{
			fprintf(stderr, "%s: missing -i option\n", argv[0]);
			fprintf(stderr, usage, argv[0]);
			exit(1);
		}
		else if (pflag == 0)
		{
			fprintf(stderr, "%s: missing -p option\n", argv[1]);
			fprintf(stderr, usage, argv[1]);
			exit(1);
		}
		else if (dflag == 0)
		{
			fprintf(stderr, "%s: missing -d option\n", argv[2]);
			fprintf(stderr, usage, argv[2]);
			exit(1);
		}  
		else if (error) 
		{ 
			fprintf(stderr, usage, argv[0]); 
			exit(1); 
		}

	printf("Client's parameters are:\n");
	printf("		server name: %s\n", server_name);
	printf("		port: %d\n", server_port);
	printf("		directory: %s\n", directory);	

	/* Create socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    	perror_exit("socket");

	/* Find server address */
    if ((rem = gethostbyname(server_name)) == NULL) 
	{	
		herror("gethostbyname");
		exit(1);
    }

    server.sin_family = AF_INET;       /* Internet domain */
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(server_port);         /* Server port */

    /* Initiate connection */
    if (connect(sock, serverptr, sizeof(server)) < 0)
	   perror_exit("connect");
    printf("Connecting to %s on port %d\n", server_name, server_port);


	/*Sending desired directory to server*/
	length = strlen(directory) + 1;
	if ((nwrite = write_all(sock, &length, sizeof(int))) == -1)
	{
		perror("write error");
		exit(1);
	}
	printf("Path i am writing to server is %s\n", directory);
	if ((nwrite = write_all(sock, directory, length)) == -1)
	{
		perror("write error");
		exit(1);
	}
	
	if ((nread = read_all(sock, &num_of_files, sizeof(int))) == -1)
	{
		perror("read error");
		exit(1);
	}
	printf("Got number of files %d\n", num_of_files);

	for(i = 0; i < num_of_files; i++)
	{
		j = num_of_files - 1;
		/*Getting path length*/
		printf("About to read from socket %d\n", sock);
		if ((nread = read_all(sock, &length, sizeof(int))) == -1)
		{
			perror("read error");
			exit(1);
		}
		path = malloc(length);
		memset(path, 0, length);
		/*Getting path*/
		if ((nread = read_all(sock, path, length)) == -1)
		{
			perror("read error");
			exit(1);
		}
		parse_string(path);
		printf("Received path %s\n", path);
		
		while ((fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1)
		{
			if (errno == EEXIST)
			{
				int error;
				if ((error = unlink(path)) == -1)
				{
					perror("Can't delete file");
					exit(3);
				}
			}
			else
			{
				perror("Error opening file for writing");
				exit(3);
			}
		}

		int nbyte, k;

		while(1)	//reads until k = 0
		{
			if ((nread = read_all(sock, &k, sizeof(int))) == -1)
			{
				perror("reading from socket");
				exit(3);
			}
			if (k == 0)
				break;
			
			if ((nread = read_all(sock, &nbyte, sizeof(int))) == -1)
			{
				perror("reading from socket");
				exit(3);
			}
			buff = realloc(buff, nbyte);
			memset(buff, 0, nbyte);
			if ((nread = read_all(sock, buff, nbyte)) == -1)
			{
				perror("reading from socket");
				exit(3);
			}

			if ((nwrite = write_all(fd, buff, nbyte)) == -1)
			{
				perror("Can't write to file");
				exit(4);
			}
			memset(buff, 0, nbyte);
		}

		close(fd);
		free(path);
		printf("Finished receiving file from server\n");

		if ((nwrite = write_all(sock, &j, sizeof(int))) == -1)
		{
			perror("Can't write to file");
			exit(4);
		}
		j--;
	}
    close(sock);
	free(buff);
	return 0;
}

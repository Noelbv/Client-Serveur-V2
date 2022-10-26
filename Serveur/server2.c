#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "server2.h"
#include "client2.h"

// static char* getTime()
// {
//    char* buffer = malloc(21);
//    time_t timestamp = time(NULL);
//    strftime(buffer, 21, "%d/%m/%Y %H:%M:%S", localtime(&timestamp));
//    printf("%s\n", buffer);
//    return(buffer);
//    free(buffer);
// }

static void logMessage(char *text, const char *filename)
{
	FILE *logFile = NULL;
	logFile = fopen(filename, "a");
	if (logFile == NULL)
	{
		printf("Impossible d'écrire dans le fichier");
	}
	time_t secs;
	secs = time(NULL);
	fprintf(logFile, "%ld;%s\n", secs, text);
	fclose(logFile);
}

static void init(void)
{
#ifdef WIN32
	WSADATA wsa;
	int err = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (err < 0)
	{
		puts("WSAStartup failed !");
		exit(EXIT_FAILURE);
	}
#endif
}

static void end(void)
{
#ifdef WIN32
	WSACleanup();
#endif
}

static void app(void)
{
	SOCKET sock = init_connection();
	char buffer[BUF_SIZE];
	/* the index for the array */
	int actual = 0;
	int max = sock;
	/* an array for all clients */
	Client clients[MAX_CLIENTS];
	system("clear");
	fd_set rdfs;
	char all_clients[1000][BUF_SIZE];
	FILE *fptr = NULL;
	int i = 0;
	int total_clients = 0;
	if (fptr = fopen("clients_db", "r"))
	{
		while (fgets(all_clients[i], BUF_SIZE, fptr))
		{
			all_clients[i][strlen(all_clients[i]) - 1] = '\0';
			i++;
		}
		fclose(fptr);
	} else {
		logMessage("", "clients_db");
	}
	total_clients = i;

	if (fptr = fopen("groupes_db", "r"))
	{
		// while (fgets(all_clients[i], BUF_SIZE, fptr))
		// {
		// 	all_clients[i][strlen(all_clients[i]) - 1] = '\0';
		// 	i++;
		// }
		fclose(fptr);
	} else {
		logMessage("", "groupes_db");
	}

	if (fptr = fopen("broadcast_logs", "r"))
	{
		fclose(fptr);
	} else {
		logMessage("", "broadcast_logs");
	}


	while (1)
	{
		i = 0;
		FD_ZERO(&rdfs);

		/* add STDIN_FILENO */
		FD_SET(STDIN_FILENO, &rdfs);

		/* add the connection socket */
		FD_SET(sock, &rdfs);

		/* add socket of each client */
		for (i = 0; i < actual; i++)
		{
			FD_SET(clients[i].sock, &rdfs);
		}

		if (select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
		{
			perror("select()");
			exit(errno);
		}

		/* something from standard input : i.e keyboard */
		if (FD_ISSET(STDIN_FILENO, &rdfs))
		{
			fgets(buffer, BUF_SIZE - 1, stdin);
			{
				char *p = NULL;
				p = strstr(buffer, "\n");
				if (p != NULL)
				{
					*p = 0;
				}
				else
				{
					/* fclean */
					buffer[BUF_SIZE - 1] = 0;
				}
			}
			if (!strcmp(buffer, "exit"))
			{
				break;
			}
		}
		else if (FD_ISSET(sock, &rdfs))
		{
			/* new client */
			SOCKADDR_IN csin = {0};
			size_t sinsize = sizeof csin;
			int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
			if (csock == SOCKET_ERROR)
			{
				perror("accept()");
				continue;
			}

			/* after connecting the client sends its name */
			if (read_client(csock, buffer) == -1)
			{
				/* disconnected */
				continue;
			}

			/* what is the new maximum fd ? */
			max = csock > max ? csock : max;

			FD_SET(csock, &rdfs);

			Client c = {csock};
			strncpy(c.name, buffer, BUF_SIZE - 1);
			clients[actual] = c;
			char *message = (char *)malloc(sizeof(c.name));
			strcpy(message, c.name);
			strcat(message, " is connected !");
			// logMessage(message, LOG);
			send_hist_to_client(clients, actual);
			send_message_to_all_clients(clients, c, actual, message, 1);
			send_message_to_a_client(clients, -1, actual, "---------You are connected !---------", 1);
			if (actual > 0)
			{
				message = (char *)malloc(BUF_SIZE);
				strcpy(message, "Also connected: ");
				for (i = 0; i < actual; ++i)
				{
					strncat(message, clients[i].name, BUF_SIZE - sizeof(message));
					strncat(message, " ", BUF_SIZE - sizeof(message));
				}
				send_message_to_a_client(clients, -1, actual, message, 1);
			}
			actual++;
		}
		else
		{
			int i = 0;
			for (i = 0; i < actual; i++)
			{
				/* a client is talking */
				if (FD_ISSET(clients[i].sock, &rdfs))
				{
					Client client = clients[i];
					int c = read_client(clients[i].sock, buffer);
					/* client disconnected */
					if (c == 0)
					{
						closesocket(clients[i].sock);
						remove_client(clients, i, &actual);
						strncpy(buffer, client.name, BUF_SIZE - 1);
						strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
						send_message_to_all_clients(clients, client, actual, buffer, 1);
					}
					else
					{
						char *str;
						str = (char *)malloc(sizeof(buffer));
						strcpy(str, buffer);
						const char *separator = ">";
						char *target = strtok(str, separator);
						int message_len = strlen(buffer) - strlen(target) - 2;
						if (message_len == -2)
						{
							send_message_to_a_client(clients, -1, i, "ERROR, wrong format ! [target|all]> [message]", 1);
							break;
						}
						char *message;
						message = (char *)malloc(message_len);
						strncpy(message, buffer + strlen(target) + 2, message_len);

						if (!strcmp(target, "all"))
						{
							send_message_to_all_clients(clients, client, actual, message, 0);
						}
						else
						{
							int j = 0;
							while (strcmp(target, clients[j].name) && j < actual)
							{
								j++;
							}
							if (j == actual)
							{
								send_message_to_a_client(clients, -1, i, "ERROR, target not found !", 1);
							}
							else
							{
								send_message_to_a_client(clients, i, j, message, 0);
							}
						}
					}
					break;
				}
			}
		}
	}

	clear_clients(clients, actual);
	end_connection(sock);
}

static void clear_clients(Client *clients, int actual)
{
	int i = 0;
	for (i = 0; i < actual; i++)
	{
		closesocket(clients[i].sock);
	}
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
	/* we remove the client in the array */
	memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
	/* number client - 1 */
	(*actual)--;
}

static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server)
{
	int i = 0;
	char message[BUF_SIZE];
	message[0] = 0;
	if (from_server == 0)
	{
		strncpy(message, sender.name, BUF_SIZE - 1);
		strncat(message, " : ", sizeof message - strlen(message) - 1);
	}
	strncat(message, buffer, sizeof message - strlen(message) - 1);
	for (i = 0; i < actual; i++)
	{
		/* we don't send message to the sender */
		if (sender.sock != clients[i].sock)
		{
			write_client(clients[i].sock, message);
		}
	}
	strncat(message, "\n", sizeof message - strlen(message) - 1);
	printf("%s\n", message);

	if (from_server)
	{
		// CASE 1 - connect & disconnect
		logMessage(message, "server_logs");
	}
	else
	{
		// CASE 2 - broadcasts
		logMessage(message, "broadcast_logs");
	}
}

static void send_message_to_a_client(Client *clients, int sender, int receiver, const char *buffer, char from_server)
{
	char message[BUF_SIZE];
	message[0] = 0;
	if (!from_server)
	{
		strncpy(message, "[PRIVATE] ", BUF_SIZE - 1);
		strncat(message, clients[sender].name, BUF_SIZE - 1);
		strncat(message, ": ", sizeof message - strlen(message) - 1);
		printf("%s to %s : ", clients[sender].name, clients[receiver].name);
	}
	else
	{
		printf("Server to %s: ", clients[receiver].name);
	}
	printf("%s\n", buffer);
	strncat(message, buffer, sizeof message - strlen(message) - 1);
	write_client(clients[receiver].sock, message);
	strncat(message, "\n", sizeof message - strlen(message) - 1);
	if (from_server)
	{
		// CASE 1 - server messages
		logMessage(message, "server_logs");
	}
	else
	{
		// CASE 2 - private messages
		char *path = malloc(1000);
		strcpy(path, "users/");
		strcat(path, clients[receiver].name);
		logMessage(message, path);
		free(path);
		path = malloc(1000);
		strcpy(path, "users/");
		strcat(path, clients[sender].name);
		logMessage(message, path);
		free(path);
	}
}

static const char *getfield(char *line, int num)
{
	const char *tok;
	for (tok = strtok(line, ";");
		 tok && *tok;
		 tok = strtok(NULL, ";\n"))
	{
		if (!--num)
			return tok;
	}
	return NULL;
}

static void send_hist_to_client(Client *clients, int receiver)
{
	char *path = malloc(BUF_SIZE);
	strcpy(path, "users/");
	strcat(path, clients[receiver].name);
	FILE *fichier_public = fopen("broadcast_logs", "r");
	FILE *fichier_perso = fopen(path, "r");
	if (fichier_perso == NULL){
		logMessage("",path);
	}
	char line[BUF_SIZE];
	char *historique_public[1000];
	char * message_perso; 
	int i = 0;
	int timestamp_perso;
	int timestamp_public[1000];
	if (fichier_public != NULL)
	{
		while (fgets(line, BUF_SIZE, fichier_public) != NULL) // On lit le fichier tant qu'on ne reçoit pas d'erreur (NULL)
		{
			char *tmp = strdup(line);
			timestamp_public[i] = atoi(getfield(tmp, 1));
			historique_public[i] = (char *)malloc(BUF_SIZE);
			tmp = strdup(line);
			tmp = (char *)getfield(tmp, 2);
			strcpy(historique_public[i], tmp);
			strcat(historique_public[i],"\n");
			i++;
		}
		fclose(fichier_public);
	}
	int j = 0;
	int k = 0;
	if (fichier_perso != NULL)
	{
		while (fgets(line, BUF_SIZE, fichier_perso) != NULL) // On lit le fichier tant qu'on ne reçoit pas d'erreur (NULL)
		{
			char *tmp = strdup(line);
			timestamp_perso = atoi(getfield(tmp, 1));
			while (k < i && timestamp_public[k] < timestamp_perso)
			{
				write_client(clients[receiver].sock, historique_public[k]);
				k++;
			}
			message_perso = (char*) malloc(BUF_SIZE);
			tmp = strdup(line);
			strcpy(message_perso, getfield(tmp, 2));
			strcat(message_perso, "\n");
			write_client(clients[receiver].sock, message_perso);
			free(message_perso);
		}
		fclose(fichier_perso);
	}
	while (k < i)
	{
		write_client(clients[receiver].sock, historique_public[k]);
		k++;
	}
	free(path);
}

static int init_connection(void)
{
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN sin = {0};

	if (sock == INVALID_SOCKET)
	{
		perror("socket()");
		exit(errno);
	}

	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(PORT);
	sin.sin_family = AF_INET;

	if (bind(sock, (SOCKADDR *)&sin, sizeof sin) == SOCKET_ERROR)
	{
		perror("bind()");
		exit(errno);
	}

	if (listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
	{
		perror("listen()");
		exit(errno);
	}
	logMessage("Server started\n", "server_logs");

	return sock;
}

static void end_connection(int sock)
{
	closesocket(sock);
}

static int read_client(SOCKET sock, char *buffer)
{
	int n = 0;

	if ((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
	{
		perror("recv()");
		/* if recv error we disonnect the client */
		n = 0;
	}

	buffer[n] = 0;

	return n;
}

static void write_client(SOCKET sock, const char *buffer)
{
	if (send(sock, buffer, strlen(buffer), 0) < 0)
	{
		perror("send()");
		exit(errno);
	}
}

int main(int argc, char **argv)
{
	init();

	app();

	end();

	return EXIT_SUCCESS;
}

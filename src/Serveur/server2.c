#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "server2.h"
#include "client2.h"
#include "group.h"

static int total_clients;
static int actual;
// tous les clients qui se sont déjà enregistrés
static char all_clients[1000][BUF_SIZE];

static void logToFile(char *text, const char *filename)
{
	FILE *logFile = NULL;
	logFile = fopen(filename, "a");
	if (logFile == NULL)
	{
		printf("Impossible d'écrire dans le fichier");
	}
	if (strlen(text) > 0)
	{
		time_t secs;
		secs = time(NULL);
		fprintf(logFile, "%ld;%s\n", secs, text);
	}
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
	if (mkdir("users", 0777) != -1)
		logToFile("users folder created !", "data/server_logs");
	SOCKET sock = init_connection();
	char buffer[BUF_SIZE];
	/* le nombre actuel de clients en ligne*/
	actual = 0;
	// le nombre actuel de groupes
	int nb_groups = 0;
	int max = sock;
	/* an array for all clients */
	Client clients[MAX_CLIENTS];
	// On impose une limite arbitraire de 50 groupes pour gérer plus facielement la mémoire allouée
	Group groups[50];
	system("clear");
	fd_set rdfs;
	
	FILE *fptr = NULL;
	int i = 0;
	// nombre de clients total
	total_clients = 0;
	if (fptr = fopen("data/users_db", "r"))
	{
		while (fgets(all_clients[i], BUF_SIZE, fptr))
		{
			all_clients[i][strlen(all_clients[i]) - 1] = '\0';
			i++;
		}
		fclose(fptr);
	}
	else
	{
		logToFile("", "data/clients_db");
	}
	total_clients = i;

	if (fptr = fopen("data/groups_db", "r"))
	{
		// while (fgets(all_clients[i], BUF_SIZE, fptr))
		// {
		// 	all_clients[i][strlen(all_clients[i]) - 1] = '\0';
		// 	i++;
		// }
		fclose(fptr);
	}
	else
	{
		logToFile("", "data/groups_db");
	}

	if (fptr = fopen("data/broadcast_logs", "r"))
	{
		fclose(fptr);
	}
	else
	{
		logToFile("", "data/broadcast_logs");
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

			/* new client already in DB ? */
			for (i = 0; i < total_clients; i++)
			{
				if (!strcmp(all_clients[i], c.name))
					break;
			}
			if (i == total_clients)
			{
				/* if not add it to the db */
				strcpy(all_clients[i], c.name);
				total_clients++;
				fptr = fopen("data/users_db", "a");
				fputs(c.name, fptr);
				fputc('\n', fptr);
				fclose(fptr);
			}

			char *message = (char *)malloc(BUF_SIZE);
			strcpy(message, c.name);
			strcat(message, " is connected !");
			send_hist_to_client(clients, actual);
			send_message_to_all_clients(clients, c, message, TRUE);
			write_client(clients[actual].sock, "----You are connected !----\n");
			if (actual > 0)
			{
				// who is also there ?
				message = (char *)malloc(BUF_SIZE);
				strcpy(message, "Also connected: ");
				for (i = 0; i < actual; ++i)
				{
					strncat(message, clients[i].name, BUF_SIZE - strlen(message));
					strncat(message, " ", BUF_SIZE - strlen(message));
				}
				send_message_to_a_client(clients, -1, actual, message, TRUE);
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
						send_message_to_all_clients(clients, client, buffer, TRUE);
					}
					else
					{
						// client writing a message
						char *str;
						str = (char *)malloc(BUF_SIZE);
						strcpy(str, buffer);
						if (str[0] == '#')
						/* On crée un groupe */
						{
							int j;
							for (j = 0; str[j]; j++)
							{
								str[j] = str[j + 1];
							}
							logToFile(strcat(strcat(str, " "), clients[i].name), "data/groups_db");
							const char *separator = " ";
							char *strToken = strtok(str, separator);
							char *nomGroupe;
							char members[10][BUF_SIZE];
							strcpy(members[0], clients[i].name);
							int k = 0;
							while ((strToken != NULL) && (k < 10))
							{
								// On demande le token suivant.
								if (k == 0)
								{
									nomGroupe = strndup(strToken, BUF_SIZE);
									printf("%s\n", nomGroupe);
								}
								else
								{
									printf("%s\n", strToken);
									strcpy(members[k], strToken);
								}
								strToken = strtok(NULL, separator);
								k++;
							}
							create_group(groups, nb_groups, nomGroupe, members, k - 1);
							nb_groups++;
							break;
						}
						const char *separator = ">";

						// check for syntax
						if (strstr(str, separator) == NULL || !strcmp(strstr(str, separator), str))
						{
							send_message_to_a_client(clients, -1, i, "ERROR, wrong format ! [target|all]> [message]", TRUE);
							break;
						}
						char *target = strtok(str, separator);
						int message_len = strlen(buffer) - strlen(target) - 2;
						if (message_len < 1)
						{
							send_message_to_a_client(clients, -1, i, "ERROR, wrong format ! [target|all]> [message]", TRUE);
							break;
						}
						char *message;
						message = (char *)malloc(message_len + 1);
						strncpy(message, buffer + strlen(target) + 2, message_len);
						message[message_len] = 0;

						// is it a broadcast message ?
						if (!strcmp(target, "all"))
						{
							send_message_to_all_clients(clients, client, message, FALSE);
						}
						else
						{
							int i_online = 0;
							int i_offline = 0;
							int i_groups = 0;
							while (strcmp(target, clients[i_online].name) && i_online < actual)
							{
								i_online++;
							}
							while (strcmp(target, all_clients[i_offline]) && i_offline < total_clients)
							{
								i_offline++;
							}
							while (strcmp(target, groups[i_groups].name) && i_groups < nb_groups)
							{
								i_groups++;
							}

							if (i_groups != nb_groups)
							{
								send_message_to_a_group(clients, clients[i].name, groups[i_groups], message);
							}
							else if (i_online != actual)
							{
								send_message_to_a_client(clients, i, i_online, message, FALSE);
							}
							else if (i_offline != total_clients)
							{
								send_message_to_offline_client(clients[i].name, all_clients[i_offline], message);
							}
							else
							{
								send_message_to_a_client(clients, -1, i, "ERROR, target not found !", TRUE);
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

static void send_message_to_offline_client(const char *sender, const char *receiver, const char *message)
{
	char log[BUF_SIZE];
	strcpy(log, "[PRIVATE] ");
	strncat(log, sender, BUF_SIZE - strlen(log) - strlen(sender));
	strncat(log, ": ", BUF_SIZE - strlen(message) - 2);
	strncat(log, message, BUF_SIZE - strlen(log) - strlen(message));
	char path[BUF_SIZE];
	strcpy(path, "users/");
	strncat(path, receiver, BUF_SIZE - 7);
	logToFile(log, path);
}

static void send_message_to_all_clients(Client *clients, Client sender, const char *buffer, char from_server)
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
		logToFile(message, "data/server_logs");
	}
	else
	{
		// CASE 2 - broadcasts
		logToFile(message, "data/broadcast_logs");
	}
}

static void send_message_to_a_client(Client *clients, int sender, int receiver, const char *buffer, char from_server)
{
	char message[BUF_SIZE];
	message[0] = 0;
	if (!from_server)
	{
		strncpy(message, "[PRIVATE] ", BUF_SIZE - 11);
		strncat(message, clients[sender].name, BUF_SIZE - strlen(message) - strlen(clients[sender].name));
		strncat(message, ": ", sizeof message - strlen(message) - 2);
		printf("%s to %s : ", clients[sender].name, clients[receiver].name);
	}
	else
	{
		printf("Server to %s: ", clients[receiver].name);
	}
	printf("%s\n", buffer);
	strncat(message, buffer, sizeof message - strlen(message) - strlen(buffer));
	write_client(clients[receiver].sock, message);
	strncat(message, "\n", sizeof message - strlen(message) - 1);
	if (from_server)
	{
		// CASE 1 - server messages
		logToFile(message, "data/server_logs");
	}
	else
	{
		// CASE 2 - private messages
		char *path = malloc(1000);
		strcpy(path, "users/");
		strcat(path, clients[receiver].name);
		logToFile(message, path);
		free(path);
		if (sender != receiver)
		{
			path = malloc(1000);
			strcpy(path, "users/");
			strcat(path, clients[sender].name);
			logToFile(message, path);
			free(path);
		}
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
	FILE *fichier_public = fopen("data/broadcast_logs", "r");
	FILE *fichier_perso = fopen(path, "r");
	if (fichier_perso == NULL)
	{
		logToFile("", path);
	}
	char line[BUF_SIZE];
	char *historique_public[1000];
	char *message_perso;
	int i = 0;
	int timestamp_perso;
	int timestamp_public[1000];
	if (fichier_public != NULL)
	{
		while (fgets(line, BUF_SIZE, fichier_public) != NULL) // On lit le fichier tant qu'on ne reçoit pas d'erreur (NULL)
		{
			if (strlen(line) > 1)
			{
				char *tmp = strndup(line, BUF_SIZE);
				timestamp_public[i] = atoi(getfield(tmp, 1));
				historique_public[i] = (char *)malloc(BUF_SIZE);
				tmp = strndup(line, BUF_SIZE);
				tmp = (char *)getfield(tmp, 2);
				strcpy(historique_public[i], tmp);
				strcat(historique_public[i], "\n");
				i++;
			}
		}
		fclose(fichier_public);
	}
	int j = 0;
	int k = 0;
	if (fichier_perso != NULL)
	{
		while (fgets(line, BUF_SIZE, fichier_perso) != NULL) // On lit le fichier tant qu'on ne reçoit pas d'erreur (NULL)
		{
			if (strlen(line) > 1)
			{
				char *tmp = strndup(line, BUF_SIZE);
				timestamp_perso = atoi(getfield(tmp, 1));
				while (k < i && timestamp_public[k] < timestamp_perso)
				{
					write_client(clients[receiver].sock, historique_public[k]);
					k++;
				}
				message_perso = (char *)malloc(BUF_SIZE);
				tmp = strndup(line, BUF_SIZE);
				strcpy(message_perso, getfield(tmp, 2));
				strcat(message_perso, "\n");
				write_client(clients[receiver].sock, message_perso);
				free(message_perso);
			}
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

static void create_group(Group *groups, int nb_of_groups, const char *nom, char members[10][BUF_SIZE], int numberOfMembers)
{
	Group newGroup;
	newGroup.size = numberOfMembers;
	memcpy(&newGroup.name, nom, BUF_SIZE);
	int i;
	for (i = 0; i < numberOfMembers; ++i)
	{
		memcpy(newGroup.members[i], members[i], BUF_SIZE);
	}
	memcpy(&newGroup.members, members, BUF_SIZE);
	groups[nb_of_groups] = newGroup;
	nb_of_groups++;
}

static void send_message_to_a_group(Client *clients, const char *sender, Group group, const char *message)
{
	char msg[BUF_SIZE];
	msg[0] = 0;
	strncpy(msg, "[", BUF_SIZE - strlen(msg) - 1);
	strncat(msg, group.name, BUF_SIZE - strlen(msg) - 1);
	strncat(msg, "] ", BUF_SIZE - strlen(msg) - 2);
	strncat(msg, sender, BUF_SIZE - strlen(msg) - strlen(sender));
	strncat(msg, " : ", BUF_SIZE - strlen(msg) - 3);
	strncat(msg, message, BUF_SIZE - strlen(msg) - strlen(message));
	strncat(msg, "\n", BUF_SIZE - strlen(msg) - 1);
	int i = 0;
	for (i; i < group.size; i++)
	{
		char *target = (char*) malloc(strlen(group.members[i])+1);
		strcpy(target, group.members[i]);
		int i_online = 0;
		int i_offline = 0;
		while (strcmp(target, clients[i_online].name) && i_online < actual)
		{
			i_online++;
		}
		while (strcmp(target, all_clients[i_offline]) && i_offline < total_clients)
		{
			i_offline++;
		}
		if (i_online != actual)
		{
			write_client(clients[i].sock, msg);
		}
		else if (i_offline != total_clients)
		{
			send_message_to_offline_client(clients[i].name, all_clients[i_offline], msg);
		}
	}
}

static int init_connection(void)
{
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN sin = {0};

	int reuse = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

#ifdef SO_REUSEPORT
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse, sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEPORT) failed");
#endif

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
	logToFile("Server started\n", "data/server_logs");

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

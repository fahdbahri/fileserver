/* C header files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <getopt.h>

/* Socket API headers */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Definitions */
#define DEFAULT_BUFLEN 512
#define PORT 4529

int main(int argc, char *argv[])
{
    int server, client;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int length, fd, rcnt, optval;
    char recvbuf[DEFAULT_BUFLEN], bmsg[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // assigning option,  port, password and directory
    unsigned short port = 0;
    char *password = NULL;
    char *directory = NULL;
    int option;

    while ((option = getopt(argc, argv, "p:d:w:")) != -1)
    {
        switch (option)
        {
        case 'p':
            port = atoi(optarg);
            break;
        case 'd':
            directory = strdup(optarg);
            break;
        case 'w':
            password = strdup(optarg);
            break;
        default:
            fprintf(stderr, "usage: %s -p port_number -d directory_name -w password\n", argv[0]);
            exit(1);
        }
    }

    if (port == 0 || directory == NULL || password == NULL)
    {
        fprintf(stderr, "usage: %s -p port_number -d directory_name -w password\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Access the specified directory
    DIR *dir = opendir(directory);
    if (!dir)
    {
        perror("Error opening a directory");
        exit(EXIT_FAILURE);
    }
    // Read directory contents
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);

    FILE *file;
    file = fopen(password, "r");
    char line[100];

    if(file == NULL)
    {
        printf("The file is not opened.\n");
        exit(0);
    }

    while(fgets(line, sizeof(line), file) != NULL)
    {
        char *username = strtok(line, ":");
        char *password = strtok(NULL, ":");

        printf("Username: %s, password: %s", username, password);
    }

    fclose(file);

    /* Open socket descriptor */
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Can't create socket!");
        return (1);
    }

    /* Fill local and remote address structure with zero */

    memset(&local_addr, 0, sizeof(local_addr));
    memset(&remote_addr, 0, sizeof(remote_addr));

    /* Set values to local_addr structure */
    local_addr.sin_family = AF_INET;
    if (port > 0)
        local_addr.sin_port = htons(port);
    else
        local_addr.sin_port = htons(PORT);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // set SO_REUSEADDR on a socket to true (1):
    optval = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

    if (bind(server, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        /* could not start server */
        perror("Bind problem!");
        return (1);
    }

    if (listen(server, SOMAXCONN) < 0)
    {
        perror("Listen");
        exit(1);
    }

    printf("\nPort number: %d\n", ntohs(local_addr.sin_port));
    printf("Directory: %s\n", directory);
    printf("Password: %s\n", password);

    printf("Wait for connection\n");

    while (1)
    { // main accept() loop
        length = sizeof remote_addr;
        if ((fd = accept(server, (struct sockaddr *)&remote_addr, &length)) == -1)
        {
            perror("Accept Problem!");
            continue;
        }

        printf("Server: got connection from %s\n",
               inet_ntoa(remote_addr.sin_addr));

        // Receive until the peer shuts down the connection
        do
        {
            // Clear Receive buffer
            memset(&recvbuf, '\0', sizeof(recvbuf));
            rcnt = recv(fd, recvbuf, recvbuflen, 0);
            if (rcnt > 0)
            {
                printf("Bytes received: %d\n", rcnt);

                // Echo the buffer back to the sender
                rcnt = send(fd, recvbuf, rcnt, 0);
                if (rcnt < 0)
                {
                    printf("Send failed:\n");
                    close(fd);
                    break;
                }
                printf("Bytes sent: %d\n", rcnt);
            }
            else if (rcnt == 0)
                printf("Connection closing...\n");
            else
            {
                printf("Receive failed:\n");
                close(fd);
                break;
            }
        } while (rcnt > 0);
    }

    // Final Cleanup
    close(server);
}
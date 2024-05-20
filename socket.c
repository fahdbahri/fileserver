/* C header files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <getopt.h>
#include <unistd.h>
#include <pthread.h>
/* Socket API headers */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// difine a structure to hold the server

struct server_setting
{
    const char *passwordFile;
    const char *directory;
};

struct client_info
{
    int fd;
    struct server_setting setting;
};

// functions for commands
void parse_command(char *command, char *username, char *password, char *buffer);
void user_command(const char *passwordFile, char *username, char *password, char *response);
void put_command(int fd, const char *initial_buffer, int authenticated);
void *handle_connection(void *arg);

/* Definitions */
#define DEFAULT_BUFLEN 512
#define PORT 4529
#define END_MARKER "\r\n.\r\n"
#define FILE_PATH_LENGTH 256

int main(int argc, char *argv[])
{
    int server, client;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int length, fd, bytes_read, optval;
    char buffer[DEFAULT_BUFLEN];
    int bufferlen = DEFAULT_BUFLEN;

    // assigning option,  port, password and directory
    unsigned short port = 0;
    char *passwordFile = NULL;
    char *directory = NULL;
    int option;

    // Parse commmand-line arguments
    while ((option = getopt(argc, argv, "d:p:u:")) != -1)
    {
        switch (option)
        {
        case 'p':
            port = atoi(optarg);
            break;
        case 'd':
            directory = optarg;
            break;
        case 'u':
            passwordFile = optarg;
            break;
        default:
            fprintf(stderr, "usage: %s -d directory_name -p port_number -u password\n", argv[0]);
            exit(1);
        }
    }

    // Validate command-line arguments
    if (port == 0 || directory == NULL || passwordFile == NULL)
    {
        fprintf(stderr, "usage: %s -d directory_name -p port_number -u password\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct server_setting setting = {passwordFile, directory};

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

    // Bind the socket
    if (bind(server, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        /* could not start server */
        perror("Bind problem!");
        return (1);
    }

    // Start Listening for incoming connections
    if (listen(server, SOMAXCONN) < 0)
    {
        perror("Listen");
        exit(1);
    }

    printf("\nFile server Listening on localhost port %d\n", ntohs(local_addr.sin_port));

    printf("Wait for connection\n");

    while (1)
    {
        // Accept incoming connection
        length = sizeof(remote_addr);
        fd = accept(server, (struct sockaddr *)&remote_addr, &length);
        if (fd == -1)
        {
            perror("Accept Problem!");
            continue;
        }

        printf("Server: got connection from %s\n",
               inet_ntoa(remote_addr.sin_addr));

        // Receive until the peer shuts down the connection

        // create a new thread
        pthread_t tid;
        struct client_info *c_info = malloc(sizeof(struct client_info));
        c_info->fd = fd;
        c_info->setting.directory = strdup(directory);
        c_info->setting.passwordFile = strdup(passwordFile);
        if (pthread_create(&tid, NULL, handle_connection, c_info) != 0)
        {
            perror("Thread creation");
            close(fd);
            free(c_info->setting.passwordFile);
            free(c_info->setting.directory);
            free(c_info);

            continue;
        }
        else
        {
            pthread_detach(tid);
        }
    }
    close(server);
    return 0;
}
void parse_command(char *command, char *username, char *password, char *buffer)
{
    sscanf(buffer, "%s %s %s", command, username, password);
    printf("Command is %s, username are %s, password is %s\n", command, username, password);
}

void user_command(const char *passwordFile, char *username, char *password, char *response)
{

    // open the password File
    FILE *file;
    file = fopen(passwordFile, "r");
    char line[100];
    int user_found = 0;

    if (file == NULL)
    {
        // handle the file error
        sprintf(response, "The file is not opened.\n");
        fclose(file);
        return;
    }

    // Read lines from the password file
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *username_file = strtok(line, ":");
        char *password_file = strtok(NULL, ":");

        if (strcmp(username_file, username) == 0 &&
            strcmp(password_file, password) == 0)
        {
            // User authenticated
            sprintf(response, "200");
            user_found = 1;
            break;
        }
    }

    // if the user found, send unauthorized response
    if (!user_found)
    {
        sprintf(response, "401 Unauthorized. Please authenticate first. \n");
    }

    fclose(file);
}

void put_command(int fd, const char *initial_buffer, int authenticated)
{
    char response[DEFAULT_BUFLEN];
    if (!authenticated)
    {
        strcpy(response, "401 Unauthorized. Please authenticate first. \n");
        send(fd, response, strlen(response), 0);
        return;
    }

    char file_path[FILE_PATH_LENGTH];
    if (sscanf(initial_buffer, "PUT %s", file_path) != 1)
    {
        strcpy(response, "400 Bad Request. Invalid PUT request. \n");
        send(fd, response, strlen(response), 0);
        return;
    }

    FILE *file = fopen(file_path, "wb");
    if (file == NULL)
    {
        perror("Error opening file for writing. ");
        strcpy(response, "500 Server Error. Failed to open file");
        send(fd, response, strlen(response), 0);
        return;
    }

    char buffer[DEFAULT_BUFLEN];
    int bytes_received;
    size_t total_bytes = 0;

    while ((bytes_received = recv(fd, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytes_received] = '\0';

        if (bytes_received >= 2 && strcmp(buffer + bytes_received - 2, ".\n") == 0 || (bytes_received >= 3 && strcmp(buffer + bytes_received - 3, ".\r\n") == 0))
        {
            bytes_received -= (buffer[bytes_received - 2] == '\r') ? 3 : 2;
            fwrite(buffer, 1, bytes_received, file);
            total_bytes += bytes_received;
            pthread_exit(NULL);
        }

        fwrite(buffer, 1, bytes_received, file);
        total_bytes += bytes_received;
    }

    fclose(file);

    if (bytes_received < 0)
    {

        strcpy(response, "400 Bad Request. \n");
        send(fd, response, strlen(response), 0);
    }
    else
    {
        snprintf(response, sizeof(response), "200 OK. File transfer completed. %zu bytes transferred and saved on the server side.\n", total_bytes);
        if (send(fd, response, strlen(response), 0) < 0)
        {
            perror("Error sending response.");
        }
    }
}

void *handle_connection(void *arg)
{
    char buffer[DEFAULT_BUFLEN];
    int bytes_read;

    struct client_info *c_info = (struct client_info *)arg;
    int fd = c_info->fd;
    const char *passwordFile = c_info->setting.passwordFile;
    const char *directory = c_info->setting.directory;

    // sending the welcome message
    char welcome[] = "Welcome to Bob's file server\n";
    // testing
    if (send(fd, welcome, strlen(welcome), 0) < 0)
    {
        perror("Error sending the welcome message");
        close(fd);
        free(c_info->setting.passwordFile);
        free(c_info->setting.directory);
        free(c_info);

        pthread_exit(NULL);
    }

    int authenticated = 0; // Flag to track user authentication
    int failed_attempts = 0;

    while (1)
    {
        // Accept incoming connection

        // Clear Receive buffer

        memset(&buffer, '\0', sizeof(buffer));
        bytes_read = recv(fd, buffer, sizeof(buffer), 0);
        if (bytes_read > 0)
        {
            // Parse the received command
            char command[100];
            char username[100];
            char password[100];
            memset(command, 0, sizeof(command));
            memset(username, 0, sizeof(username));
            memset(password, 0, sizeof(password));
            parse_command(command, username, password, buffer);

            char response[DEFAULT_BUFLEN];
            memset(response, 0, sizeof(response));

            // Handle different commaands
            if (strcmp(command, "USER") == 0)
            {
                // Handle USER command
                user_command(passwordFile, username, password, response);
                if (strcmp(response, "200") == 0)
                {
                    authenticated = 1;
                    strcpy(response, "200 User test granted to access.\n");

                    send(fd, response, strlen(response), 0);
                }
                else
                {
                    failed_attempts++;
                    send(fd, response, strlen(response), 0);
                    if (failed_attempts >= 3)
                    {
                        strcpy(response, "403 Forbidden. Too many attempts. \n");
                        send(fd, response, strlen(response), 0);
                        close(fd);
                        free(c_info->setting.passwordFile);
                        free(c_info->setting.directory);
                        free(c_info);

                        pthread_exit(NULL);
                    }
                }
            }
            else if (strcmp(command, "LIST") == 0)
            {
                // Handle LIST command
                if (authenticated == 1)
                {
                    // Access the specified directory
                    DIR *dir = opendir(directory);
                    if (!dir)
                    {
                        sprintf(response, "Error opening a directory");
                        send(fd, response, strlen(response), 0);
                        continue;
                    }

                    // Read directory contents
                    struct dirent *entry;
                    while ((entry = readdir(dir)) != NULL)
                    {
                        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        {
                            continue;
                        }

                        // Append directory entry names to the response
                        strcat(response, entry->d_name);
                        strcat(response, "\n");
                    }

                    strcat(response, END_MARKER);

                    send(fd, response, strlen(response), 0);
                    closedir(dir);
                }
                else
                {
                    strcpy(response, "401 Unauthorized. Please authenticate first. \n");
                    send(fd, response, strlen(response), 0);
                }
            }
            else if (strcmp(command, "GET") == 0)
            {
                // handle GET command
                if (authenticated == 1)
                {

                    char file_path[250];
                    sscanf(buffer, "%*s %s", file_path);

                    // open the specified file for reading
                    FILE *file = fopen(file_path, "r");
                    if (file == NULL)
                    {
                        sprintf(response, "404 File %s not found\n", file_path);
                    }
                    else
                    {
                        // reading the file content
                        char file_content[DEFAULT_BUFLEN];
                        size_t bytes_read = fread(file_content, 1, sizeof(file_content), file);
                        fclose(file);

                        if (bytes_read == 0)
                        {
                            sprintf(response, "500 Internal server error. \n");
                        }
                        else
                        {

                            if (send(fd, file_content, bytes_read, 0) < 0)
                            {
                                perror("Error sending the file content. ");
                                close(fd);
                                pthread_exit(NULL);
                            }
                            sprintf(response, END_MARKER);

                            if (send(fd, response, strlen(response), 0) < 0)
                            {
                                perror("Error sending the response message. ");
                                close(fd);
                                free(c_info->setting.passwordFile);
                                free(c_info->setting.directory);
                                free(c_info);

                                pthread_exit(NULL);
                            }
                        }
                    }
                    send(fd, response, strlen(response), 0);
                }
                else
                {
                    strcpy(response, "401 Unauthorized. Please authenticate first. \n");
                    send(fd, response, strlen(response), 0);
                }
            }
            else if (strcmp(command, "PUT") == 0)
            {
                put_command(fd, buffer, authenticated);
            }
            else if (strcmp(command, "DEL") == 0)
            {
                if (authenticated)
                {
                    char file_path[250];
                    sscanf(buffer, "%*s %s", file_path);

                    if (access(file_path, F_OK) == 0)
                    {
                        if (remove(file_path) == 0)
                        {
                            sprintf(response, "200 File %s Deleted. \n", file_path);
                        }
                        else
                        {

                            sprintf(response, "500 Server error. \n");
                        }
                    }
                    else
                    {
                        sprintf(response, "404 File %s not found. \n", file_path);
                    }
                    send(fd, response, strlen(response), 0);
                }
                else
                {
                    strcpy(response, "401 Unauthorized. Please authenticate first. \n");
                    send(fd, response, strlen(response), 0);
                }
            }
            else if (strcmp(command, "QUIT") == 0)
            {
                strcpy(response, "GoodBye!\n");
                send(fd, response, strlen(response), 0);
                close(fd);
                free(c_info->setting.passwordFile);
                free(c_info->setting.directory);
                free(c_info);

                pthread_exit(NULL);
            }
            else
            {
                printf("An invalid FTP command \n");
            }
        }

        else if (bytes_read == 0)
        {
            printf("Connection closing...\n");
            close(fd);
            free(c_info->setting.passwordFile);
            free(c_info->setting.directory);
            free(c_info);

            pthread_exit(NULL);
        }

        else if (bytes_read < 0)
        {
            printf("Receive failed:\n");
            close(fd);
            free(c_info->setting.passwordFile);
            free(c_info->setting.directory);
            free(c_info);

            pthread_exit(NULL);
        }
    }

    close(fd);
    free(c_info->setting.passwordFile);
    free(c_info->setting.directory);
    free(c_info);

    pthread_exit(NULL);
}
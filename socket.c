/* C header files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <getopt.h>
#include <unistd.h>

/* Socket API headers */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// functions for commands
void parse_command(char *command, char *username, char *password, char *buffer);
void user_command(const char *passwordFile, char *username, char *password, char *response);
void list_users_command(const char *directory, char *response);

/* Definitions */
#define DEFAULT_BUFLEN 512
#define PORT 4529

int main(int argc, char *argv[])
{
    int server, client;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int length, fd, rcnt, optval;
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

    int authenticated = 0; // Flag to track user authentication

    while (1)
    {
        // Accept incoming connection
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
            memset(&buffer, '\0', sizeof(buffer));
            rcnt = recv(fd, buffer, bufferlen, 0);
            if (rcnt > 0)
            {
                // Parse the received command
                char command[100];
                char username[100];
                char password[100];
                memset(command, 0, sizeof(command));
                memset(username, 0, sizeof(username));
                memset(password, 0, sizeof(password));
                parse_command(command, username, password, buffer);

                char response[100];
                memset(response, 0, sizeof(response));

                // Handle different commaands
                if (strcmp(command, "USER") == 0)
                {
                    // Handle USER command
                    user_command(passwordFile, username, password, response);
                    if (strcmp(response, "200 User test granted to access.\n") == 0)
                    {
                        authenticated = 1;
                    }
                }
                else if (strcmp(command, "LIST") == 0)
                {
                    // Handle LIST command
                    if (authenticated == 1)
                    {
                        list_users_command(directory, response);
                    }
                    else
                    {
                        strcpy(response, "401 Unauthorized. Please authenticate first. \n");
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
                                // send the file to the client
                                if (send(fd, file_content, bytes_read, 0) < 0)
                                {
                                    perror("Error sending the file content. ");
                                    close(fd);
                                    break;
                                }
                                else
                                {
                                    sprintf(response, "\n 200 OK. \n");
                                }
                            }
                        }
                    }
                    else
                    {
                        strcpy(response, "401 Unauthorized. Please authenticate first. \n");
                    }
                }
                else if (strcmp(command, "PUT") == 0)
                {
                    if (authenticated == 1)
                    {
                        char file_path[250];
                        sscanf(buffer, "%*s %s", file_path);

                        // open the specified file for reading

                        if (access(file_path, F_OK) == 0)
                        {
                            sprintf(response, "404 File %s already exists\n", file_path);
                        }
                        else
                        {
                            FILE *file = fopen(file_path, "wb");
                            if (file == NULL)
                            {
                                perror("Error opening file for writing. ");
                                return 0;
                            }

                            while (1)
                            {
                                memset(buffer, 0, sizeof(buffer));
                                size_t bytes_received = recv(fd, buffer, sizeof(buffer), 0);
                                if (bytes_received < 0)
                                {
                                    perror("Error receving data");
                                    close(fd);
                                    fclose(file);
                                    return 0;
                                }

                                else if (bytes_received == 0 || strcmp(buffer, ".") == 0)
                                {
                                    break;
                                }
                                else
                                {
                                    size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
                                    if (bytes_received > bytes_written)
                                    {
                                        perror("Error writing a file. ");
                                        close(fd);
                                        return 0;
                                    }
                                }
                            }

                            fclose(file);

                            const char *response = "File transefer completed. \n";
                            if (send(fd, response, strlen(response), 0) < 0)
                            {
                                perror("Error sending data. ");
                                close(fd);
                                fclose(file);
                                return 0;
                            }
                        }
                    }
                    else
                    {
                        strcpy(response, "401 Unauthorized. Please authenticate firs. ");
                    }

                    if (send(fd, response, strlen(response), 0) < 0)
                    {
                        perror("Error sending data");
                        close(fd);
                        continue;
                    }
                }
                else if (rcnt == 0)
                    printf("Connection closing...\n");
                else
                {
                    printf("Receive failed:\n");
                    close(fd);
                    break;
                }
            }
        } while (rcnt > 0);
    }
    // Final Cleanup
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
        exit(0);
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
            sprintf(response, "200 User test granted to access.\n");
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

void list_users_command(const char *directory, char *response)
{
    // Access the specified directory
    DIR *dir = opendir(directory);
    if (!dir)
    {
        sprintf(response, "Error opening a directory");
        exit(EXIT_FAILURE);
    }

    // Read directory contents
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        // Append directory entry names to the response
        strcat(response, entry->d_name);
        strcat(response, "\n");
    }

    closedir(dir);
}

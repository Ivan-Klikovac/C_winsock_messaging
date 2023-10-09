#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <errno.h>

#define MAX_CLIENTS 64
#define BUFFER_SIZE 4096
#define PORT 1234

#define ERR 1
#define LOGIN 10
#define REGISTER 11
#define DM 12
#define SENDALL 13
#define SEPARATOR 127

// first byte of the messages sent between the client and server define what the message contains

typedef struct User
{
    SOCKET client_socket; // putting the socket here so that I can identify a user from a list of these in the list of connected clients
    char username[16];
    char is_online;
} User;

int login(SOCKET socket, char* user_data, User users[MAX_CLIENTS]);

int load_users(User users[MAX_CLIENTS])
{
    FILE* login_data = fopen("login_data.srv", "r");

    if(login_data == NULL)
    {
        printf("Failed to open login_data.srv\n");
        return 0;
    }

    char string[144];

    while(!feof(login_data))
    {
        fgets(string, 144, login_data);
        strtok(string, " ");
        
        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            if(users[i].username[0] == 0) // if the first character of the username is 0 then that position in the array is unused
            {
                strcpy(users[i].username, string);
                break; // so that it sets only one element in the array
            }
        }
    }

    return 1;
}

void start(User users[MAX_CLIENTS])
{
    int result = load_users(users);
    if(result == 0) printf("Failed to start server\n");
}

int main()
{
    WSADATA wsa;
    SOCKET master_socket, new_socket, client_socket[MAX_CLIENTS], max_sd;
    User users[MAX_CLIENTS];
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE];
    fd_set readfds;

    if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("Failed to initialize Winsock. Error code: %d\n", WSAGetLastError());
        return 1;
    }

    if((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        printf("Failed to create master socket\n");
        return 1;
    }

    int opt = 1; // set master socket to allow multiple connections
    if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0)
    {
        printf("setsockopt failed\n");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if(bind(master_socket, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        printf("Failed to bind master socket to port\n");
        return 1;
    }

    printf("Listening on port %d...\n", PORT);

    if(listen(master_socket, MAX_CLIENTS) < 0)
    {
        printf("Failed to listen for incoming connections on port %d\n", PORT);
        return 1;
    }

    int address_length = sizeof(address);
    printf("Waiting for connections...\n");

    memset(client_socket, 0, sizeof(client_socket)); // initialize arrays
    memset(users, 0, sizeof(users));

    start(users);

    while(1)
    {
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            int sd = client_socket[i];
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if(activity < 0 && errno != EINTR)
        {
            perror("Select error");
            continue;
        }
        else if(activity == 0) continue;

        if(FD_ISSET(master_socket, &readfds))
        {
            if((new_socket = accept(master_socket, (struct sockaddr*)&address, (int*)&address_length)) < 0)
            {
                printf("Failed to accept connection\n");
                return 1;
            }

            printf("New connection from %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            for(int i = 0; i < MAX_CLIENTS; i++)
                if(client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    break;
                }
        }

        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            int sd = client_socket[i];
            if(sd > 0 && FD_ISSET(sd, &readfds))
            {
                int bytes_read = recv(sd, buffer, BUFFER_SIZE, 0);
                if(bytes_read == 0)
                {
                    printf("User %d disconnected\n", sd);
                    closesocket(sd);
                    client_socket[i] = 0;
                }

                else if(bytes_read == SOCKET_ERROR)
                {
                    printf("Failed to receive data from %d. Disconnecting...\n", sd);
                    closesocket(sd);
                    client_socket[i] = 0;
                }

                else // handle incoming data
                {
                    buffer[bytes_read] = '\0';

                    if(buffer[0] == SENDALL)
                    {
                        int logged_in = 0;

                        for(int i = 0; i < MAX_CLIENTS; i++)
                        {
                            if(users[i].client_socket == sd) // user is logged in
                            {
                                logged_in = 1;
                                printf("%s: %s\n", users[i].username, buffer + 1); // + 1 to move the pointer to the second character
                                send(sd, buffer, strlen(buffer), 0); // respond with the initial message
                                break;
                            }
                        }

                        if(!logged_in)
                        {
                            printf("User %d <not logged in>: %s\n", sd, buffer + 1);
                            send(sd, buffer, strlen(buffer), 0);
                        }
                        
                    }

                    if(buffer[0] == LOGIN)
                    {
                        int result = login(sd, buffer + 1, users); // login function will return 1 if the credentials are correct and add the user to the list

                        if(result == 1)
                        {
                            buffer[1] = 1;
                            strcpy(&buffer[2], "Login successful\n");
                        }

                        else
                        {
                            buffer[1] = 0;
                            strcpy(&buffer[2], "Login unsuccessful\n");
                        }

                        send(sd, buffer, strlen(buffer), 0); // respond with the result
                    }



                    
                }
            }
        }
    }
}

int login(SOCKET socket, char* user_data, User users[MAX_CLIENTS])
{
    char* username = user_data;
    char* password = strchr(user_data, SEPARATOR); // more fitting name would be separator but this works ig

    *password = '\0'; // replace the separator with null term so that the username is a proper string

    password++; // now it does point to the start of the password

    FILE* login_data = fopen("login_data.srv", "r");

    if(login_data == NULL) 
    {
        printf("Failed to open login_data.srv\n");
        return 0;
    }

    while(!feof(login_data))
    {
        char string[144] = {0};
        fgets(string, 144, login_data);
        char* token = strtok(string, " ");
        if(strcmp(username, token) == 0) // found the username in the file
        {
            printf("Found the username in the file\n");
            token = strtok(NULL, " "); // get the password

            if(strcmp(password, token) == 0)
            {
                printf("Found the password\n");
                for(int i = 0; i < MAX_CLIENTS; i++)
                {
                    if(strcmp(username, users[i].username) == 0)
                    {
                        printf("Found the client\n");
                        users[i].client_socket = socket;
                        users[i].is_online = 1;
                        return 1;
                    }
                }

                printf("Couldn't find the client\n");
            }

            printf("Couldn't find the password\n");
        }

        printf("Couldn't find the username\n");
    }

    printf("Closing file\n");

    fclose(login_data);
    return 0;
}

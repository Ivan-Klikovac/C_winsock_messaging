#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 1234
#define BUFFER_SIZE 4096

#define ERR 1
#define LOGIN 10
#define REGISTER 11
#define DM 12
#define SENDALL 13
#define SEPARATOR 127

// first byte of the messages sent between the client and server defines what the messages contain

void send_data(SOCKET s, char* data)
{
    if (send(s, data, strlen(data), 0) < 0)
        printf("Failed to send message to server\n");
}

void receive_data(SOCKET s, char* buffer)
{
    int bytes_received = recv(s, buffer, BUFFER_SIZE, 0);

    if(bytes_received < 0)
        printf("Failed to receive data from server\n");

    else if(bytes_received == 0)
        printf("Server disconnected\n");

    else
    {
        buffer[bytes_received] = '\0';

        if(buffer[0] == SENDALL)
            printf("<You> %s\n", buffer);
        
        if(buffer[0] == ERR)
            printf("<Server> Error: %s\n", buffer);
    }
}

int login(SOCKET s, char* username, char* password);
void help();
void sendall(SOCKET s, char* message, char* buffer);

void shift(char* data)
{
    // shift everything by 1 byte to the right to make room for the info byte

    int length = strlen(data);
    
    for(int i = length; i > 0; i--)
        data[i] = data[i - 1];
    
    data[length] = 0; // null term the string after shifting because the previous null term got overwritten
}

char* on_connect(SOCKET s, char* username)
{
    char password[128];
    printf("Username: "); fgets(username, 16, stdin);
    printf("Password: "); fgets(password, 128, stdin);
    username[strlen(username) - 1] = 0;
    password[strlen(password) - 1] = 0;
    login(s, username, password);
    
    return username;
}

int main()
{
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in server;
    char data[BUFFER_SIZE], buffer[BUFFER_SIZE];

    // initialize winsock
    if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("Failed to initialize Winsock. Error code: %d\n", WSAGetLastError());
        return 1;
    }

    // create socket
    if((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        printf("Failed to create socket\n");
        return 1;
    }

    // initialize server structure
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server.sin_port = htons(SERVER_PORT);

    if(connect(s, (struct sockaddr*)&server, sizeof(server)) < 0)
    {
        printf("Failed to connect to server\n");
        return 1;
    }

    char username[16]; // place the "result" of on_connect here
    on_connect(s, username);

    while(1)
    {
        printf(">");
        fgets(data, BUFFER_SIZE, stdin);
        data[strlen(data) - 1] = ' ';
        char* token = strtok(data, " ");


        while(token - data < strlen(data))
        {
            if(strcmp(token, "help") == 0)
            {
                help();
                break;
            }

            if(strcmp(token, "sendall") == 0)
            {
                token = strtok(NULL, " ");
                sendall(s, token, buffer);
                break;
            }

            if(strcmp(token, "exit") == 0) return 0;

            else break;
        }
    }

    getchar();

    // clean up
    closesocket(s);
    WSACleanup();

    return 0;
}

void help()
{
    printf("list\nsendall <message>\ndm <username>\nsend <message>\nleavedm\n");
}

void sendall(SOCKET s, char* message, char* buffer)
{
    shift(message);
    message[0] = SENDALL;
    send_data(s, message);
    receive_data(s, buffer);
}

int login(SOCKET s, char* username, char* password)
{
    char buffer[144];
    char recv_buffer[4096];

    buffer[0] = LOGIN;

    strcpy(buffer + 1, username);
    char* ptr = strchr(buffer + 1, 0);
    *ptr = SEPARATOR;
    strcpy(ptr + 1, password);

    printf("buffer: %s\n", buffer);

    // login packet goes like this: username separator password \0

    send_data(s, buffer);
    receive_data(s, recv_buffer);

    if(recv_buffer[1] == 1)
    {
        printf("<Server> \n", &recv_buffer[2]);
        return 1;
    }

    else
    {
        printf("<Server> %s\n", &recv_buffer[2]);
        return 0;
    }
}

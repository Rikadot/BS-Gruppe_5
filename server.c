//
// Created by lukew on 03.04.2022.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include "server.h"
#include "functions.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

KeyValueData keyValueDataArray[ARRAY_SIZE];
KeyValueData *keyValueData;


int initializeArray() {
    printf("Initialize array.\n");
    fflush(0);
    for (int i = 0; i < ARRAY_SIZE; i++) {
        KeyValueData temp;
        strcpy(temp.key, "\0");
        strcpy(temp.value, "\0");
        keyValueDataArray[i] = temp;
    }
}




#define MAX_PARAMS 3

char splitChar[] = " ";
char *param[MAX_PARAMS];

char res[ARRAY_SIZE];
char *temp = res;

int commandParam(char *input, char *splitchar, char **param, int size) {
    int i = 0;
    param[0] = strtok(input, splitchar);
    while (param[i++] && i < size) {
        param[i] = strtok(NULL, splitchar);
    }
    return (i);
}

int startServer() {

    initializeArray();

    int port = 5678;

    int counter = 0;
    char command[BUFFER_SIZE];

    int server_fd;
    int client_fd;
    int error;
    int command_error;

    struct sockaddr_in server_address, client;
    char buf[BUFFER_SIZE];

    //Messages
    char *welcome_message = "Hello from the KeyValue-Store Server.\r\n"
                            "Type HELP for more information.\r\n";
    char *end_message = "Bye from the KeyValue-Store Server.\r\n";
    char *double_space_message = "\rYou cannot put more then one space, behind each other.\r\n";

    //Shared Memory
    int id;
    id = shmget(IPC_PRIVATE, sizeof(keyValueDataArray), IPC_CREAT | 0777);
    if (id < 0){
        printf ("Error with key %d\n", IPC_PRIVATE);
        fflush(0);
    }
    keyValueData = shmat(id,0,0);

    //For fork (Multiple clients)
    int pid;




    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        printf("Could not create a socket!\n");
        fflush(0);
        exit(-1);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

    error = bind(server_fd, (struct sockaddr *) &server_address, sizeof(server_address));
    if (error < 0) {
        printf("Could not bind a socket!\n");
        fflush(0);
        exit(-1);
    }

    error = listen(server_fd, 128);
    if (error < 0) {
        printf("Could not listen on socket!\n");
        fflush(0);
        exit(-1);
    }

    printf("Server started.\n");
    fflush(0);

    while (1) {
        if(pid == 0){
            printf("Child ended connection.\n");
            fflush(0);
            close(server_fd);
            break;
        }
        socklen_t client_len = sizeof(client);
        //Server accepts new incoming connection
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);
        pid = fork();
        printf("Testing ----------------%i\n",pid);
        if(pid < 0){
            printf("Could not create a new thread!\n");
            fflush(0);
            exit(-3);
        }

        //Child Process
        if(pid == 0) {

            if (client_fd < 0) {
                printf("Could not establish new connection!\n");
                fflush(0);
                exit(-1);
            }

            //Send welcome_message to client
            send(client_fd, welcome_message, strlen(welcome_message), 0);
            fflush(0);

            //Clear
            counter = 0;
            memset(command, 0, sizeof(command));
            memset(buf, 0, sizeof(buf));

            //Client Input / Reading
            while (1) {
                int read = recv(client_fd, buf, BUFFER_SIZE, 0);

                //Check if input comes in single chars threw telnet
                command[counter] = buf[0];
                counter++;

                //Check for more than one space
                if (counter > 1 && command[counter - 1] == 32 && command[counter - 2] == 32) {
                    send(client_fd, double_space_message, strlen(double_space_message), 0);
                    counter = 0;
                    memset(command, 0, sizeof(command));
                    memset(buf, 0, sizeof(buf));
                }

                //Delete input from server when client presses backspace
                if (buf[0] == 8) {
                    fflush(0);
                    counter -= 2;
                    if (counter < 0) {
                        counter = 0;
                    }
                }

                //Check ASCII Code for enter
                if (command[counter - 1] == 13) {
                    //Check if there is a command or only enter
                    if (strlen(command) > 1) {
                        //Add \0 to string command for comparison
                        command[counter - 1] = '\0';
                        //__________________________________________________

                        char message[ARRAY_SIZE];

                        commandParam(command, splitChar, param, 3);

                        //PUT Command
                        if (strcmp(param[0], "PUT") == 0) {
                            command_error = put(param[1], param[2], keyValueData);
                            if (command_error == -2) {
                                sprintf(message, "Key is not allowed to contain a special character.\n\r");
                            } else if (command_error == -1) {
                                sprintf(message, "An error occurred.\n\r");
                            } else {
                                sprintf(message, "PUT:Key %s:Value %s\n\r", param[1], param[2]);
                            }
                            send(client_fd, message, strlen(message), 0);
                            fflush(0);
                        } else

                            //GET Command
                        if (strcmp(param[0], "GET") == 0) {
                            command_error = get(param[1], temp, keyValueData);
                            if (command_error == -1) {
                                sprintf(message, "GET:Key %s:key_nonexistent\n\r"
                                                 "This key does not exist.\n\r", param[1]);
                            } else {
                                sprintf(message, "GET:Key %s:Value %s\n\r", param[1], temp);
                            }
                            send(client_fd, message, strlen(message), 0);
                            fflush(0);
                        } else

                            //DEL Command
                        if (strcmp(param[0], "DEL") == 0) {
                            command_error = del(param[1], keyValueData);
                            if (command_error == -1) {
                                sprintf(message, "DEL:Key %s:key_nonexistent\n\r"
                                                 "This key does not exist.\n\r", param[1]);
                            } else {
                                sprintf(message, "DEL:Key %s:key_deleted\n\r"
                                                 "Key successfully deleted.\n\r", param[1]);
                            }
                            send(client_fd, message, strlen(message), 0);
                            fflush(0);
                        } else

                            //QUIT Command
                        if (strcmp(param[0], "QUIT") == 0) {
                            send(client_fd, end_message, strlen(end_message), 0);
                            fflush(0);
                            close(client_fd);
                            break;
                        } else

                            //Help Command
                        if (strcmp(param[0], "HELP") == 0) {
                            sprintf(message,
                                    "PUT Command: Usage \"PUT <Key> <Value>\" --> saves a key and its value.\n\r"
                                    "GET Command: Usage \"GET <Key>\" --> returns the value of the given key.\n\r"
                                    "DEL Command: Usage \"DEL <Key>\" --> removes the key and the belonging value.\n\r"
                                    "QUIT Command: Usage \"QUIT\" --> let the client disconnect from the server.\n\r");
                            send(client_fd, message, strlen(message), 0);
                            fflush(0);
                        } else {
                            //Unknown command message
                            sprintf(message, "Unknown command, please type HELP for more information.\n\r");
                            send(client_fd, message, strlen(message), 0);
                        }
                    }
                    //Reset input
                    counter = 0;
                    memset(command, 0, sizeof(command));
                    memset(buf, 0, sizeof(buf));
                    printf("\n");
                    fflush(0);
                }

                if (read < 0) {
                    printf("Read %i", read);
                    printf("Error while reading input from client!\n");
                    fflush(0);
                    break;
                }
            }
        }
    }
    return 0;
}


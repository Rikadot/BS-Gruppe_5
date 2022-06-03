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
#include <sys/sem.h>
#include <sys/msg.h>

KeyValueData *keyValueData;

#define MAX_PARAMS 3

//Command
char splitChar[] = " ";
char *param[MAX_PARAMS];
char res[ARRAY_SIZE];
char *temp = res;

//Input
int counter = 0;
char command[BUFFER_SIZE];
char buf[BUFFER_SIZE];

//Server and Error
int port = 5678;
int server_fd;
int client_fd;
int error;
int command_error;

//Semaphore and shared memory
int semaphoreId;
struct sembuf up, down;

int sharedMemoryId;

int sharedMemoryId2;

//Functions
void exitWhenError(int read, const char *string);

void clearInput();

int commandParam(char *input, char *splitchar, char **param, int size);

int beg(){
    return semop(semaphoreId,&down,1);
}

int end(){
    return semop(semaphoreId,&up,1);
}

//TODO REMOVE
//variables for pub/sub
int messageQueueId;
typedef struct{
    long mtype;                     // message type
    char mtext[100];
}Message;



_Noreturn int startServer() {


    //Message queue
    messageQueueId = msgget(IPC_PRIVATE, IPC_CREAT | 0777);

    struct sockaddr_in server_address, client;


    //Messages
    char *welcome_message = "Hello from the KeyValue-Store Server.\r\n"
                            "Type HELP for more information.\r\n";
    char *end_message = "Bye from the KeyValue-Store Server.\r\n";
    char *double_space_message = "\rYou cannot put more then one space, behind each other.\r\n";

    //Shared Memory Sub
    exitWhenError(sharedMemoryId2,"Could not create shared memory\n");

    //Shared Memory
    sharedMemoryId = shmget(IPC_PRIVATE, sizeof(KeyValueData) * ARRAY_SIZE, IPC_CREAT | 0777);
    exitWhenError(sharedMemoryId, "Could not create shared memory\n");
    keyValueData = shmat(sharedMemoryId, 0, 0);

    //Semaphore
    semaphoreId = semget(IPC_PRIVATE,1,IPC_CREAT|0600);
    exitWhenError(semaphoreId,"Could not initialize semaphore.\n");

    unsigned short state[1];
    //Set all semaphores to 1
    state[0] = 1;
    semctl(semaphoreId,0,SETALL,state);

    down.sem_op = -1;
    down.sem_num = 0;
    down.sem_flg = SEM_UNDO;

    up.sem_op = 1;
    up.sem_num = 0;
    up.sem_flg = SEM_UNDO;

    //For fork (Multiple clients)
    int pid = 1;

    //Create server file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    exitWhenError(server_fd,"Could not create a socket!\n");

    //Server adress config
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    //Instant reuse
    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

    //Binding socket
    error = bind(server_fd, (struct sockaddr *) &server_address, sizeof(server_address));
    exitWhenError(error,"Could not bind a socket!\n");

    //Listen on serverfile descriptor (max 128 clients)
    error = listen(server_fd, 128);
    exitWhenError(error,"Could not listen on socket!\n");

    printf("Server started.\n");
    fflush(0);

    while (1) {
        socklen_t client_len = sizeof(client);
        //Server accepts new incoming connection
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

        //Create a fork / child
        pid = fork();
        exitWhenError(pid,"Could not create a new thread!\n");

        //Child Process
        if(pid == 0) {

            //New fork for message queue reading
            int pid2 = fork();
            if(pid2 == 0){
                while (1) {
                    Message message2;
                    if (msgrcv(messageQueueId, &message2, 100, getppid(), IPC_NOWAIT) < 0) {
                    } else {
                        printf("received message %s \n", message2.mtext);
                        send(client_fd,message2.mtext,strlen(message2.mtext),0);
                    }
                }
            }

            //Client file descriptor
            exitWhenError(client_fd,"Could not establish new connection!\n");

            //Send welcome_message to client
            send(client_fd, welcome_message, strlen(welcome_message), 0);
            fflush(0);

            //Clear
            clearInput();


            while (1) {

                //Client Input / Reading
                int read = recv(client_fd, buf, BUFFER_SIZE,0);

                //Check if input comes in single chars threw telnet
                command[counter] = buf[0];
                counter++;

                //Check for more than one space
                if (counter > 1 && command[counter - 1] == 32 && command[counter - 2] == 32) {
                    send(client_fd, double_space_message, strlen(double_space_message), 0);
                    clearInput();
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


                        char message[ARRAY_SIZE];
                        commandParam(command, splitChar, param, 3);

                        int semaphoreValue;
                        semaphoreValue = semctl(semaphoreId, 0, GETVAL, 0);
                        printf("semaphoreValue %i\n", semaphoreValue);
                        int processWhoChanged = semctl(semaphoreId, 0, GETPID, 0);
                        printf("WhoChangedSemaphore %i\n", processWhoChanged);
                        int currentProcess = getpid();
                        printf("CurrentProcess %i\n", currentProcess);

                        if (semaphoreValue == 1 || (semaphoreValue == 0 && processWhoChanged == currentProcess)) {

                            if (strcmp(param[0], "BEG") == 0) {
                                sprintf(message, "Started a transaction.\n\r");
                                send(client_fd, message, strlen(message), 0);
                                fflush(0);
                                beg();
                            } else if (strcmp(param[0], "END") == 0) {
                                sprintf(message, "Ended a transaction.\n\r");
                                send(client_fd, message, strlen(message), 0);
                                fflush(0);
                                end();
                            } else

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

                                //_______________________________________________________________
                                Message publishMessage;
                                publishMessage.mtype = -1;

                                char str[STRING_LENGTH];
                                strcpy(str,keyValueData[command_error].subscriberList);
                                char *ptr = strtok(str, ",");
                                while(ptr != NULL) {
                                    publishMessage.mtype = strtol(ptr, (char**)NULL, 10);
                                    strcpy(publishMessage.mtext, message);
                                    if (msgsnd(messageQueueId, &publishMessage, 100, 0) < 0) {
                                        perror("Failed to send message in queue.");
                                        exit(-1);
                                    }
                                    ptr = strtok(NULL, ",");
                                }
                                //_____________________________________________________________
                            } else

                                //GET Command
                            if (strcmp(param[0], "GET") == 0) {
                                command_error = get(param[1], temp, keyValueData);
                                if (command_error == -1 || strcmp(temp, "\0")) {
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
                                //_______________________________________________________________
                                Message publishMessage;
                                publishMessage.mtype = -1;

                                char str[STRING_LENGTH];
                                strcpy(str,keyValueData[command_error].subscriberList);
                                char *ptr = strtok(str, ",");
                                while(ptr != NULL) {
                                    publishMessage.mtype = strtol(ptr, (char**)NULL, 10);
                                    strcpy(publishMessage.mtext, message);
                                    if (msgsnd(messageQueueId, &publishMessage, 100, 0) < 0) {
                                        perror("Failed to send message in queue.");
                                        exit(-1);
                                    }
                                    ptr = strtok(NULL, ",");
                                }
                                //_____________________________________________________________
                            } else

                                //QUIT Command
                            if (strcmp(param[0], "QUIT") == 0) {
                                send(client_fd, end_message, strlen(end_message), 0);
                                fflush(0);

                                //Quit without end
                                if(semaphoreValue == 0 || processWhoChanged == currentProcess){
                                    end();
                                }

                                shutdown(client_fd, SHUT_RDWR);
                                break;
                            } else

                                //SUB Command
                            if(strcmp(param[0], "SUB") == 0){
                                int feedback = addToList(getpid(),param[1], keyValueData);
                                for(int i = 0; i < ARRAY_SIZE; i++) {
                                    if(strcmp(keyValueData[i].key, param[1])){
                                        char str[STRING_LENGTH];
                                        strcpy(str,keyValueData[i].subscriberList);
                                        char * ptr = strtok(str, ",");
                                        printf("%s", ptr);
                                        while (ptr != NULL) {
                                            ptr = strtok(NULL, ",");
                                            printf("%s", ptr);
                                        }
                                    }
                                }
                                switch (feedback) {
                                    case 0: sprintf(message, "Subscribed to %s.\n\r",param[1]); break;
                                    case -2: sprintf(message, "User already subscribed to %s.\n\r",param[1]); break;
                                    default: sprintf(message, "An Error has occurred.\n\r",param[1]);
                                }
                                send(client_fd, message, strlen(message), 0);
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
                        } else {
                            if (strcmp(param[0], "QUIT") == 0) {
                                send(client_fd, end_message, strlen(end_message), 0);
                                fflush(0);
                                shutdown(client_fd, SHUT_RDWR);
                                clearInput();
                                break;
                            } else {
                                sprintf(message, "You cannot interact with this server, because there is a current interaction.\n\r");
                                send(client_fd, message, strlen(message), 0);
                                clearInput();
                            }
                        }
                    }
                    //Reset input
                    clearInput();
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
    shmdt(keyValueData);
    shmctl(sharedMemoryId, IPC_RMID, 0);
    shmctl(sharedMemoryId2,IPC_RMID,0);
    semctl(semaphoreId, 0, IPC_RMID);
    return 0;
}

void clearInput() {
    counter = 0;
    memset(command, 0, sizeof(command));
    memset(buf, 0, sizeof(buf));
}

void exitWhenError(int input, const char *message) {
    if(input < 0){
        perror(message);
        exit(-1);
    }
}

int commandParam(char *input, char *splitchar, char **param, int size) {
    int i = 0;
    param[0] = strtok(input, splitchar);
    while (param[i++] && i < size) {
        param[i] = strtok(NULL, splitchar);
    }
    return (i);
}



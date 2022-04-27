//
// Created by lukew on 03.04.2022.
//
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "functions.h"

//Insert key and theirs values in the array
int put(char * key, char * value, KeyValueData * keyValueDataArray){

    //Check if key contains special chars or empty space
    for(int i = 0; i < strlen(key);i++) {
        char temp = key[i];
        if (isspace(temp) != 0) {
            printf("An error occurred string contains SPACE!\n");
            fflush(0);
            return -2;
        }
        if(!((temp >= 'a' && temp <= 'z') || (temp >= 'A' && temp <= 'Z') || (temp >= '0' && temp <= '9'))){
            printf("An error occurred key contains special character!\n");
            fflush(0);
            return -2;
        }
    }

    //Search for existing key
    for(int i = 0; i < ARRAY_SIZE;i++){
        if(strcmp(keyValueDataArray[i].key,key) == 0){
            printf("There is an entry with this key.\n");
            fflush(0);
            strcpy(keyValueDataArray[i].value,value);
            return 0;
        }
    }

    //Search next free entry in array
    KeyValueData temp;
    strcpy(temp.key,key);
    strcpy(temp.value, value);
    for(int i = 0; i < ARRAY_SIZE;i++){
        if(strcmp(keyValueDataArray[i].key,"\0") == 0){
            printf("New entry created.\n");
            fflush(0);
            keyValueDataArray[i] = temp;
            return 1;
        }
    }

    printf("An error occurred!\n");
    fflush(0);
    return -1;
}

int get(char* key, char* res,KeyValueData * keyValueDataArray){
    for(int i = 0; i < ARRAY_SIZE;i++) {
        if (strcmp(keyValueDataArray[i].key, key) == 0) {
            printf("Key found.\n");
            fflush(0);
            strcpy(res,keyValueDataArray[i].value);
            return 0;
        }
    }
    printf("An error occurred no entry for this key\n");
    fflush(0);
    return -1;
}

int del(char* key,KeyValueData * keyValueDataArray){
    for(int i = 0; i < ARRAY_SIZE;i++) {
        if (strcmp(keyValueDataArray[i].key, key) == 0) {
            printf("Key found and deleted.\n");
            fflush(0);
            strcpy(keyValueDataArray[i].key,"\0");
            strcpy(keyValueDataArray[i].value,"\0");
            return 0;
        }
    }
    printf("An error occurred no entry for this key\n");
    fflush(0);
    return -1;
}


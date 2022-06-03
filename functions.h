//
// Created by lukew on 03.04.2022.
//

#ifndef TEST_FUNCTIONS_H
#define TEST_FUNCTIONS_H

#define STRING_LENGTH 128
#define ARRAY_SIZE 128

typedef struct KeyValueData {
    char key[STRING_LENGTH];
    char value[STRING_LENGTH];
    char subscriberList[STRING_LENGTH]
} KeyValueData;

int put(char * key, char * value,KeyValueData * keyValueDataArray);
int get(char* key, char* res,KeyValueData * keyValueDataArray);
int del(char* key,KeyValueData * keyValueDataArray);
int addToList(int pid, char * key, KeyValueData *keyValueDataArray);

#endif //TEST_FUNCTIONS_H

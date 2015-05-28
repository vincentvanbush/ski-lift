#ifndef SRC_LOGGER_H
#define SRC_LOGGER_H
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

int first = 1;
void logEvent(int mytid, char *str)
{
    char fileName[255];
    FILE *file;
    sprintf(fileName,"/tmp/logs/%d.log",mytid);

    if(access(fileName,F_OK) != -1 && first != 1) //file.exists() == true
    {
        file = fopen(fileName,"a");
    }
    else
    {
        file = fopen(fileName,"w+");
        first = 0;
    }

    fprintf(file,"%s\n",str);
    fclose(file);


}


#endif //SRC_LOGGER_H

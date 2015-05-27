#ifndef SRC_LOGGER_H
#define SRC_LOGGER_H
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

int first = 1;
void logEvent(char *message, char *phase, int id)
{
    char fileName[255];
    FILE *file;
    sprintf(fileName,"/tmp/logs/%d.log",id);

    if(access(fileName,F_OK) != -1 && first != 1) //file.exists() == true
    {
        file = fopen(fileName,"a");
    }
    else
    {
        file = fopen(fileName,"w+");
        first = 0;
    }

    
    struct timeval current_time;
    time_t logTime = time(NULL);
    char *timeStr =  asctime(localtime(&logTime));
    gettimeofday(&current_time, NULL);
    timeStr[strlen(timeStr)-6] = '\0';
    timeStr = timeStr + 11;
    fprintf(file,"%s %06ld: %s\n",timeStr, current_time.tv_usec ,phase);
    fprintf(file,"%s\n",message);
    fprintf(file,"\n");
    fclose(file);
    

}


#endif //SRC_LOGGER_H
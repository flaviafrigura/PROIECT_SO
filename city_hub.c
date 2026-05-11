#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CMD 256
#define MAX_BUF 512

void start_monitor()
{
    int pipefiledescriptors[2];
    if(pipe(pipefiledescriptors)<0)
    {
        perror("Problema creare pipe");
        return;
    }
    pid_t hub_mon=fork();
    if(hub_mon<0)
    {
        perror("Problema creare proces copil hub_mon");
        return;
    }
    if(hub_mon==0)
    {
        pid_t monitor=fork();
        if(monitor<0)
        {
            perror("Problema creare proces copil monitor");
            exit(1);
        }
        if(monitor==0)
        {
            close(pipefiledescriptors[0]);
            dup2(pipefiledescriptors[1],STDOUT_FILENO);
            close(pipefiledescriptors[1]);
            execl("./monitor_reports","monitor_reports",NULL);
            perror("A esuat inlocuirea procesului cu monitor_reports");
            exit(1);
        }
        close(pipefiledescriptors[1]);
        char buffer[512];
        int n;
        while((n=read(pipefiledescriptors[0],buffer,sizeof(buffer)-1))>0)
        {
            buffer[n]='\0';
            printf("MONITOR: %s",buffer);
            if(strstr(buffer,"terminated")!=NULL)
            {
                printf("HUB: Monitor ended\n");
            }
        }
        close(pipefiledescriptors[0]);
        wait(NULL);
        exit(0);
    }
    printf("Monitor now active\n");
}

void calculate_scores(char districts[][],int n)
{
    for (int i=0;i<n;i++)
    {
        int pipefiledescriptors[2];
        if (pipe(pipefiledescriptors)<0)
        {
            perror("Problema creare pipe");
            continue;
        }
        pid_t scorer=fork();
        if(scorer<0)
        {
            perror("Problema creare proces");
            continue;
        }
        if(scorer==0)
        {
            close(pipefiledescriptors[0]);
            dup2(pipefiledescriptors[1],STDOUT_FILENO);
            close(pipefiledescriptors[1]);
            execl("./scorer", "scorer", districts[i], NULL);
            perror("Problema la execul scorer ului");
            exit(1);
        }
        close(pipefiledescriptors[1]);
        char buffer[512];
        int n;
        printf("%s\n",districts[i]);
        while((n=read(pipefiledescriptors[0],buffer,sizeof(buffer)-1))>0)
        {
            buffer[n]='\0';
            printf("%s",buffer);
        }
        close(pipefiledescriptors[0]);
        wait(NULL);
    }
}

int main()
{
    char command[512];
    while (1)
    {
        printf("city_hub> ");
        if(fgets(command,sizeof(command),stdin)==NULL)
            break;
        command[strcspn(command,"\n")]='\0';
        if(strcmp(command,"exit")==0)
            break;
        else if(strcmp(command,"start_monitor")==0)
            start_monitor();
        else if(strncmp(command,"calculate_scores",16)==0)
        {
            char *districts[20];
            int count=0;
            char *token=strtok(command," ");
            token=strtok(NULL," ");
            while(token!=NULL)
            {
                districts[count++]=token;
                token=strtok(NULL," ");
            }
            calculate_scores(districts,count);
        }
        else
            printf("Unknown command\n");
    }
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

void handler(int sig)
{
    if(sig==SIGINT)
    {
        printf("TERMINATED: SIGINT recieved. Terminating.\n");
        unlink(".monitor_pid");
        exit(0);
    }
    else if(sig==SIGUSR1)
    {
        printf("MSG: SIGUSR1 recieved. Adding new report.\n");
    }
}

int main()
{
    int file=open(".monitor_pid",O_RDONLY);
    if(file>=0)
    {
        char buffer[32]={0};
        read(file,buffer,sizeof(buffer)-1);
        close(file);
        pid_t existing=(pid_t)atoi(buffer);
        if((existing>0)&&(kill(existing,0)==0))
        {
            printf("MSG:Another monitor is already running (PID %d). Exiting.\n",existing);
            fflush(stdout);
            return 1;
        }
        unlink(".monitor_pid");
    }

    file=open(".monitor_pid",O_CREAT|O_WRONLY|O_TRUNC,0644);
    if(file<0)
    {
        perror("Problema la crearea/override PID file");
        return 1;
    }
    char buffer[32];
    sprintf(buffer,"%d\n",getpid());
    write(file,buffer,strlen(buffer));
    close(file);
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler=handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGUSR1,&sa,NULL);
    printf("MSG:Monitor is active now. PID is %d\n",getpid());
    fflush(stdout);
    while(1)
    {
        pause();
    }
    return 0;
}

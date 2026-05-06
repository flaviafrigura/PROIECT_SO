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
        printf("SIGINT recieved. Terminating.\n");
        unlink(".monitor_pid");
        exit(0);
    }
    else if(sig==SIGUSR1)
    {
        printf("SIGUSR1 recieved. Adding new report.\n");
    }
}

int main()
{
    int file=open(".monitor_pid",O_CREAT|O_WRONLY|O_TRUNC,0644);
    if(file<0)
    {
        perror("Error with the PID file");
        return 1;
    }
    char buffer[32];
    sprintf(buffer,"%d\n",getpid());
    write(file,buffer,strlen(buffer));
    close(file);
    struct sigaction signal;
    memset(&signal,0,sizeof(signal));
    signal.sa_handler=handler;
    sigaction(SIGINT,&signal,NULL);
    sigaction(SIGUSR1,&signal,NULL);
    printf("Monitor is active now. PID is %d\n",getpid());
    while(1)
    {
        pause();
    }
    return 0;
}

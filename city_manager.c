#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>

#define NAME_LEN 32
#define CATEGORY_LEN 32
#define DESC_LEN 128

typedef struct
{
    int id;
    char inspector[NAME_LEN];
    float latitude;
    float longitude;
    char category[CATEGORY_LEN];
    int severity;
    time_t timestamp;
    char description[DESC_LEN];
}report;

void mode_to_string(mode_t mode, char *str)
{
    str[0]=(mode&S_IRUSR)?'r':'-';
    str[1]=(mode&S_IWUSR)?'w':'-';
    str[2]=(mode&S_IXUSR)?'x':'-';
    str[3]=(mode&S_IRGRP)?'r':'-';
    str[4]=(mode&S_IWGRP)?'w':'-';
    str[5]=(mode&S_IXGRP)?'x':'-';
    str[6]=(mode&S_IROTH)?'r':'-';
    str[7]=(mode&S_IWOTH)?'w':'-';
    str[8]=(mode&S_IXOTH)?'x':'-';
    str[9]='\0';
}

void create_district(const char *district)
{
    struct stat st;
    if(stat(district,&st)==0&&S_ISDIR(st.st_mode))
        return;

    mkdir(district,0750);
    char path[256];
    int file;

    sprintf(path,"%s/reports.dat",district);
    file=open(path,O_CREAT|O_EXCL|O_RDWR,0664);
    if(file>=0)close(file);
    chmod(path,0664);

    sprintf(path,"%s/district.cfg",district);
    file=open(path,O_CREAT|O_EXCL|O_RDWR,0640);
    if(file>=0)
    {
        write(file,"2\n",2);
        close(file);
    }
    chmod(path,0640);

    sprintf(path,"%s/logged_district",district);
    file=open(path,O_CREAT|O_EXCL|O_RDWR,0644);
    if(file>=0)close(file);
    chmod(path,0644);

    char linkname[256];
    sprintf(linkname,"active_reports-%s",district);
    if(lstat(linkname,&st)<0)
    {
        sprintf(path,"%s/reports.dat",district);
        symlink(path,linkname);
    }
}

int parse_condition(const char *input,char *field,char *op,char *value)
{
    if(sscanf(input,"%[^:]:%[^:]:%s",field,op,value)!=3)
        return 0;

    if(strcmp(field,"severity")!=0 &&
       strcmp(field,"category")!=0 &&
       strcmp(field,"inspector")!=0 &&
       strcmp(field,"timestamp")!=0)
        return 0;

    if(strcmp(op,"==")!=0 &&
       strcmp(op,"!=")!=0 &&
       strcmp(op,">")!=0 &&
       strcmp(op,"<")!=0 &&
       strcmp(op,">=")!=0 &&
       strcmp(op,"<=")!=0)
        return 0;

    return 1;
}

int cmp_int(int a,int b,const char *op)
{
    if(!strcmp(op,"=="))return a==b;
    if(!strcmp(op,"!="))return a!=b;
    if(!strcmp(op,">"))return a>b;
    if(!strcmp(op,"<"))return a<b;
    if(!strcmp(op,">="))return a>=b;
    if(!strcmp(op,"<="))return a<=b;
    return 0;
}

int cmp_str(const char *a,const char *b,const char *op)
{
    if(!strcmp(op,"=="))return strcmp(a,b)==0;
    if(!strcmp(op,"!="))return strcmp(a,b)!=0;
    return 0;
}

int match_condition(report *r,const char *field,const char *op,const char *value)
{
    if(strcmp(field,"severity")==0)
        return cmp_int(r->severity,atoi(value),op);

    if(strcmp(field,"timestamp")==0)
        return cmp_int((int)r->timestamp,(int)atol(value),op);

    if(strcmp(field,"category")==0)
        return cmp_str(r->category,value,op);

    if(strcmp(field,"inspector")==0)
        return cmp_str(r->inspector,value,op);

    return 0;
}

void notify_monitor(char *district,char *role,char *user)
{
    int pidfile=open(".monitor_pid",O_RDONLY);
    char path[256];
    sprintf(path,"%s/logged_district",district);
    int file=open(path,O_WRONLY|O_APPEND);
    if(file<0)
    {
        if(pidfile>=0) close(pidfile);
        return;
    }
    char message[256];
    time_t timenow=time(NULL);
    if(pidfile<0)
    {
        sprintf(message,"%ld [%s] %s: Monitor not found\n",timenow,role,user);
        write(file,message,strlen(message));
        close(file);
        return;
    }
    char buffer[32];
    read(pidfile,buffer,sizeof(buffer));
    close(pidfile);
    pid_t pid=atoi(buffer);
    if(kill(pid,SIGUSR1)==0)
    {
        sprintf(message,"%ld [%s] %s: Monitor notified successfully\n",timenow,role,user);
    }
    else
    {
        sprintf(message,"%ld [%s] %s: Failed to notify monitor\n",timenow,role,user);
    }
    write(file,message,strlen(message));
    close(file);
}

void add_report(char *district,char *role,char *user,double lat,double lon,char *category,int severity,char *desc)
{
    struct stat st;
    if(stat(district,&st)<0)
        create_district(district);
    char path[256];
    sprintf(path,"%s/reports.dat",district);
    if(stat(path,&st)<0)
    {
        printf("Nu pot accesa reports.dat\n");
        return;
    }
    if(strcmp(role,"manager")==0)
    {
        if(!(st.st_mode&S_IWUSR))
        {
            printf("Permisiune refuzata: managerul nu are drept de scriere\n");
            return;
        }
    }
    else
    {
        if(!(st.st_mode&S_IWGRP))
        {
            printf("Permisiune refuzata: inspectorul nu are drept de scriere\n");
            return;
        }
    }
    int file=open(path,O_APPEND|O_RDWR);
    if(file<0)
        return;
    report r;
    if(st.st_size==0)
    {
        r.id=1;
    }
    else
    {
        lseek(file,-sizeof(report),SEEK_END);
        report last;
        read(file,&last,sizeof(last));
        r.id=last.id+1;
    }
    lseek(file,0,SEEK_END);
    strncpy(r.inspector,user,NAME_LEN);
    r.latitude=(float)lat;
    r.longitude=(float)lon;
    strncpy(r.category,category,CATEGORY_LEN);
    r.severity=severity;
    r.timestamp=time(NULL);
    strncpy(r.description,desc,DESC_LEN);
    write(file,&r,sizeof(r));
    close(file);
    notify_monitor(district,role,user);
}

void list_reports(char *district,char *role)
{
    char path[256];
    sprintf(path,"%s/reports.dat",district);
    struct stat st;
    if(stat(path,&st)<0)
    {
        printf("Districtul nu exista sau reports.dat lipseste\n");
        return;
    }
    if(strcmp(role,"manager")==0)
    {
        if(!(st.st_mode&S_IRUSR))
        {
            printf("Permisiune refuzata: managerul nu are drept de citire\n");
            return;
        }
    }
    else
    {
        if(!(st.st_mode&S_IRGRP))
        {
            printf("Permisiune refuzata: inspectorul nu are drept de citire\n");
            return;
        }
    }
    char perm_str[10];
    mode_to_string(st.st_mode, perm_str);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime));
    printf("Fisier: %s | Permisiuni: %s | Dimensiune: %ld bytes | Ultima modificare: %s\n",
           path, perm_str, (long)st.st_size, time_buf);

    int file=open(path,O_RDONLY);
    if(file<0)
        return;
    report r;
    while(read(file,&r,sizeof(r))>0)
    {
        printf("%d %s %s %d\n",r.id,r.inspector,r.category,r.severity);
    }
    close(file);
}

void view_report(char *district,char *role,int id)
{
    char path[256];
    sprintf(path,"%s/reports.dat",district);
    struct stat st;
    if(stat(path,&st)<0)
    {
        printf("Districtul nu exista sau reports.dat lipseste\n");
        return;
    }
    if(strcmp(role,"manager")==0)
    {
        if(!(st.st_mode&S_IRUSR))
        {
            printf("Permisiune refuzata: managerul nu are drept de citire\n");
            return;
        }
    }
    else
    {
        if(!(st.st_mode&S_IRGRP))
        {
            printf("Permisiune refuzata: inspectorul nu are drept de citire\n");
            return;
        }
    }
    int file=open(path,O_RDONLY);
    if(file<0)
        return;
    report r;
    int found=0;
    while(read(file,&r,sizeof(r))>0)
    {
        if(r.id==id)
        {
            found=1;
            printf("ID:%d\n",r.id);
            printf("Inspector:%s\n",r.inspector);
            printf("Location:%f %f\n",r.latitude,r.longitude);
            printf("Category:%s\n",r.category);
            printf("Severity:%d\n",r.severity);
            printf("Timestamp:%ld\n",(long)r.timestamp);
            printf("Description:%s\n",r.description);
            break;
        }
    }
    if(!found)printf("Report not found\n");
    close(file);
}

void remove_report(char *district,char *role,int id)
{
    if(strcmp(role,"manager")!=0)
    {
        printf("Permisiune refuzata: doar managerul poate sterge rapoarte\n");
        return;
    }
    char path[256];
    sprintf(path,"%s/reports.dat",district);
    struct stat st;
    if(stat(path,&st)<0)
    {
        printf("reports.dat nu exista\n");
        return;
    }
    if(!(st.st_mode&S_IWUSR))
    {
        printf("Permisiune refuzata: managerul nu are drept de scriere\n");
        return;
    }
    int file=open(path,O_RDWR);
    if(file<0)
        return;
    report r,next;
    int found=0;
    while(read(file,&r,sizeof(r))>0)
    {
        if(r.id==id)
        {
            found=1;
            break;
        }
    }
    if(!found)
    {
        printf("Report not found\n");
        close(file);
        return;
    }
    while(read(file,&next,sizeof(next))>0)
    {
        lseek(file,-2*(off_t)sizeof(report),SEEK_CUR);
        write(file,&next,sizeof(next));
        lseek(file,(off_t)sizeof(report),SEEK_CUR);
    }
    struct stat st2;
    fstat(file,&st2);
    ftruncate(file,st2.st_size-(off_t)sizeof(report));
    close(file);
}

void update_threshold(char *district,char *role,int value)
{
    if(strcmp(role,"manager")!=0)
    {
        printf("Permisiune refuzata: doar managerul poate actualiza pragul\n");
        return;
    }
    char path[256];
    sprintf(path,"%s/district.cfg",district);
    struct stat st;
    if(stat(path,&st)<0)
    {
        printf("district.cfg nu exista\n");
        return;
    }
    if((st.st_mode&0777)!=0640)
    {
        printf("Config permissions changed\n");
        return;
    }
    if(!(st.st_mode&S_IWUSR))
    {
        printf("Permisiune refuzata: managerul nu are drept de scriere pe district.cfg\n");
        return;
    }
    int file=open(path,O_WRONLY|O_TRUNC);
    if(file<0)
        return;
    char buf[16];
    sprintf(buf,"%d\n",value);
    write(file,buf,strlen(buf));
    close(file);
}

void filter_reports(char *district,char *role,char conditions[][64],int count)
{
    char path[256];
    sprintf(path,"%s/reports.dat",district);
    struct stat st;
    if(stat(path,&st)<0)
    {
        printf("reports.dat nu exista\n");
        return;
    }
    /* FIX: verificare permisiuni si pentru filter */
    if(strcmp(role,"manager")==0)
    {
        if(!(st.st_mode&S_IRUSR))
        {
            printf("Permisiune refuzata: managerul nu are drept de citire\n");
            return;
        }
    }
    else
    {
        if(!(st.st_mode&S_IRGRP))
        {
            printf("Permisiune refuzata: inspectorul nu are drept de citire\n");
            return;
        }
    }
    int file=open(path,O_RDONLY);
    if(file<0)
        return;
    report r;
    char field[32],op[8],value[64];
    while(read(file,&r,sizeof(r))>0)
    {
        int ok=1;
        for(int i=0;i<count;i++)
        {
            if(!parse_condition(conditions[i],field,op,value))
            {
                ok=0;
                break;
            }
            if(!match_condition(&r,field,op,value))
            {
                ok=0;
                break;
            }
        }
        if(ok)
        {
            printf("%d %s %s %d\n",r.id,r.inspector,r.category,r.severity);
        }
    }
    close(file);
}

void remove_district(char *district,char *role)
{
    if(strcmp(role,"manager")!=0)
    {
        printf("Permisiune refuzata: doar managerul poate sterge districtul\n");
        return;
    }
    if(district==NULL||strlen(district)==0)
    {
        printf("Invalid name\n");
        return;
    }
    char path[256];
    sprintf(path,"%s/district.cfg",district);
    struct stat st;
    if(stat(path,&st)<0)
    {
        printf("Districtul nu exista\n");
        return;
    }
    if(!(st.st_mode&S_IWUSR))
    {
        printf("Permisiune refuzata: managerul nu are drept de scriere\n");
        return;
    }

    char link[256];
    sprintf(link,"active_reports-%s",district);
    unlink(link);

    pid_t pid=fork();
    if(pid<0)
    {
        printf("Fork failed\n");
        return;
    }
    if(pid==0)
    {
        execlp("rm","rm","-rf",district,NULL);
        perror("Exec failed");
        exit(1);
    }
    waitpid(pid,NULL,0);
}

int main(int argc,char *argv[])
{
    char *role=NULL;
    char *user=NULL;
    char *cmd=NULL;
    char *district=NULL;
    double lat=0,lon=0;
    char category[32]="";
    int severity=0;
    char desc[128]="";

    for(int i=1;i<argc;i++)
    {
        if(!strcmp(argv[i],"--role")&&i+1<argc)
            role=argv[++i];
        else if(!strcmp(argv[i],"--user")&&i+1<argc)
            user=argv[++i];
        else if(!strcmp(argv[i],"--lat")&&i+1<argc)
            lat=atof(argv[++i]);
        else if(!strcmp(argv[i],"--lon")&&i+1<argc)
            lon=atof(argv[++i]);
        else if(!strcmp(argv[i],"--category")&&i+1<argc)
            strcpy(category,argv[++i]);
        else if(!strcmp(argv[i],"--severity")&&i+1<argc)
            severity=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--desc")&&i+1<argc)
            strcpy(desc,argv[++i]);
        else if(argv[i][0]=='-'&&argv[i][1]=='-'&&i+1<argc)
        {
            cmd=argv[i];
            district=argv[++i];
        }
    }
    if(!role||!user||!cmd||!district)
    {
        printf("Argumente insuficiente.\n");
        return 1;
    }
    if(!strcmp(cmd,"--add"))
        add_report(district,role,user,lat,lon,category,severity,desc);
    else if(!strcmp(cmd,"--list"))
        list_reports(district,role);
    else if(!strcmp(cmd,"--view"))
        view_report(district,role,atoi(argv[argc-1]));
    else if(!strcmp(cmd,"--remove_report"))
        remove_report(district,role,atoi(argv[argc-1]));
    else if(!strcmp(cmd,"--update_threshold"))
        update_threshold(district,role,atoi(argv[argc-1]));
    else if(!strcmp(cmd,"--filter"))
    {
        char conditions[10][64];
        int count=0;
        int start=0;
        for(int i=1;i<argc;i++)
        {
            if(!strcmp(argv[i],"--filter")&&i+1<argc)
            {
                start=i+2;
                break;
            }
        }
        for(int i=start;i<argc&&count<10;i++)
        {
            if(argv[i][0]=='-'&&argv[i][1]=='-')
            {
                i++;
                continue;
            }
            strcpy(conditions[count++],argv[i]);
        }
        filter_reports(district,role,conditions,count);
    }
    else if(!strcmp(cmd,"--remove_district"))
        remove_district(district,role);
    else
        printf("Comanda necunoscuta: %s\n",cmd);

    return 0;
}

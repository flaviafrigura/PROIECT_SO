#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

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
        return cmp_int(r->timestamp,atol(value),op);

    if(strcmp(field,"category")==0)
        return cmp_str(r->category,value,op);

    if(strcmp(field,"inspector")==0)
        return cmp_str(r->inspector,value,op);

    return 0;
}

void add_report(char *district,char *role,char *user, double lat,double lon,char *category,int severity,char *desc)
{
    struct stat st;
    if(stat(district,&st)<0)
        create_district(district);
    char path[256];
    sprintf(path,"%s/reports.dat",district);
    stat(path,&st);
    if(strcmp(role,"manager")==0)
    {
        if(!(st.st_mode&S_IWUSR))
            return;
    }
    else
    {
        if(!(st.st_mode&S_IWGRP))
            return;
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
    r.latitude=lat;
    r.longitude=lon;
    strncpy(r.category,category,CATEGORY_LEN);
    r.severity=severity;
    r.timestamp=time(NULL);
    strncpy(r.description,desc,DESC_LEN);
    write(file,&r,sizeof(r));
    close(file);
}

void list_reports(char *district,char *role)
{
    char path[256];
    sprintf(path,"%s/reports.dat",district);
    struct stat st;
    stat(path,&st);
    if(strcmp(role,"manager")==0)
    {
        if(!(st.st_mode&S_IRUSR))
            return;
    }
    else
    {
        if(!(st.st_mode&S_IRGRP))
            return;
    }
    int file=open(path,O_RDONLY);
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
    stat(path,&st);
    if(strcmp(role,"manager")==0)
    {
        if(!(st.st_mode&S_IRUSR))return;
    }
    else
    {
        if(!(st.st_mode&S_IRGRP))return;
    }
    int file=open(path,O_RDONLY);
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
            printf("Timestamp:%ld\n",r.timestamp);
            printf("Description:%s\n",r.description);
            break;
        }
    }
    if(!found)printf("Report not found\n");
    close(file);
}

void remove_report(char *district,char *role,int id)
{
    struct stat st;
    if(strcmp(role,"manager")!=0)
        return;
    else
    {
        if(!(st.st_mode&S_IWUSR))
            return;
    }
    char path[256];
    sprintf(path,"%s/reports.dat",district);
    stat(path,&st);
    int file=open(path,O_RDWR);
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
        close(file);
        return;
    }
    while(read(file,&next,sizeof(next))>0)
    {
        lseek(file,-2*sizeof(report),SEEK_CUR);
        write(file,&next,sizeof(next));
        lseek(file,sizeof(report),SEEK_CUR);
    }
    struct stat st2;
    fstat(file,&st2);
    ftruncate(file,st2.st_size-sizeof(report));
    close(file);
}

void update_threshold(char *district,char *role,int value)
{
    struct stat st;
    char path[256];
    sprintf(path,"%s/district.cfg",district);
    stat(path,&st);
    if((st.st_mode&0777)!=0640)
    {
        printf("Config permissions changed\n");
        return;
    }
    if(strcmp(role,"manager")!=0)
        return;
    else
    {
        if(!(st.st_mode&S_IWUSR))
            return;
    }
    int file=open(path,O_WRONLY|O_TRUNC);
    char buf[16];
    sprintf(buf,"%d\n",value);
    write(file,buf,strlen(buf));
    close(file);
}

void filter_reports(char *district,char conditions[][64],int count)
{
    char path[256];
    sprintf(path,"%s/reports.dat",district);
    int file=open(path,O_RDONLY);
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
    struct stat st;
    if(strcmp(role,"manager")!=0)
        return;
    else
    {
        if(!(st.st_mode&S_IWUSR))
            return;
    }
    if(district==NULL||strlen(district)==0)
    {
        printf("Invalid name\n");
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
        return;
    }
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
        if(!strcmp(argv[i],"--role"))
            role=argv[++i];
        else if(!strcmp(argv[i],"--user"))
            user=argv[++i];
        else if(!strcmp(argv[i],"--lat"))
            lat=atof(argv[++i]);
        else if(!strcmp(argv[i],"--lon"))
            lon=atof(argv[++i]);
        else if(!strcmp(argv[i],"--category"))
            strcpy(category,argv[++i]);
        else if(!strcmp(argv[i],"--severity"))
            severity=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--desc"))
            strcpy(desc,argv[++i]);
        else if(argv[i][0]=='-')
        {
            cmd=argv[i];
            district=argv[++i];
        }
    }
    if(!role||!user||!cmd||!district)
        return 1;
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
        for(int i=3;i<argc;i++)
        {
            strcpy(conditions[count++],argv[i]);
        }
        filter_reports(district,conditions,count);
    }
    else if(!strcmp(cmd,"--remove_district"))
        remove_district(district,role);
    return 0;
}

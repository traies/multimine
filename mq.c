#define NORMAL_PR 0
#define WARNING_PR 1000
#define ERR_PR sysconf(_SC_MQ_PRIO_MAX) - 1
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>




int main(){
  mqd_t mqd;
  int i = fork();
  if(i != 0){//father
    int pr;
    mq_close(mqd);
    mq_unlink("/mq");

    char  m[100];
    struct   mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 100;
    attr.mq_curmsgs = 10;
     mqd = mq_open("/mq",O_RDONLY|O_CREAT,0666,&attr);
     if (mqd == (mqd_t) -1){
       printf("Not created \n");
       return 1;
     }


int j = 0;
   sleep(10);
while(j++<3){
        printf("devuelve %d\n",mq_receive(mqd,m,100,&pr));
        printf("%s\n",m );
        printf("%d\n",pr );
      }
        mq_close(mqd);
        mq_unlink("/mq");
      waitpid(i,NULL,0);
  }else{
       mqd = mq_open("/mq",O_WRONLY);
         mq_send(mqd,"NORMAL MSG",strlen("NORMAL MSG")+1,NORMAL_PR);
   mq_send(mqd,"WARNING MSG",strlen("WARNING MSG")+1,WARNING_PR);
           mq_send(mqd,"ERROR MSG",strlen("ERROR MSG")+1,ERR_PR);

  }
}

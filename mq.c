#define NORMAL_PR 0
#define WARNING_PR 1000
#define ERR_PR sysconf(_SC_MQ_PRIO_MAX) - 1
#include <comms.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <string.h>


int main(){
  Address addr;
  addr.fifo="/tmp/mq";

  Connection * c = mm_connect(&addr);
if(!c){
  printf("failed\n" );
  return 0;
}

  mqd_t mqd;
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

 mm_write( c, "got_connected",strlen("got_connected")+1);
int j = 0;

   sleep(10);
while(1){
        mq_receive(mqd,m,100,&pr);
        printf("Recibi mensaje %s de prioridad %d\n",m,pr );

      }
        mq_close(mqd);
        mq_unlink("/mq");

/*

*/

}

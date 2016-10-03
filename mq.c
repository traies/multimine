#define NORMAL_PR 0
#define WARNING_PR 1000
#define ERR_PR 32768
#include <comms.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <string.h>


int main(int argc,char * argv[]){
  char * addr = configuration("config",mm_commtype(),2);
  sleep(1);
  Connection * c = mm_connect(addr);
if(!c){
  printf("failed\n" );
  getchar();
  return 0;
}
mq_unlink("/mq");
  mqd_t mqd;
    int pr;
    char  m[100];
    char * type ;
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


while(1){
        mq_receive(mqd,m,100,&pr);
        switch(pr){
          case NORMAL_PR : type = "NORMAL";
                          break;
          case WARNING_PR : type = "WARNING";
                          break;
          case ERR_PR : type = "ERROR";
                      break;
        }
        printf("%s;tipo:%s\n",m,type );

      }


/*

*/

}

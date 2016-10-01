#include "comms.h"
#include "configurator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SIZE 128
#define CLIENT 0
#define SERVER 1
#define MQ 2
#define SERVER_MQ 3
static char * getField(char * line,int n);

char * configuration(char * config_file,int comm_mode,int mode){
  FILE * f = fopen(config_file,"r");
  int len=0;
  char  s_line[SIZE];
  fgets(s_line,SIZE,f);
  char  c_line[SIZE];
  fgets(c_line,SIZE,f);
  char  s_mq_line[SIZE];
  fgets(s_mq_line,SIZE,f);
  char  mq_line[SIZE];
  fgets(mq_line,SIZE,f);
  s_line[strlen(s_line)-1]=0;
  c_line[strlen(c_line)-1]=0;
  s_mq_line[strlen(s_mq_line)-1]=0;
  mq_line[strlen(mq_line)-1]=0;
  char * using ;
  if(mode == SERVER){
    using=s_line;
  } else if (mode == CLIENT){
    using=c_line;
  }else if ( mode == SERVER_MQ){
    using = s_mq_line;
  }else{
    using = mq_line;
  }
  char * tok;

  switch (comm_mode) {
    case COMM_FIFO:
      tok=getField(using,0);
      break;
    case COMM_SOCK_UNIX:
      tok=getField(using,1);
      break;
    case COMM_SOCK_INET:
      tok=getField(using,2);
      break;
    default:
      fclose(f);
      return NULL;
  }
  char * ans = malloc(strlen(tok)+1);
  strcpy(ans,tok);
  fclose(f);
  return ans;
}

static char * getField(char * line,int n){
  char * token;
  int i;
  for(i=0,token=strtok(line,";");token!=NULL && i<=n;
      token=strtok(NULL,";"),i++){
    if(i==n){
      return token;
    }
  }
  return NULL;
}

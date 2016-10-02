#include <server.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <queue.h>
#include <comms.h>
#include <unistd.h>
#include <signal.h>
#include <marsh.h>
#include <pthread.h>
#include <mqueue.h>
#include <string.h>
#include "configurator.h"
#include <msg_structs.h>
#include <game.h>
#include <server_marsh.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <common.h>
#define NORMAL_PR 0
#define WARNING_PR 1000
#define ERR_PR sysconf(_SC_MQ_PRIO_MAX) - 1

#define TRUE 1
#define FALSE 0
#define COLS 50
#define ROWS 20
#define MINES 100
#define BUF_SIZE 20000
#define DEFAULT_PLAYERS 1

typedef struct ClientPthreads
{
     pthread_t p_attr;
     pthread_t p_info;
     int8_t info_killflag, attr_killflag;
     int r_fd;
     int w_fd;
} ClientPthreads;

int64_t update_scores(Minefield * m, UpdateStruct * us )
{
     int64_t player_scores[8];
     int64_t players;

     if (m == NULL || us == NULL) {
	  return -1;
     }
     players = get_scores(m, player_scores);
     if (players < 0) {
	  return -1;
     }
     memcpy(&us->player_scores, player_scores, sizeof(int64_t[8]));
     us->players = players;
     return 0;
}

void srv_exit(ClientPthreads * pths[], int cli_i)
{
     system("rm /tmp/mine_serv");
      system("rm /tmp/mq");
      if (pths == NULL || cli_i == 0) {
	   return;
      }
      for (int i = 0; i < cli_i; i++) {
	   /* killing threads */
	   if (pths[i] == NULL) {
		continue;
	   }
	   pths[i]->attr_killflag = TRUE;
	   pthread_join(pths[i]->p_attr, NULL);
	   pths[i]->info_killflag = TRUE;
	   pthread_join(pths[i]->p_info, NULL);
      }
     exit(0);
}

/* DEBUG: tidy FIFO delete handler */
void sig_handler(int signo)
{
     if (signo == SIGINT){
	  srv_exit(NULL, 0);
     }
     return;
}

typedef struct attender
{
     Connection * con;
     int w_fd, buf_size;
     int8_t * killflag;
} Attender;

/* attendants threads run here */
void * attend(void * a)
{
     Attender * attr = (Attender *) a ;
     Connection * con = attr->con;
     int buf_size = attr->buf_size, w_fd = attr->w_fd, len;
     int8_t * buf = malloc(buf_size), * killflag = attr->killflag;
     int sel;
     struct timeval timeout;
     timeout.tv_sec = 1;
     timeout.tv_usec = 0;

     free(a);
     /* expects blocking read */
     while (!*killflag) {
	  if ((len = receive(con, buf, buf_size, &timeout)) > 0) {
	       printf("len %d\n", len);
	       write(w_fd, buf, len);
	  }
	  else if (len == -1) {
	       printf("disconnect \n");
	       break;
	  }
     }
     *killflag = TRUE;
     free(buf);
     pthread_exit(0);
}

typedef struct Informer
{
     Connection * con;
     int r_fd, buf_size;
     int8_t * killflag;
} Informer;

/* informer threads run here */
void * inform(void * a)
{
     Informer * info = (Informer *) a;
     Connection * con = info->con;
     int r_fd = info->r_fd;
     int8_t buf[BUF_SIZE];
     int8_t * killflag = info->killflag;
     int64_t len, buf_size = BUF_SIZE;
     fd_set fds;
     UpdateStruct * us;
     struct timeval timeout;
     timeout.tv_sec = 1;
     timeout.tv_usec = 0;

     /* expects blocking read */
     while(!(*killflag)) {
	  select(r_fd + 1, &fds, NULL, NULL, &timeout);
	  timeout.tv_sec = 1;
	  timeout.tv_usec = 0;
	  if (FD_ISSET(r_fd, &fds) && (len = read(r_fd, buf, 1)) > 0) {
	       if (buf[0] == INITGAME) {
		    read(r_fd, &buf[1], sizeof(InitStruct));
		    send_init(con, (InitStruct *) (buf + 1));
	       }
	       else if (buf[0] == UPDATEGAME) {
		    read(r_fd, &buf[1], sizeof(UpdateStruct));
		    us = (UpdateStruct *) &buf[1];
		    read(r_fd, &buf[sizeof(UpdateStruct) + 1], 4 * us->len);
		    send_update(con, (UpdateStruct *) (buf + 1));
	       }
	       else if (buf[0] == ENDGAME) {
		    read(r_fd, &buf[1], sizeof(EndGameStruct));
		    send_endgame(con, (EndGameStruct *) (buf + 1));
	       }
         else if (buf[0] == HIGHSCORE) {
           read(r_fd, &buf[1], sizeof(int));
          read(r_fd, &buf[1+sizeof(int)], (int)buf[1]*sizeof(Highscore));
          send_highscore(con, (Highscore *) (buf + 1+sizeof(int)));
        }
	  }
	  else if (FD_ISSET(r_fd, &fds) && len == 0) {
	       break;
	  }
	  else {
	       FD_SET(r_fd, &fds);
	  }
     }
     printf("closing informer\n");
     mm_disconnect(con);
     pthread_exit(0);
}



int64_t add_client(ClientPthreads * cli_arr [], fd_set * r_set, fd_set * w_set, int64_t * cli_i, int * attr_nfds, int * info_nfds, Connection * con, int attr_buf_size, int info_buf_size)
{
     ClientPthreads * cli_p;
     Attender * attender;
     Informer * informer;
     int attr_fd[2], info_fd[2];
     pthread_t pt_attr, pt_info;

     cli_p = malloc(sizeof(ClientPthreads));
     attender = malloc(sizeof(Attender));
     informer = malloc(sizeof(Informer));
     if (!informer || !attender || !cli_p) {
	  free(attender);
	  free(cli_p);
	  return -1;
     }
     cli_p->attr_killflag = FALSE;
     cli_p->info_killflag = FALSE;
     attender->con = con;
     informer->con = con;
     pipe(attr_fd);
     pipe(info_fd);
     attender->w_fd = attr_fd[1];
     informer->r_fd = info_fd[0];
     attender->buf_size = attr_buf_size;
     informer->buf_size = info_buf_size;
     attender->killflag = &(cli_p->attr_killflag);
     informer->killflag = &(cli_p->info_killflag);
     pthread_create(&pt_attr, NULL, attend, attender);
     pthread_create(&pt_info, NULL, inform, informer);
     cli_p->p_attr = pt_attr;
     cli_p->p_info = pt_info;
     cli_p->r_fd = attr_fd[0];
     cli_p->w_fd = info_fd[1];
     FD_SET(attr_fd[0], r_set);
     FD_SET(info_fd[1], w_set);
     cli_arr[*cli_i] = cli_p;
     *cli_i = *cli_i + 1;
     if (*attr_nfds < attr_fd[0] + 1) {
	  *attr_nfds = attr_fd[0] + 1;
     }
     if (*info_nfds < info_fd[1] + 1) {
	  *info_nfds = info_fd[1] + 1;
     }
     return 0;
}

int main(int argc, char * argv[])
{
     char * srv_addr;
     char * srv_addr_mq;
     Connection * c = NULL;
     int64_t rows = ROWS, cols = COLS, mines = MINES;
     Minefield * minef;
     char fifo[20];
     int sflag, q = 0, attr_nfds = 0, info_nfds = 0, max_size = 1000;
     int64_t  us_size, es_size;
     fd_set r_set, w_set;
     struct timeval timeout;
     int count = 0;
     QueryStruct * qs;
     UpdateStruct  * us;
     EndGameStruct es;
     InitStruct * is;
     int players;
     ClientPthreads * pths[10];
     int64_t cli_i = 0;
     Listener * lp = NULL;
     int8_t endflag = FALSE, msg_type;


     if (argc > 1) {
	  sscanf(argv[1],"%d",&players);
     }
     else {
	  players = DEFAULT_PLAYERS;
     }
     us_size = cols * rows * 4 + sizeof(UpdateStruct);
     us = malloc(us_size);

     if (!us) {
	  printf("memory error.\n");
	  return -1;
     }
     us->len = 0;

     int8_t u_flag = 0;
     int8_t h_flag = 0;
     int player;
      Highscore * h;

     timeout.tv_sec = 10;
     timeout.tv_usec = 0;

     signal(SIGINT,sig_handler);

     //system("rm /tmp/mine_serv");

     system("rm /tmp/mq");
     mq_unlink("/mq");

     /* setting fifo path */
     sprintf(fifo, "/tmp/mine_serv");

     srv_addr = fifo;

     /*
     srv_addr_mq ="/tmp/mq";

     char * addr = configuration("config",mm_commtype(),3);

      lp = mm_listen(addr);
      if (lp == NULL) {
    printf("fracaso\n");
    return 0;
      }


     system("gnome-terminal -e ./bin/mq.out");


     c = mm_accept(lp) ;
       char msg[100]="";

       while(strcmp(msg,"got_connected") != 0){
       mm_read(c,msg,strlen("got_connected")+1);
     }

     mm_disconnect(c);
       mqd_t mqd = mq_open("/mq",O_WRONLY);



       mm_disconnect_listener(lp);
     */


     /* open connection */

      char * addr = configuration("config",mm_commtype(),1);

      lp = mm_listen(addr);
      if (lp == NULL) {
	   printf("fracaso\n");
	   return 0;
      }

     /* wait for connections */
     while ( count < players && (c = mm_accept(lp)) != NULL) {
	  /* established conection on c, needs to create thread */
	  printf("conexion establecida. Creando thread.\n");
	  add_client(pths, &r_set, &w_set, &cli_i, &attr_nfds, &info_nfds, c, sizeof(QueryStruct) + 1, us_size);
	  printf("thread creado..\n");
	  // mq_send(mqd,"GOT A CONNECTION",strlen("GOT A CONNECTION")+1,NORMAL_PR);
	  count++;
     }

     mm_disconnect_listener(lp);
     lp = NULL;

     if (count < players) {
	  /* connections failed */
	  srv_exit(NULL, 0);
     }
     /* create minefield */
     minef = create_minefield(cols, rows, mines, players);

     char * data_struct;
     char * highscore_struct;
     int data_size;
     data_struct = malloc(sizeof(UpdateStruct) + 4 * cols * rows + 1);
     highscore_struct = malloc(sizeof(Highscore)*10 + 1+sizeof(int));
     if (!data_struct || !highscore_struct) {
	  printf("memory error\n");
	  return -1;
     }
     data_struct[0] = INITGAME;
     is = &data_struct[1];
     is->cols = cols;
     is->rows = rows;
     is->mines = mines;
     is->players = cli_i;
     /* send game info to clients */
     for (int i = 0; i < cli_i; i++) {
	  is->player_id = i;
	  write(pths[i]->w_fd, data_struct, sizeof(InitStruct) + 1);
     }

     while (!q) {

	  /* read() */
	  sflag = select(attr_nfds, &r_set, NULL, NULL, &timeout);
	  timeout.tv_sec = 10;

	  if (sflag == 0) {
	       for (int i = 0; i < cli_i; i++) {
		    FD_SET(pths[i]->r_fd, &r_set);
	       }
	       printf("listening...\n");
	  }

	  if (sflag < 0) {
	       /* timeout expired*/
	       srv_exit(pths,cli_i);
	  }
	  else if (sflag > 0){
	       for(int i = 0; i < cli_i; i++) {
		    if (pths[i] == NULL) {
			 continue;
		    }
		    if (pths[i]->attr_killflag) {
			 printf("kill\n");
			 pths[i]->info_killflag = TRUE;
			 printf("kill\n");

			 pths[i] = NULL;

			 //	 reset_score(m, i);
		    }
		    else if (FD_ISSET(pths[i]->r_fd, &r_set)) {
			 /* read fd */
			 if (read(pths[i]->r_fd,data_struct,max_size) > 0) {
			      if (data_struct[0] == QUERYMINE) {
				   qs = (QueryStruct *) &data_struct[1];
				   if (update_minefield(minef, qs->x, qs->y, i, us) > 0) {
					u_flag = 1;
					//    sprintf(msg,"x: %d y: %d player:%d",(int)qs.x,(int)qs.y, (int) i);
					//	   mq_send(mqd,msg,strlen(msg)+1,NORMAL_PR);
        }
      }else if (data_struct[0] == HIGHSCORE){
              player= i;
              h_flag = 1;
            }
          }
			 }
		    else {
			 FD_SET(pths[i]->r_fd, &r_set);
		    }
	       }

	       /* send to all

		  while(!FD_EMPTY(&w_set)) {
	             select(info_nfds, NULL, &w_set, NULL, &timeout);
             }
	       */
	       endflag = check_win_state(minef, &es);
	       if (u_flag){


			 //	 sprintf(msg,"unveiled x:%d y:%d n:%d player:%d ", us->tiles[i].x, us->tiles[i].y, us->tiles[i].nearby, us->tiles[i].player);
				 //	mq_send(mqd,msg,strlen(msg)+1,NORMAL_PR);

		    update_scores(minef, us);
		    msg_type = UPDATEGAME;
		    data_struct[0] = msg_type;
		    data_size = sizeof(UpdateStruct) + 4 * us->len;
		    memcpy(&data_struct[1], us, data_size);
		    for (int i = 0; i < cli_i; i++) {
			 if (pths[i] == NULL) {
			      continue;
			 }
			 //write(pths[i]->w_fd, &msg_type, 1);

			 write(pths[i]->w_fd, data_struct, data_size + 1);
		    }
		    us->len = 0;
		    u_flag = 0;
      }else if (h_flag){
        int count;
        open_database();
        insert_highscore("hola",520);
        insert_highscore("hoa",450);
        insert_highscore("hla",400);
        insert_highscore("",52);
       h = get_highscores(&count);
        msg_type = HIGHSCORE;
        highscore_struct[0] = msg_type;
        highscore_struct[1] = count;
		    data_size = sizeof(Highscore) *count;
		    memcpy(&highscore_struct[1+sizeof(int)], h, data_size);
	  write(pths[player]->w_fd, highscore_struct, data_size + 1);
        h_flag = 0;
      }
	       if (endflag) {
		    msg_type = ENDGAME;
		    data_struct[0] = msg_type;
		    memcpy(&data_struct[1], &es, sizeof(EndGameStruct));

		    for (int i = 0; i < cli_i;i++) {
			 if (pths[i] == NULL) {
			      continue;
			 }

			 write(pths[i]->w_fd, data_struct, sizeof(EndGameStruct) + 1);
		    }
		    q = TRUE;
	       }
	  }
     }

     printf("game ended.\n");
     getchar();

     srv_exit(pths, cli_i);
     //minef = create_minefield(cols, rows, mines);

     /* game loop */
     // while (player > 1)
     //    read();
     //    update();
     //    print_log();
     //    send();



     /* fifo clean up */
     /* final */
     // close_connections();
     // free_minefield();
     // return;
}

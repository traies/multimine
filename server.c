#include <server.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
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
#define MEMORY_ERROR -1
#define TRUE 1
#define FALSE 0
#define COLS 50
#define ROWS 20
#define MINES 100
#define BUF_SIZE 50000
#define DEFAULT_PLAYERS 1

typedef struct ClientPthreads
{
	pthread_t p_attr;
	pthread_t p_info;
	int r_fd;
	int w_fd;
	int8_t * attr_killflag;
	int8_t * info_killflag;
	Connection * con;
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
	int8_t * buf, * recv_buf;
	struct timeval timeout;
	int8_t * killflag = attr->killflag;
	free(attr);
	buf = malloc (buf_size);
	recv_buf = malloc(buf_size);
	if (!buf || !recv_buf) {
		return NULL;
	}
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	/* expects blocking read */
	while (!*killflag) {
		if ((len = receive(con, buf, recv_buf, buf_size, &timeout)) > 0) {
			write(w_fd, buf, len);
		}
		else if (len == -1) {
			printf("disconnect \n");
			break;
		}
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
	}
	/* close fd to main server */
	close(w_fd);
	free(buf);
	free(recv_buf);
	free(killflag);
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
	// TODO cambiar a malloc()
	int8_t buf[BUF_SIZE];
	int64_t len;
	int8_t * killflag = info->killflag;
	fd_set fds;
	UpdateStruct * us;
	struct timeval timeout;
	free(info);
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(r_fd, &fds);
	/* expects blocking read */
	while(!*killflag) {
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
			else if (buf[0] == HIGHSCORES) {
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
	free(killflag);
	pthread_exit(0);
}

typedef struct Greeter {
	int8_t * greet_killflag;
	Listener * lp;
	pthread_mutex_t * mutex, * serv_mutex;
} Greeter;

void * greet(void * g)
{
	Greeter * greet = (Greeter * ) g;
	int8_t * killflag = greet->greet_killflag;
	struct timeval timeout;
	pthread_mutex_t * mutex = greet->mutex;
	pthread_mutex_t * serv_mutex = greet->serv_mutex;
	Connection * con;
	Listener * lp = greet->lp;
	BusyStruct bs;
	free(greet);
	strcpy(bs.message, "El servidor esta ocupado.");
	bs.length = strlen(bs.message);
	while (!*killflag) {
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		/* lock mutex */
		printf("bloqueo el mutex\n");
		pthread_mutex_lock(mutex);
		/* accept incoming connections */
		if (mm_select_accept(lp, &timeout) > 0) {
			/* answer with busy message */
			printf("estoy aceptando...\n");
			con = mm_accept(lp);
			send_busy(con, &bs);
			/* disconnect */
			mm_disconnect(con);
		}
		/* unlock mutex */
		printf("desbloqueo el mutex\n");
		pthread_mutex_unlock(mutex);
		pthread_mutex_lock(serv_mutex);
		pthread_mutex_unlock(serv_mutex);
	}
	pthread_mutex_destroy(mutex);
	free(killflag);
	pthread_exit(0);
}

int64_t add_client(ClientPthreads * cli_arr [], fd_set * r_set, fd_set * w_set,
	int64_t count, int * attr_nfds, int * info_nfds, Connection * con,
	int attr_buf_size, int info_buf_size)
{
	ClientPthreads * cli_p;
	Attender * attender;
	Informer * informer;
	int attr_fd[2], info_fd[2];
	pthread_t pt_attr, pt_info;
	int8_t * attr_killflag = malloc(sizeof(int8_t));
	int8_t * info_killflag = malloc(sizeof(int8_t));
	cli_p = malloc(sizeof(ClientPthreads));
	attender = malloc(sizeof(Attender));
	informer = malloc(sizeof(Informer));
	if (!informer || !attender || !cli_p || !attr_killflag || !info_killflag) {
		free(attender);
		free(cli_p);
		free(attr_killflag);
		free(info_killflag);
		return -1;
	}
	*attr_killflag = false;
	*info_killflag = false;
	attender->con = con;
	informer->con = con;
	pipe(attr_fd);
	pipe(info_fd);
	attender->w_fd = attr_fd[1];
	informer->r_fd = info_fd[0];
	attender->buf_size = attr_buf_size;
	informer->buf_size = info_buf_size;
	attender->killflag = attr_killflag;
	informer->killflag = info_killflag;
	pthread_create(&pt_attr, NULL, attend, attender);
	pthread_create(&pt_info, NULL, inform, informer);
	cli_p->attr_killflag = attr_killflag;
	cli_p->info_killflag = info_killflag;
	cli_p->p_attr = pt_attr;
	cli_p->p_info = pt_info;
	cli_p->r_fd = attr_fd[0];
	cli_p->w_fd = info_fd[1];
	cli_p->con = con;
	FD_SET(attr_fd[0], r_set);
	FD_SET(info_fd[1], w_set);
	cli_arr[count] = cli_p;
	if (*attr_nfds < attr_fd[0] + 1) {
		*attr_nfds = attr_fd[0] + 1;
	}
	if (*info_nfds < info_fd[1] + 1) {
		*info_nfds = info_fd[1] + 1;
	}
	return 0;
}

static void copy_endgame_struct(int8_t * data_struct, EndGameStruct * endgame_struct, int size)
{
	EndGameStruct * aux =  (EndGameStruct *) &data_struct[1];
	memset(data_struct, 0, size);
	data_struct[0] = ENDGAME;
	aux->players = endgame_struct->players;
	aux->winner_id = endgame_struct->winner_id;
	for (int i = 0; i < aux->players; i++) {
		aux->player_scores[i] = endgame_struct->player_scores[i];
	}
	return;
}

static void copy_update_struct(int8_t * data_struct, UpdateStruct * update_struct, int data_max_size, int data_size)
{
	UpdateStruct * aux = (UpdateStruct *)&data_struct[1];
	memset(data_struct, 0, data_max_size);
	data_struct[0] = UPDATEGAME;
	aux->len = update_struct->len;
	aux->players = update_struct->players;
	for (int i=0; i < aux->players; i++) {
		aux->player_scores[i] = update_struct->player_scores[i];
	}
	for (int i=0; i < aux->len; i++) {
		aux->tiles[i].x = update_struct->tiles[i].x;
		aux->tiles[i].y = update_struct->tiles[i].y;
		aux->tiles[i].nearby = update_struct->tiles[i].nearby;
		aux->tiles[i].player = update_struct->tiles[i].player;
	}
	return;
}
int64_t attend_requests(Minefield * minef, int64_t msize,
	ClientPthreads * pths[8], int64_t players,mqd_t mqd)
{
	fd_set r_set;
	int8_t q = false, uflag = false, msg_type, endflag = false,h_flag=false,h_add_flag = false;
	int64_t nfds = 0, sflag, rlen, data_size, data_max_size, pleft ;
	UpdateStruct * us;
	QueryStruct * qs;
	EndGameStruct es;
	struct timeval timeout;
	int8_t * data_struct;
	char * highscore_struct;
	int player;
	Highscore * h;
	pleft = players;
	char msg[100]=" ";
	/* init data buffer */
	data_max_size = sizeof(UpdateStruct) + msize * 4 + 1;
	data_struct = calloc(1, data_max_size);
	us = calloc(1, data_max_size);
	highscore_struct = malloc(sizeof(Highscore)*10 + 1 +sizeof(int));
	if (!data_struct || !us || !highscore_struct) {
		free_minefield(minef);
		return MEMORY_ERROR;
	}
	us->len = 0;

	/* set select timeout */
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	/* fill in read file descriptor set */
	FD_ZERO(&r_set);
	for (int i = 0; i < players; i++) {
		if (nfds < pths[i]->r_fd) {
			nfds = pths[i]->r_fd;
		}
		FD_SET(pths[i]->r_fd, &r_set);
	}
	nfds++;

	while(!q && pleft > 0)  {
		/* select on pthreads read */
		sflag = select(nfds, &r_set, NULL, NULL, &timeout);
		/* reset timeout */
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		if (sflag == 0) {
			/* timeout expired */
			for (int i = 0; i < players; i++) {
				if (pths[i] == NULL) {
					continue;
				}
				FD_SET(pths[i]->r_fd, &r_set);
			}
			printf("listening...\n");
		}
		else if (sflag < 0) {
			/* an error ocurred */
			return -1;
		}
		else {
			/* a fd is ready */
			for (int i = 0; i < players; i++) {
				/* skip closed connections */
				if (pths[i] == NULL) {
					continue;
				}
				if (FD_ISSET(pths[i]->r_fd, &r_set)) {
					/* read first byte */
					rlen = read(pths[i]->r_fd, data_struct, 1);
					if (rlen > 0) {
						/* request arrived */
						if (data_struct[0] == QUERYMINE) {
							/* read remaining data */
							read(pths[i]->r_fd, &data_struct[1], sizeof(QueryStruct));
							qs = (QueryStruct *) &data_struct[1];
							sprintf(msg,"PLAYER %d OPENED x: %d, y: %d ", i, qs->x, qs->y);
							mq_send(mqd,msg,strlen(msg)+1,NORMAL_PR);
							if (update_minefield(minef, qs->x, qs->y, i, us)) {
								uflag = true;
							}
						}
						else if (data_struct[0] == HIGHSCORES){
							player= i;
							h_flag = true;
						}
						else if (data_struct[0] == HIGHSCORE_ADD){
							read(pths[i]->r_fd, &data_struct[1], sizeof(Highscore));
							h_add_flag = true;
						}
					}
					else if (rlen == 0) {
						/* client disconnected */
						sprintf(msg,"PLAYER %d DISCONECTED", i);
						mq_send(mqd,msg,strlen(msg)+1,WARNING_PR);
						reset_score(minef, i);
						close(pths[i]->w_fd);
						FD_CLR(pths[i]->r_fd, &r_set);
						free(pths[i]);
						pths[i] = NULL;
						pleft--;
					}
				}
				else {
					/* add fd to set */
					FD_SET(pths[i]->r_fd, &r_set);
				}
			}
			if(!endflag){
				endflag = check_win_state(minef, &es);
			}
			if (uflag) {
				/* send updates to players */
				uflag = false;
				update_scores(minef, us);
				msg_type = UPDATEGAME;
				copy_update_struct(data_struct, us, data_max_size, data_size);
				data_size = sizeof(UpdateStruct) + us->len * 4;
				for (int i = 0; i < players; i++) {
					if (pths[i] == NULL) {
						continue;
					}
					write(pths[i]->w_fd, data_struct, data_size + 1);
				}
				us->len = 0;
			}
			else if (h_flag){
				int count,size;
				h = get_highscores(&count);
				msg_type = HIGHSCORES;
				highscore_struct[0] = msg_type;
				highscore_struct[1] = count;
				size = sizeof(Highscore) *count;
				memcpy(&highscore_struct[1+sizeof(int)], h, size);
				write(pths[player]->w_fd, highscore_struct, size + 1);
				h_flag = false;
			}
			else if (h_add_flag){
				h = (Highscore *)&data_struct[1];
				if(h[0].score > 0) {
					insert_highscore(h[0].name,h[0].score);
					q = true;
				}
			}
			if (endflag && !h_add_flag) {
				/* end game conditions where met */
				copy_endgame_struct(data_struct, &es, data_max_size);
				for (int i = 0; i < players; i++) {
					if (pths[i] == NULL) {
						continue;
					}
					write(pths[i]->w_fd, data_struct, sizeof(EndGameStruct) + 1);
				}
				q = true;
			}
		}
	}
	free(us);
	free(data_struct);
	free(highscore_struct);
	free_minefield(minef);
	for (int i = 0; i < players; i++) {
		if (pths[i] != NULL) {
			*pths[i]->attr_killflag = true;
			pthread_join(pths[i]->p_attr, NULL);
			*pths[i]->info_killflag = true;
			pthread_join(pths[i]->p_info, NULL);
			mm_disconnect(pths[i]->con);
			free(pths[i]);
			pths[i] = NULL;
		}
	}
	return 0;
}

int64_t host_game(ClientPthreads * pths[8], int64_t players, int64_t rows,
	int64_t cols, int64_t mines,mqd_t mqd)
{
	Minefield * minef;
	char * data_struct;
	int64_t ret;
	InitStruct * is;

	/* create minefield */
	minef = create_minefield(cols, rows, mines, players);
	if (minef == NULL) {
		return MEMORY_ERROR;
	}
	/* init data buffer with max size */
	data_struct = calloc(1, sizeof(InitStruct) + 1);
	if (!data_struct) {
		free_minefield(minef);
		return MEMORY_ERROR;
	}

	/* init INITGAME message struct  */
	data_struct[0] = INITGAME;
	is = (InitStruct *) &data_struct[1];
	is->cols = cols;
	is->rows = rows;
	is->mines = mines;
	is->players = players;

	/* send game info to clients */
	for (int i = 0; i < players; i++) {
		is->player_id = i;
		write(pths[i]->w_fd, data_struct, sizeof(InitStruct) + 1);
	}

	/* free init data buffer */
	free(data_struct);

	/* attend requests */
	ret = attend_requests(minef, rows * cols, pths , players,mqd);
	return ret;
}

static ClientPthreads * pths[8];
static int cli_i = 0;
static Listener * lp = NULL;
void srv_exit()
{
	for (int i = 0; i < cli_i; i++) {
		if (pths[i] != NULL) {
			*pths[i]->attr_killflag = true;
			pthread_join(pths[i]->p_attr, NULL);
			*pths[i]->info_killflag = true;
			pthread_join(pths[i]->p_info, NULL);
			mm_disconnect(pths[i]->con);
			free(pths[i]);
			pths[i] = NULL;
		}
	}
	close_database();
	if (lp != NULL) {
		mm_disconnect_listener(lp);
	}
	exit(0);
	return;
}

int main(int argc, char * argv[])
{
	Connection * c = NULL;
	int64_t rows = ROWS, cols = COLS, mines = MINES;
	int attr_nfds = 0, info_nfds = 0;
	fd_set r_set, w_set;
	int count, players;

	signal(SIGINT, srv_exit);

	if (argc > 1) {

		sscanf(argv[1],"%d",&players);
		if(players > 7 || 0 >= players ){
			printf("%d no es un numero valido. Inserte un numero de jugadores del 1 al 7",players);
			return 1;
		}
	} else {
		players = DEFAULT_PLAYERS;
	}
	int us_size = cols * rows * 4 + sizeof(UpdateStruct);

	//int8_t u_flag = 0;
	//int8_t h_flag = 0;
	//Highscore * h;

	char * addr = configuration("config",mm_commtype(),3);

	lp = mm_listen(addr);
	if (lp == NULL) {
		printf("fracaso\n");
		return 0;
	}
	system("gnome-terminal -e ./bin/mq.out");
	c = mm_accept(lp);
	char msg[100]="";

	while(strcmp(msg,"got_connected") != 0){
		mm_read(c,(int8_t *) msg,strlen("got_connected")+1);
	}
	mm_disconnect(c);
	
	mqd_t mqd = mq_open("/mq",O_WRONLY);
	mm_disconnect_listener(lp);

	/* open connection */
	addr = configuration("config",mm_commtype(),1);
	lp=mm_listen(addr);
	if(lp==NULL){
		printf("fracaso\n");
		return 0;
	}

	/* start greeter */
	pthread_mutex_t * greet_mutex = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_t * serv_mutex = malloc(sizeof(pthread_mutex_t));
	if (greet_mutex == NULL || serv_mutex == NULL) {
		return -1;
	}
	pthread_mutex_init(greet_mutex, NULL);
	pthread_mutex_init(serv_mutex, NULL);
	pthread_mutex_lock(greet_mutex);
	Greeter * greeter = malloc(sizeof(Greeter));
	if (greeter == NULL) {
		return -1;
	}
	int8_t * greet_killflag = malloc(sizeof(int8_t));
	if (greet_killflag == NULL) {
		return -1;
	}
	*greet_killflag = false;
	greeter->greet_killflag = greet_killflag;
	greeter->lp = lp;
	greeter->mutex = greet_mutex;
	greeter->serv_mutex = serv_mutex;
	pthread_t p_greet;
	pthread_create(&p_greet, NULL, greet, greeter);

	while (true) {
		/* wait for connections */
		count = 0;
		while ( count < players && (c = mm_accept(lp)) != NULL) {
			/* established conection on c, needs to create thread */
			printf("conexion establecida. Creando thread.\n");
			add_client(pths, &r_set, &w_set, count, &attr_nfds, &info_nfds, c, sizeof(Highscore) + 1, us_size);
			printf("thread creado..\n");
			mq_send(mqd,"GOT A CONNECTION",strlen("GOT A CONNECTION")+1,NORMAL_PR);
			count++;
			cli_i++;
		}

		if (count < players) {
			/* connections failed */
			return -1;
		}
		pthread_mutex_unlock(greet_mutex);
		host_game(pths, players, rows, cols, mines,mqd);
		cli_i = 0;
		printf("me voy a desbloquear\n");
		pthread_mutex_lock(serv_mutex);
		pthread_mutex_lock(greet_mutex);
		pthread_mutex_unlock(serv_mutex);
		printf("me desbloquie.\n");
	}
	mm_disconnect_listener(lp);
	close_database();
	return 0;
}

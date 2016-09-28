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



#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

#define NORMAL_PR 0
#define WARNING_PR 1000
#define ERR_PR sysconf(_SC_MQ_PRIO_MAX) - 1
#define min(a,b)    ((a < b)? a:b)
#define max(a,b)    ((a < b)? b:a)
#define clamp(a, b, c) max(min(a,c),b)
#define TRUE 1
#define FALSE 0
#define COLS 50
#define ROWS 20
#define MINES 100
#define BUF_SIZE 20000
struct Sector;
struct SectorNode;

typedef struct
{

     int64_t x, y;
     struct Sector * sector;
     int64_t ownerid;
     int64_t nearby;
     int8_t ismine;
     int8_t issectorowner;
     int8_t visited;
} Tile;

typedef struct
{
     const int8_t name[10];
     int64_t color;
     int64_t id;
} Player;

typedef struct Sector
{
     int64_t size;
     struct SectorNode * node;
} Sector;

typedef struct SectorNode
{
     Tile * tile;
     struct SectorNode * next;
} SectorNode;

typedef struct Minefield
{
     int64_t cols, rows;
     Tile *** tiles;
} Minefield;

int8_t construct_sectors(Minefield * m);
Tile * uncover_tile(Minefield * minefield, uint64_t x, uint64_t y);
int8_t solo_sector(Tile * t);
int8_t multi_sector(Minefield * m, int64_t x, int64_t y);
int8_t add_tile(Sector * s, Tile * t);
void free_minefield(Minefield_p m);
void free_tile(Tile * t);
void free_sector(Sector * s);
Tile * uncover_tile(Minefield * minefield, uint64_t x, uint64_t y);

Minefield_p create_minefield(int64_t cols, int64_t rows, int64_t mines)
{
     int64_t size = cols * rows;
     int64_t * rand_arr, * mine_arr;
     int64_t aux, x, y, c = 0, r = 0, maxkx, maxky;
     Minefield * minefield;
     void * t;
     Tile * tile;

     /* allocate auxiliar structures */
     rand_arr = malloc(sizeof(int64_t) * size);
     mine_arr = malloc(sizeof(int64_t) * mines);
     /* allocate minefield */
     minefield = malloc(sizeof(Minefield));
     if (!rand_arr || !mine_arr || !minefield) {
       /* mallocs failed */
       free(rand_arr);
       free(mine_arr);
       free(minefield);
       return NULL;
     }

     /* initialize minefield */
     minefield->cols = cols;
     minefield->rows = rows;
     t = minefield->tiles = malloc(sizeof(Tile **) * cols);
     while(t && c < cols) {
	  t = minefield->tiles[c] = malloc(sizeof(Tile*) * rows);
	  while(t && r < rows) {
	       t = minefield->tiles[c][r] = calloc(1, sizeof(Tile));
	       if (t){
		    minefield->tiles[c][r]->x = c;
		    minefield->tiles[c][r]->y = r;
		    minefield->tiles[c][r]->ownerid = -1;
		    minefield->tiles[c][r]->sector = NULL;
		    minefield->tiles[c][r]->visited = FALSE;
	       }
	       r++;
	  }
	  r = 0;
	  c++;
     }
     if (!t){
	  /* malloc failed */
	  free(rand_arr);
	  free(mine_arr);
	  for (int i = 0; i < c; i++) {
	       for (int j = 0; j < r; j++) {
		    free(minefield->tiles[i][j]);
	       }
	       free(minefield->tiles[i]);
	  }
	  free(minefield->tiles);
	  free(minefield);
	  return NULL;
     }
     /* initialize random array */
     for (int i = 0; i < size; i++){
	  rand_arr[i] = i;
     }
     srand(time(0));
     /* randomize mine locations */
     for (int j = 0; j < mines ; j++) {
	  aux = rand() % (size - j);
	  mine_arr[j] = rand_arr[aux];
	  rand_arr[aux] = rand_arr[size - j - 1];
     }
     /* asign mines to tiles */
     for (int k = 0; k < mines ; k++) {
	  x = mine_arr[k] % cols;
	  y = mine_arr[k] / cols;
	  tile = minefield->tiles[x][y];
	  tile->ismine = TRUE;
	  maxkx = min(x+1,cols-1);
	  maxky = min(y+1,rows-1);
	  for (int kx = max(x-1,0); kx <= maxkx;kx++){
	       for (int ky = max(y-1, 0); ky <= maxky; ky++) {
		    (minefield->tiles[kx][ky]->nearby)++;
	       }
	  }
     }

     /* free auxiliar arrays */
     free(rand_arr);
     free(mine_arr);

     /* construct sectors */
     int f = construct_sectors(minefield);
     if (f){
	  free_minefield(minefield);
	  minefield = NULL;
     }
     return minefield;
}

/* constructs mine sectors on minefield initialization */
int8_t construct_sectors(Minefield * m)
{
     Tile * t;
     int8_t malloc_error = FALSE;

     if (!m){
	  return -1;
     }
     for (int i = 0; !malloc_error && i < m->cols; i++) {
	  for (int j = 0; j < m->rows && !malloc_error; j++) {
	       t = m->tiles[i][j];
	       if (t->sector || t->ismine){
		    continue;
	       }
	       if (t->nearby > 0){
		    /* solo sector */
		    malloc_error = solo_sector(t);
	       }
	       else {
		    /* multiple tiles sector */
		    malloc_error = multi_sector(m, i, j);
	       }
	  }
     }
     return malloc_error;
}

/* multiple tile sector initialization */
int8_t multi_sector(Minefield * m, int64_t x, int64_t y)
{
     int8_t malloc_error = FALSE;
     int64_t maxx, maxy, vc = 0;
     Queue_p q = new_queue();
     Tile * t;
     Sector *s = calloc(1,sizeof(Sector));
     int64_t (*vis)[2];
     vis = malloc(sizeof(int64_t[2]) * m->rows * m->cols);
     if (!q || !s || !vis){
	  return -1;
     }
     m->tiles[x][y]->issectorowner = TRUE;
     malloc_error = enque(q, m->tiles[x][y]);
     if(malloc_error) {
	  return -1;
     }

     while (!malloc_error && peek(q)) {
	  t = deque(q);
	  if (!t){
	       malloc_error = -1;
	       break;
	  }
	  if (t->ismine || t->visited) {
	       continue;
	  }
	  t->visited = TRUE;
	  vis[vc][0] = t->x;
	  vis[vc][1] = t->y;
	  vc++;
	  malloc_error = add_tile(s, t);
	  if (malloc_error || t->nearby > 0){
	       continue;
	  }
	  t->sector = s;
	  maxx = min(t->x + 1, m->cols-1);
	  maxy = min(t->y + 1, m->rows-1);
	  for (int i = max(t->x - 1, 0); !malloc_error && i <= maxx; i++){
	       for (int j = max(t->y-1,0); !malloc_error && j <= maxy; j++){
		    if ((i == t->x && j == t->y) || m->tiles[i][j]->visited == TRUE){
			 continue;
		    }
		    malloc_error = enque(q, m->tiles[i][j]);
	       }
	  }
     }
     if (malloc_error) {
	  free_sector(s);
     }
     for (int c=0; c < vc; c++) {
	  m->tiles[vis[c][0]][vis[c][1]]->visited = FALSE;
     }
     free(vis);
     free(q);
     return malloc_error;
}

/* solo tile sector initialization */
int8_t solo_sector(Tile * t)
{
     Sector * s = malloc(sizeof(Sector));
     if (!s || !t) {
	  return -1;
     }
     s->node = malloc(sizeof(SectorNode));
     if (!s->node){
	  free(s);
	  return -1;
     }
     s->size = 1;
     s->node->next = NULL;
     s->node->tile = t;
     t->sector = s;
     t->issectorowner = TRUE;
     return 0;
}

/* add tile to sector */
int8_t add_tile(Sector * s, Tile * t)
{
     SectorNode * n = malloc(sizeof(SectorNode)), *aux;
     if (!n || !s) {
	  return -1;
     }
     aux = s->node;
     n->tile = t;
     n->next = aux;
     s->node = n;
     (s->size)++;
     return 0;
}

/* free minefield's structures */
void free_minefield(Minefield_p m)
{
     if (!m){
	  return;
     }
     for (int i = 0; i < m->cols; i++){
	  for (int j=0; j < m->rows; j++) {
	       free_tile(m->tiles[i][j]);
	  }
	  free(m->tiles[i]);
     }
     free(m->tiles);
     free(m);
     return;
}

void free_tile(Tile * t)
{
     if (!t) {
	  return;
     }
     if (t->issectorowner){
	  free_sector(t->sector);
     }
     free(t);
     return;
}

void free_sector(Sector * s)
{
     SectorNode * n, *aux;
     if (!s){
	  return;
     }
     n = s->node;
     while(n){
	  aux = n->next;
	  free(n);
	  n = aux;
     }
     free(s);
}

/* uncovers underlying sector of a given tile */
int64_t uncover_sector(Minefield_p m, int64_t x, int64_t y, int64_t player, int8_t (*retbuf)[4])
{
     int64_t c=0;
     Tile * t;
     Sector * s;
     SectorNode * sn;

     if (!m || x < 0 || x > m->cols || y < 0 || y > m->rows){
	  return -1;
     }
     t = m->tiles[x][y];
     if (t->ownerid >= 0) {
	  return -1;
     }
     if (t->ismine) {
	  retbuf[c][0] = x;
	  retbuf[c][1] = y;
	  retbuf[c][2] = 9;
	  retbuf[c][3] = player;
	  c++;
	  return c;
     }
     s = t->sector;
     if (!s) {
	  return 0;
     }

     sn = s->node;
     for (int i=0; i < s->size; i++){
	  t = sn->tile;
	  sn = sn->next;
	  if (t->ownerid < 0) {
	       t->ownerid = player;
	       retbuf[c][0] = t->x;
	       retbuf[c][1] = t->y;
	       retbuf[c][2] = t->nearby;
	       retbuf[c][3] = t->ownerid;
	       c++;
	  }
     }
     return c;
}

int64_t update_minefield(Minefield * m, int64_t x, int64_t y, int64_t player, UpdateStruct * us)
{
     int64_t count = 0, base_index;
     int8_t buf[BUF_SIZE][4];
     if (m == NULL || us == NULL || x < 0 || y < 0 || x >= m->cols || y >= m->rows) {
	  return -1;
     }
     base_index = us->len;
     count = uncover_sector(m, x, y, player,buf);
     for (int i=0; i < count; i++) {
	  us->tiles[base_index+i].x = buf[i][0];
	  us->tiles[base_index+i].y = buf[i][1];
	  us->tiles[base_index+i].nearby = buf[i][2];
	  us->tiles[base_index+i].player = buf[i][3];
     }
     us->len += count;
     return count;
}

Tile * uncover_tile(Minefield * m, uint64_t x, uint64_t y)
{
     Tile * t;
     if (m==NULL || x < 0 || y < 0 || x >= m->cols || y >= m->rows) {
	  return NULL;
     }
     t = m->tiles[x][y];
     if (t->ownerid == 0) {
	  t->ownerid = 1;
     }
     else {
	  t = NULL;
     }
     return t;
}

int64_t get_mine_buffer(Minefield_p m, int64_t ** mb)
{
     int k = 0;
     for (int i = 0; i < m->cols; i++) {
	  for (int j = 0; j < m->rows; j++) {
	       if (m->tiles[i][j]->ismine){
		    mb[i][j] = 9;
	       }
	       else {
		    mb[i][j] = m->tiles[i][j]->nearby;
	       }
	       k++;
	  }
     }
     return k;
}

void srv_exit(void)
{
     system("rm /tmp/mine_serv");
      system("rm /tmp/mq");
     exit(0);
}

/* DEBUG: tidy FIFO delete handler */
void sig_handler(int signo)
{
     if (signo == SIGINT){
	  srv_exit();
     }
     return;
}

typedef struct attender
{
     Connection * con;
     int w_fd, buf_size;
} Attender;

/* attendants threads run here */
void * attend(void * a)
{
     Attender * attr = (Attender *) a ;
     Connection * con = attr->con;
     int buf_size = attr->buf_size, w_fd = attr->w_fd, len;
     int8_t * buf = malloc(buf_size);

     free(a);
     /* expects blocking read */
     while ((len = mm_read(con, buf, buf_size)) > 0) {
	  write(w_fd, buf, len);
     }
     free(con);
     free(buf);
     pthread_exit(0);
}

typedef struct Informer
{
     Connection * con;
     int r_fd, buf_size;
} Informer;

/* informer threads run here */
void * inform(void * a)
{
     Informer * info = (Informer *) a;
     Connection * con = info->con;
     int r_fd = info->r_fd;
     int8_t buf[BUF_SIZE];
     int64_t len, buf_size = BUF_SIZE;

     /* expects blocking read */
     while((len = read(r_fd, buf, buf_size)) > 0) {
	  mm_write(con, buf, len);
     }
     pthread_exit(0);
}

typedef struct ClientPthreads
{
     pthread_t p_attr;
     pthread_t p_info;
     int r_fd;
     int w_fd;
} ClientPthreads;

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
     attender->con = con;
     informer->con = con;
     pipe(attr_fd);
     pipe(info_fd);
     attender->w_fd = attr_fd[1];
     informer->r_fd = info_fd[0];
     attender->buf_size = attr_buf_size;
     informer->buf_size = info_buf_size;
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


int main(void)
{
     Listener_p lp;
     char * srv_addr;
     char * srv_addr_mq;
     Connection * c = NULL;
     int64_t rows = ROWS, cols = COLS, mines = MINES;
     Minefield * minef;
     char fifo[20];
     int sflag, q = 0, attr_nfds = 0, info_nfds = 0, max_size = 1000;
     int64_t cli_i = 0, us_size;
     fd_set r_set, w_set;
     struct timeval timeout;
     ClientPthreads * pths[10];
     int count = 0;
     QueryStruct qs;
     UpdateStruct  * us;
     InitStruct is;
     us_size = cols * rows * 4 + sizeof(int64_t);
     us = malloc(us_size);
     us->len = 0;

     int8_t u_flag = 0;

     timeout.tv_sec = 10;
     timeout.tv_usec = 0;

     signal(SIGINT,sig_handler);
     system("rm /tmp/mine_serv");
     system("rm /tmp/mq");
     mq_unlink("/mq");
     /* setting fifo path */
     sprintf(fifo, "/tmp/mine_serv");

     srv_addr = fifo;
     srv_addr_mq ="/tmp/mq";
     lp = mm_listen(srv_addr_mq);
     system("gnome-terminal -x ./mq.out");


     while ( (c = mm_accept(lp)) == NULL) ;
       char msg[100]="";
       while(strcmp(msg,"got_connected") != 0){
       mm_read(c,msg,strlen("got_connected")+1);
     }
 mqd_t mqd = mq_open("/mq",O_WRONLY);



     /* open connection */
     lp = mm_listen(srv_addr);

     /* wait for connections */
     while ( count < 2 && (c = mm_accept(lp)) != NULL) {



	  /* established conection on c, needs to create thread */
	  printf("conexion establecida. Creando thread.\n");
	  add_client(pths, &r_set, &w_set, &cli_i, &attr_nfds, &info_nfds, c, sizeof(QueryStruct), sizeof(int64_t) + 4 * rows * cols);
	  printf("thread creado..\n");
   	  mq_send(mqd,"GOT A CONNECTION",strlen("GOT A CONNECTION")+1,NORMAL_PR);
	  count++;

     }
     if (count < 2) {
	  /* connections failed */
	  srv_exit();
     }
     /* create minefield */
     minef = create_minefield(cols, rows, mines);
     is.cols = cols;
     is.rows = rows;
     is.mines = mines;
     /* send game info to clients */
     for (int i = 0; i < cli_i; i++) {
	  write(pths[i]->w_fd, (char *) &is, sizeof(InitStruct));
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
	       srv_exit();
	  }
	  else if (sflag > 0){
	       for(int i = 0; i < cli_i; i++) {
		    if (FD_ISSET(pths[i]->r_fd, &r_set)) {
			 /* read fd */
			 if (read(pths[i]->r_fd,(char *) &qs,max_size) > 0) {

			 /* update_minefield(Minefield * m, QueryStruct * qs) */
			 //	 printf("%s\n", buf);
			      sprintf(msg,"x: %d y: %d player:%d",(int)qs.x,(int)qs.y, (int) i);
                              	mq_send(mqd,msg,strlen(msg)+1,NORMAL_PR);
			      update_minefield(minef, qs.x, qs.y, i, us);
			      u_flag = 1;
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
	       if (u_flag){
		    /*UpdateStruct us = fetch_updates(m);*/
	       //   int64_t u_struct_s = sizeof(m->u_struct);
		    printf("%d\n",us->len);

		    for (int i = 0; i < us->len; i++) {
			 sprintf(msg,"unveieled x:%d y:%d n:%d player:%d ", us->tiles[i].x, us->tiles[i].y, us->tiles[i].nearby, us->tiles[i].player);
                          	mq_send(mqd,msg,strlen(msg)+1,NORMAL_PR);
		    }

		    for (int i = 0; i < cli_i; i++) {
			 write(pths[i]->w_fd, (char *) us, sizeof(int64_t) + sizeof(int8_t) * 4 * us->len);
		    }
		    us->len = 0;
		    u_flag = 0;

	       }
	  }
     }


     printf("player initialization request \n");
     srv_exit();
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

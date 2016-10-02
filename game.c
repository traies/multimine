#include <stdint.h>
#include <stdbool.h>
#include <stdio.h> //TODO remove
#include <queue.h>
#include <game.h>
#include <msg_structs.h>
#include <stdlib.h>
#include <time.h>
#include <common.h>

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
     int8_t endflag;
     int64_t winner_id;
     int64_t cols, rows;
     int64_t players;
     int64_t players_left;
     int64_t player_scores[7][2];
     int64_t player_ids[7];
     int64_t unk_tiles;
     Tile *** tiles;
} Minefield;

static int8_t construct_sectors(Minefield * m);
static Tile * uncover_tile(Minefield * minefield, uint64_t x, uint64_t y);
static int8_t solo_sector(Tile * t);
static int8_t multi_sector(Minefield * m, int64_t x, int64_t y);
static int8_t add_tile(Sector * s, Tile * t);
void free_minefield(Minefield * m);
static void free_tile(Tile * t);
static void free_sector(Sector * s);
static Tile * uncover_tile(Minefield * minefield, uint64_t x, uint64_t y);


Minefield * create_minefield(int64_t cols, int64_t rows, int64_t mines, int64_t players)
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
     minefield->players = players;
     minefield->players_left = players;
     minefield->endflag = false;
     minefield->winner_id = -1;

     for (int i = 0; i < players; i++) {
	  minefield->player_scores[i][0] = i;
	  minefield->player_scores[i][1] = 0;
	  minefield->player_ids[i] = i;
     }
     minefield->unk_tiles = cols * rows - mines;

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
		    minefield->tiles[c][r]->visited = false;
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
	  tile->ismine = true;
	  tile->nearby = 9;
	  maxkx = min(x+1,cols-1);
	  maxky = min(y+1,rows-1);
	  for (int kx = max(x-1,0); kx <= maxkx;kx++){
	       for (int ky = max(y-1, 0); ky <= maxky; ky++) {
		    if (!minefield->tiles[kx][ky]->ismine){
			 (minefield->tiles[kx][ky]->nearby)++;
		    }
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
static int8_t construct_sectors(Minefield * m)
{
     Tile * t;
     int8_t malloc_error = false;

     if (!m){
	  return -1;
     }
     for (int i = 0; !malloc_error && i < m->cols; i++) {
	  for (int j = 0; j < m->rows && !malloc_error; j++) {
	       t = m->tiles[i][j];
	       if (t->sector){
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
static int8_t multi_sector(Minefield * m, int64_t x, int64_t y)
{
     int8_t malloc_error = false;
     int64_t maxx, maxy, vc = 0;
     Queue_p q = new_queue();
     Tile * t;
     Sector *s = calloc(1,sizeof(Sector));
     int64_t (*vis)[2];
     vis = malloc(sizeof(int64_t[2]) * m->rows * m->cols);
     if (!q || !s || !vis){
	  return -1;
     }
     m->tiles[x][y]->issectorowner = true;
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
	  t->visited = true;
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
		    if ((i == t->x && j == t->y) || m->tiles[i][j]->visited == true){
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
	  m->tiles[vis[c][0]][vis[c][1]]->visited = false;
     }
     free(vis);
     free(q);
     return malloc_error;
}

/* solo tile sector initialization */
static int8_t solo_sector(Tile * t)
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
     t->issectorowner = true;
     return 0;
}

/* add tile to sector */
static int8_t add_tile(Sector * s, Tile * t)
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
void free_minefield(Minefield * m)
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

static void free_tile(Tile * t)
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

static void free_sector(Sector * s)
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
static int64_t uncover_sector(Minefield * m, int64_t x, int64_t y, int64_t player, int8_t (*retbuf)[4])
{
     int64_t c=0;
     Tile * t;
     Sector * s;
     SectorNode * sn;

     if (!m || x < 0 || x > m->cols || y < 0 || y > m->rows){
	  return 0;
     }
     t = m->tiles[x][y];

     if (t->ownerid >= 0 || m->player_scores[m->player_ids[player]][1] < 0) {
	  return 0;
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

/* update minefield state */
int64_t update_minefield(Minefield * m, int64_t x, int64_t y, int64_t player, UpdateStruct * us)
{
     int64_t count = 0, base_index;
     static int8_t (*buf)[4];
     if (m == NULL || us == NULL || m->player_scores[player] < 0 || x < 0 || y < 0 || x >= m->cols || y >= m->rows) {
	  return -1;
     }

     if (buf == NULL){
	  buf = malloc(sizeof(int8_t[4]) * m->rows * m->cols);
	  if (buf == NULL) {
	       return -1;
	  }
     }
     base_index = us->len;
     count = uncover_sector(m, x, y, player,buf);
     if (count == 1 && buf[0][2] == 9) {
	  m->player_scores[m->player_ids[player]][1] = -1;
	  m->players_left--;
     }
     else {
	  m->player_scores[m->player_ids[player]][1] += count;
	  m->unk_tiles -= count;
     }
     for (int i=0; i < count; i++) {
	  us->tiles[base_index+i].x = buf[i][0];
	  us->tiles[base_index+i].y = buf[i][1];
	  us->tiles[base_index+i].nearby = buf[i][2];
	  us->tiles[base_index+i].player = buf[i][3];
     }
     us->len += count;
     return count;
}

static void update_scores_to_ids(int64_t *player_ids, int64_t (*player_scores)[2],  int64_t players)
{
     for (int i = 0; i < players; i++) {
	  player_ids[player_scores[i][0]] = i;
     }
}

static void sort_scores(int64_t (* player_scores)[2], int64_t players)
{
     int64_t aux0, aux1;
     int j;
     /* insertion sort */
     for(int i = 1; i < players; i++) {
	  aux0 = player_scores[i][0];
	  aux1 = player_scores[i][1];
	  j = i - 1;
	  while ( j >= 0 && player_scores[j][1] < aux1) {
	       player_scores[j+1][0] = player_scores[j][0];
	       player_scores[j+1][1] = player_scores[j][1];
	       j = j - 1;
	  }
	  player_scores[j+1][0] = aux0;
	  player_scores[j+1][1] = aux1;
     }
     return;
}

int8_t check_win_state(Minefield * m, EndGameStruct * es)
{
     int64_t (*pscores)[2], * pids,players, pleft, utiles;
     if (!m) {
	  return true;
     }
     pscores = m->player_scores;
     pids = m->player_ids;
     players = m->players;
     pleft = m->players_left;
     utiles = m->unk_tiles;
     if (utiles <= 0 || pleft == 0) {
	  return true;
     }
     if (players > 1) {
	  sort_scores(pscores, players);
	  update_scores_to_ids(pids, pscores, players);
	  if (pscores[0][1] > utiles + pscores[1][1] || pleft == 1) {
	       es->winner_id = pscores[0][0];
	       es->players = get_scores(m, es->player_scores);
	       return true;
	  }
     }
     return false;
}

int8_t get_scores(Minefield * m, int64_t player_scores[8])
{
     if (!m) {
	  return -1;
     }
     for(int i=0; i < m->players; i++) {
	  player_scores[i] = m->player_scores[m->player_ids[i]][1];
     }
     return m->players;
}

int8_t reset_score(Minefield * m, int64_t playerid)
{
     if (!m) {
	  return -1;
     }
     m->player_scores[m->player_ids[playerid]][0] = -1;
     return 0;
}

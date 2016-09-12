#include <server.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <queue.h>
#define min(a,b)    ((a < b)? a:b)
#define max(a,b)    ((a < b)? b:a)
#define clamp(a, b, c) max(min(a,c),b)
#define TRUE 1
#define FALSE 0

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
		    minefield->tiles[c][r]->ownerid = 0;
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
int64_t uncover_sector(Minefield_p m, int64_t x, int64_t y, int64_t (*retbuf)[3])
{
     int64_t c=0;
     Tile * t;
     Sector * s;
     SectorNode * sn;
     
     if (!m || x < 0 || x > m->cols || y < 0 || y > m->rows){
	  return -1;
     }
     t = m->tiles[x][y];
     if (t->ismine) {
	  retbuf[c][0] = x;
	  retbuf[c][1] = y;
	  retbuf[c][2] = 9;
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
	  if (t->ownerid == 0) {
	       t->ownerid = 1;
	       retbuf[c][0] = t->x;
	       retbuf[c][1] = t->y;
	       retbuf[c][2] = t->nearby;
	       c++;
	  }
     }
     return c;
}

Tile * uncover_tile(Minefield * minefield, uint64_t x, uint64_t y) 
{
     Tile * t = minefield->tiles[x][y];
     
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

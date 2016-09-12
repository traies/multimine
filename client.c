#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define min(a,b)    ((a < b)? a:b)
#define max(a,b)    ((a < b)? b:a)
#define clamp(a, b, c) max(min(a,c),b)

struct Sector;
struct Q_node;
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

typedef struct
{
     int64_t cols, rows;
     Tile *** tiles;
} Minefield;

typedef struct Q_node
{
     Tile * tile;
     struct Q_node * next;
} Q_node;

typedef struct
{
     Q_node * first;
     Q_node * last;
} Queue;

int8_t construct_sectors(Minefield * m);
void free_minefield(Minefield * m);


int64_t enque(Queue * q, Tile * t) 
{
     Q_node * node = malloc(sizeof(Q_node));
     if (!q || !t){
	  return -1;
     }
     
     if (!node) {
	  return -1;
     }
     node->next = NULL;
     node->tile = t;
     if (!q->first) {
	  q->first = node;
	  q->last = node;
     }
     else{
	  q->last->next = node;
	  q->last = node;
     }
     return 0;
}

Tile * deque(Queue * q) 
{
     Q_node * node;
     Tile * t;
     
     if (!q || !q->first){
	  return NULL;
     }
     node = q->first;
     if (node == q->last) {
	  q->first = NULL;
	  q->last = NULL;
     }
     else {
	  q->first = q->first->next;
     }
     t = node->tile;
     free(node);
     return t;
}

int8_t peek(Queue * q)
{
     return q->first != NULL;
}

Minefield * create_minefield(int64_t cols, int64_t rows, int64_t mines)
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

     
/* creates ncurses window */
WINDOW * create_window(uint64_t h, uint64_t w, uint64_t sy, uint64_t sx)
{
     WINDOW * win;
     win = newwin(h, w, sy, sx);
     wattrset(win, COLOR_PAIR(1));
     box(win, 0, 0);
     wmove(win,1,1);
     wrefresh(win);
     return win;
}

/* draws current minefield */
void draw_minefield(WINDOW * win, Minefield * mine) 
{
     Tile * t;
     
     for (int i = 0; i < mine->cols; i++) {
	  for (int j = 0; j < mine->rows; j++){
	       t = mine->tiles[i][j];
	       if (t->ismine){
		    wattron(win,COLOR_PAIR(2));
		    mvwaddch(win,j+1,i+1,'*');
		    wattroff(win,COLOR_PAIR(2));
	       }
	       else if (t->nearby > 0) {
		    mvwaddch(win,j+1, i+1,(int8_t)('0' + t->nearby));
	       }
	  }
     }
     return;
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

void draw_tile(WINDOW * win, Tile * t)
{
     if (t->ismine) {
	  wattron(win,COLOR_PAIR(2));
	  mvwaddch(win,t->y+1,t->x+1,'*');
	  wattroff(win,COLOR_PAIR(2));
     }
     else if (t->nearby > 0) {
	  mvwaddch(win,t->y+1,t->x+1,(int8_t)('0' + t->nearby));
	  
     }
     else {
	  mvwaddch(win,t->y+1,t->x+1,'0');
	  
     }
     return;
}

void draw_tile_alt(WINDOW * win, int64_t y, int64_t x, int64_t nearby) 
{
     if (nearby < 0) {
	  wattron(win,COLOR_PAIR(2));
	  mvwaddch(win,y+1,x+1,'*');
	  wattroff(win,COLOR_PAIR(2));
     }
     else if (nearby > 0) {
	  mvwaddch(win,y+1,x+1,(int8_t)('0' + nearby));
	  
     }
     else {
	  mvwaddch(win,y+1,x+1,'-');
	  
     }
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
     
int8_t multi_sector(Minefield * m, int64_t x, int64_t y)
{
     int8_t malloc_error = FALSE;
     int64_t maxx, maxy, vc = 0;
     Queue * q = calloc(1,sizeof(Queue));
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

int64_t uncover_sector(Minefield * m, int64_t x, int64_t y, int64_t (*retbuf)[3])
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
	  retbuf[c][2] = -1;
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
     
int main() 
{
     
     int64_t c, x, y, win_h, win_w, cols, rows, mines, count;
     int64_t (*sector_tiles)[3];
     WINDOW * win, * win2;
     Minefield * minefield;
     Tile * tile;
     
     cols = 50;
     rows = 10;
     mines = 25;
     
     minefield = create_minefield( cols - 2, rows - 2, mines);
     
     initscr(); /* ncurses init */
     if (has_colors() == FALSE) {
	  endwin();
	  printf("no color");
	  return 0;
     }
     /* color initialization */
     start_color();
     init_pair(1, COLOR_WHITE, COLOR_BLACK);
     init_pair(2, COLOR_RED, COLOR_BLACK);
     init_pair(3, COLOR_BLUE, COLOR_BLACK);
     init_pair(4, COLOR_BLACK, COLOR_WHITE);
     
     x = y = 1;
     win_h = rows;
     win_w = cols;
     cbreak(); /* disable char buffering */
     keypad(stdscr, TRUE); /* enable keypad, arrow keys */
     noecho(); /* disable echoing typed chars*/
     //curs_set(0); /* set cursor to invisible */
     refresh();
     
     win = create_window(win_h, win_w, (LINES - win_h) / 2 - 5, (COLS - win_w) / 2);
     draw_minefield(win,minefield);
     wrefresh(win);

     win2 = create_window(win_h, win_w, (LINES - win_h) / 2 + 5, (COLS - win_w) / 2);
     wrefresh(win2);

     sector_tiles = malloc(sizeof(int64_t[3]) * minefield->rows * minefield->cols);
     if (!sector_tiles){
	  return -1;
     }
     //wattrset(win, COLOR_PAIR(2));
     while((c=toupper(getch())) != 'Q') {
	  switch(c) {
	  case '\n':
	       
	       count = 0;
	       count = uncover_sector(minefield, x-1, y-1, sector_tiles);
	       if (sector_tiles != NULL) {
		    for(int i = 0; i < count; i++){
			 draw_tile_alt(win2, sector_tiles[i][1],sector_tiles[i][0],sector_tiles[i][2]);
		    }
		    wmove(win2,y,x);
	       }
	       break;
	  case KEY_UP:
	       y = max(1,y-1);
	       wmove(win2,y,x);
	       break;
	  case KEY_DOWN:
	       /* move cursor down */
	       y = min(win_h - 2, y + 1);
	       wmove(win2,y,x);
	       break;
	  case KEY_RIGHT:
	       /* move cursor right */
	       x = min(win_w - 2, x + 1);
	       wmove(win2,y,x);
	       break;
	  case KEY_LEFT:
	       /* move cursor left */
	       x = max(1, x - 1);
	       wmove(win2,y,x);
	       break;
	  case KEY_RESIZE:
	       endwin();
	       clear();
	       refresh();
	       win = create_window(win_h, win_w, (LINES - win_h) / 2 - 5, (COLS - win_w) / 2);
	       draw_minefield(win,minefield);
	       wrefresh(win);

	       win2 = create_window(win_h, win_w, (LINES - win_h) / 2 + 5, (COLS - win_w) / 2);
	       wrefresh(win2);
	       wattrset(win2, COLOR_PAIR(4));
	       break;
	  default:
	       break;

	  }
	  wrefresh(win2);
     }
     endwin(); /* reset terminal */
     return 0;
}


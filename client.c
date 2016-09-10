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

typedef struct 
{

     int64_t x, y;
     struct Sector * sector;
     int64_t ownerid;
     int64_t nearby;
     int8_t ismine;
} Tile;

typedef struct
{
     const int8_t name[10];
     int64_t color;
     int64_t id;
} Player;

typedef struct
{
     int64_t size;
     Tile *** tiles;
} Sector;

typedef struct
{
     int64_t cols, rows;
     Tile *** tiles;
} Minefield;

Minefield * create_minefield(int64_t cols, int64_t rows, int64_t mines)
{
     int64_t size = cols * rows;
     int64_t * rand_arr, * mine_arr;
     int64_t aux, x, y, c = 0, r = 0, maxkx, maxky;
     Minefield * minefield;
     void * t;
     Tile * tile;
     
     
     /* TODO: check mallocs */
     rand_arr = malloc(sizeof(int64_t) * size);
     mine_arr = malloc(sizeof(int64_t) * mines);
     minefield = malloc(sizeof(Minefield));
     minefield->cols = cols;
     minefield->rows = rows;
     minefield->tiles = malloc(sizeof(Tile **) * cols);
     
     do {
	  minefield->tiles[c] = malloc(sizeof(Tile*) * rows);
	  do {
	       minefield->tiles[c][r] = calloc(1, sizeof(Tile));
	  } while (++r < rows);
	  r = 0;
     }while(++c < cols);
     /*
     if (!t){
	  // fallo el malloc 
	  return 0;
     }*/
     
     for (int i = 0; i < size; i++){
	  rand_arr[i] = i;
     }
     srand(time(0));

     for (int j = 0; j < mines ; j++) {
	  aux = rand() % (size - j);
	  mine_arr[j] = rand_arr[aux];
	  rand_arr[aux] = rand_arr[size - j - 1];
     }

     for (int k = 0; k < mines ; k++) {
	  x = mine_arr[k] % cols;
	  y = mine_arr[k] / cols;
	  tile = minefield->tiles[x][y];
	  tile->x = x;
	  tile->y = y;
	  tile->ismine = TRUE;
	  maxkx = min(x+1,cols-1);
	  maxky = min(y+1,rows-1);
	  for (int kx = max(x-1,0); kx <= maxkx;kx++){
	       for (int ky = max(y-1, 0); ky <= maxky; ky++) {
		    (minefield->tiles[kx][ky]->nearby)++;
	       }
	  }
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

void draw_minefield(WINDOW * win, Minefield * mine) 
{
     Tile * t;
     
     for (int i = 0; i < mine->cols; i++) {
	  for (int j = 0; j < mine->rows; j++){
	       t = mine->tiles[i][j];
	       if (t->ismine){
		    wattron(win,COLOR_PAIR(2));
		    mvwaddch(win,j+1,i+1,'O');
		    wattroff(win,COLOR_PAIR(2));
	       }
	       else if (t->nearby > 0) {
		    mvwaddch(win,j+1, i+1,(int8_t)('0' + t->nearby));
	       }
	  }
     }
     return;
}



int main() 
{
     int64_t c;
     int64_t x, y, win_h, win_w, cols, rows, mines;
     WINDOW * win;
     Minefield * minefield;

     cols = 50;
     rows = 10;
     mines = 25;
     
     minefield = create_minefield( cols - 2, rows - 2, mines);
     
     initscr(); /* ncurses init */
     if (has_colors() == FALSE) {
	  printw("no color");
	  getch();
	  return 0;
     }
     start_color();
     init_pair(1, COLOR_WHITE, COLOR_BLACK);
     init_pair(2, COLOR_RED, COLOR_BLACK);
     init_pair(3, COLOR_BLUE, COLOR_BLACK);
     
     x = y = 1;
     win_h = 10;
     win_w = 50;
     cbreak(); /* disable char buffering */
     keypad(stdscr, TRUE); /* enable keypad, arrow keys */
     noecho(); /* disable echoing typed chars*/
     //curs_set(0); /* set cursor to invisible */
     refresh();
     
     win = create_window(win_h, win_w, (LINES - win_h) / 2, (COLS - win_w) / 2);
     draw_minefield(win,minefield);
     wrefresh(win);
     
     //wattrset(win, COLOR_PAIR(2));
     while((c=toupper(getch())) != 'Q') {
	  switch(c) {
	  case KEY_UP:
	       y = max(1,y-1);
	       wmove(win,y,x);
	       break;
	  case KEY_DOWN:
	       /* move cursor down */
	       y = min(win_h - 2, y + 1);
	       wmove(win,y,x);
	       break;
	  case KEY_RIGHT:
	       /* move cursor right */
	       x = min(win_w - 2, x + 1);
	       wmove(win,y,x);
	       break;
	  case KEY_LEFT:
	       /* move cursor left */
	       x = max(1, x - 1);
	       wmove(win,y,x);
	       break;
	  default:
	       break;

	  }
	  wrefresh(win);
     }
     endwin(); /* reset terminal */
     return 0;
}


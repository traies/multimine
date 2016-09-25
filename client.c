#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <queue.h>
#include <signal.h>
#include <marsh.h>
#include <comms.h>
#include <unistd.h>

#define min(a,b)    ((a < b)? a:b)
#define max(a,b)    ((a < b)? b:a)
#define clamp(a, b, c) max(min(a,c),b)

void draw_tile(WINDOW * win, int64_t y, int64_t x, int64_t nearby, int64_t cp1, int64_t cp2);

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
void draw_minefield(WINDOW * win, int64_t ** buf, int64_t cols, int64_t rows, int64_t cp1, int64_t cp2)
{
     for (int i = 0; i < cols; i++){
	  for (int j = 0; j < rows; j++){
	       draw_tile(win, j, i, buf[i][j], cp1, cp2);
	  }
     }
     return;
}

void draw_tile(WINDOW * win, int64_t y, int64_t x, int64_t nearby, int64_t cp1, int64_t cp2)
{
     if (nearby == 10) {
	  wattron(win,COLOR_PAIR(cp1));
	  mvwaddch(win,y+1,x+1,'#');
	  wattroff(win,COLOR_PAIR(cp1));
     }

     if (nearby == 9) {
	  wattron(win,COLOR_PAIR(cp2));
	  mvwaddch(win,y+1,x+1,'*');
	  wattroff(win,COLOR_PAIR(cp2));
     }
     else if (nearby > 0 && nearby < 9) {
	  wattron(win,COLOR_PAIR(cp1));
	  mvwaddch(win,y+1,x+1,(int8_t)('0' + nearby));
	  wattron(win,COLOR_PAIR(cp1));
     }
     else if (nearby == 0){
	  wattron(win,COLOR_PAIR(cp1));
	  mvwaddch(win,y+1,x+1,' ');
	  wattron(win,COLOR_PAIR(cp1));
     }
     return;
}
void update_marks(WINDOW * win, int64_t marks)
{
     wmove(win,2,1);
     wprintw(win, "              ");
     wmove(win,2,1);
     wprintw(win, "marks: %d", marks);
     wrefresh(win);
}

void update_utiles(WINDOW * win, int64_t utiles)
{
     wmove(win,3,1);
     wprintw(win, "                  ");
     wmove(win,3,1);
     wprintw(win, "tiles left: %d", utiles);
     wrefresh(win);
}

void update_mines(WINDOW * win, int64_t mines)
{
     wmove(win,1,1);
     wprintw(win, "              ");
     wmove(win,1,1);
     wprintw(win, "mines: %d", mines);
     wrefresh(win);
}


char fin[30], fout[30];
void cli_exit() {
     char buf[50];
     printf("algo.\n");
     sprintf(buf, "rm %s", fin);
     system(buf);
     sprintf(buf, "rm %s", fout);
     system(buf);
     endwin(); /* reset terminal */
     exit(0);
     return;
}

/* DEBUG: tidy FIFO delete handler */
void sig_handler(int signo)
{
     if (signo == SIGINT) {
	  cli_exit();
     }
     return;
}



int main()
{
     Address srv_addr;
     int64_t rows, cols, mines, us_size;
     QueryStruct qs;
     UpdateStruct  * us;
     InitStruct is;
     Connection * con;

     /* setting fifo path */
     sprintf(fin, "/tmp/r%d", getpid());
     sprintf(fout, "/tmp/w%d", getpid());

     signal(SIGINT, sig_handler);

     /* setting up communications */
     srv_addr.fifo = "/tmp/mine_serv";
     con = mm_connect(&srv_addr);
     if (!con) {
	  printf("no se pudo subscribir.\n");
	  cli_exit();
	  return 0;
     }
     printf("suscrito.\n");
     if ( mm_read(con, (char *) &is, sizeof(InitStruct)) > 0) {
	  printf("cols: %d rows: %d mines: %d \n", (int) is.cols, (int) is.rows, (int) is.mines);
     }
     else {
	  printf("no se inicio.\n");
     }
     int len, max_size = 100;

     int64_t c, x, y, win_h, win_w,  mb_size_1 = 0, mb_size_2 = 0, count = 0, auxi = 0, auxj = 0, marks = 0;
     int8_t auxx, auxy, auxn;
     int64_t utiles = 0;
     int8_t win_flag = FALSE, loose_flag = FALSE;

     int64_t ** mine_buffer = NULL, (*mine_buffer_aux)[3] = NULL;
     WINDOW * win, * win_side;

     cols = is.cols;
     rows = is.rows;
     mines= is.mines;
     us_size = sizeof(int64_t) + sizeof(int8_t) * cols * rows * 3;
     us = malloc(us_size);
     mine_buffer = malloc(sizeof(int64_t *) * (cols));
     mine_buffer_aux = malloc(sizeof(int64_t[3]) * (rows) * (cols));
     utiles = (cols) * (rows) - mines;
     if (!us || !mine_buffer || !mine_buffer_aux){
	  return -1;
     }

     do{
	  mine_buffer[auxi] = malloc(sizeof(int64_t) * (rows));
     } while (mine_buffer[auxi] && auxi++ < (cols));

     for (int i = 0; i < (cols); i++) {
	  for (int j = 0; j < (rows); j++) {
	       mine_buffer[i][j] = -1;
	  }
     }
     /* ncurses init */
     initscr();

     if (has_colors() == FALSE) {
	  endwin();
	  printf("no color");
	  return 0;
     }
     /* color initialization */

     start_color();
     init_pair(1, COLOR_WHITE, COLOR_BLACK);
     init_pair(2, COLOR_RED, COLOR_BLACK);
     init_pair(3, COLOR_BLACK, COLOR_WHITE);
     init_pair(4, COLOR_RED, COLOR_WHITE);



     win_h = rows + 2;
     win_w = cols + 2;
     x = cols / 2 + 1;
     y = rows / 2 + 1;
     /* disable char buffering */
     cbreak();
     /* enable keypad, arrow keys */
     keypad(stdscr, TRUE);
     /* disable echoing typed chars */
     noecho();
     /* set cursor to invisible */
     //curs_set(0);
     refresh();

     win = create_window(win_h, win_w, (LINES - win_h) / 2, (COLS - win_w) / 2);
     draw_minefield(win, mine_buffer, cols, rows, 1,2);
     wrefresh(win);

     win_side = create_window(win_h, win_w / 2, (LINES - win_h) / 2, (COLS - win_w) / 2 + win_w);
     update_mines(win_side, mines);
     update_utiles(win_side, utiles);
     update_marks(win_side, marks);
     wmove(win,y,x);
     wrefresh(win);

     //wattrset(win, COLOR_PAIR(2));
     while(!win_flag && !loose_flag && (c=toupper(getch())) != 'Q') {
	  switch(c) {
	  case 'X':
	       if (mine_buffer[x-1][y-1] == 10){
		    break;
	       }
	       qs.x = x-1;
	       qs.y = y-1;
	       mm_write(con, (char *) &qs, sizeof(QueryStruct));
	       mm_read(con, (char *) us, us_size);

	       count = us->len;
	       update_marks(win_side,count);
	       wmove(win,y,x);
	       if (count > 0) {
		    for(int i = 0; i < count; i++){
			 auxx = us->tiles[i].x;
			 auxy = us->tiles[i].y;
			 auxn = us->tiles[i].nearby;

			 if (mine_buffer[auxx][auxy] == 10) {
			      marks--;
			      update_marks(win_side,marks);
			      wmove(win,y,x);
			 }
			 if (auxn == 9) {
			      loose_flag = TRUE;
			 }

			 mine_buffer[auxx][auxy] = auxn;
			 draw_tile(win, auxy,auxx,auxn, 3,4);
		    }
		    us->len = 0;
		    wmove(win,y,x);
		    utiles-=count;
		    update_utiles(win_side, utiles);
		    if (utiles <= 0) {
			 win_flag = TRUE;
		    }
	       }
	       break;
	  case ' ':
	       if (mine_buffer[x-1][y-1] < 0 || mine_buffer[x-1][y-1] == 10) {
		    if (mine_buffer[x-1][y-1] < 0) {
			 mine_buffer[x-1][y-1] = 10;
			 draw_tile(win, y-1, x-1, 10, 3,4);
			 marks++;
		    }
		    else {
			 mine_buffer[x-1][y-1] = -1;
			 draw_tile(win, y-1, x-1, 0, 1,2);
			 marks--;
		    }
		    update_marks(win_side, marks);
		    wmove(win,y,x);
	       }
	       break;
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
	  case KEY_RESIZE:
	       endwin();
	       clear();
	       refresh();
	       win = create_window(win_h, win_w, (LINES - win_h) / 2 , (COLS - win_w) / 2);
	       draw_minefield(win,mine_buffer,cols,rows, 1, 2);
	       wrefresh(win);

	       win_side = create_window(win_h, win_w, (LINES - win_h) / 2, (COLS - win_w) / 2 + win_w);
	       update_mines(win_side, mines);
	       update_utiles(win_side, utiles);
	       update_marks(win_side, marks);
	       wrefresh(win_side);
	       wmove(win,y,x);
	       wrefresh(win);
	       break;
	  default:
	       break;

	  }
	  wrefresh(win);
     }
     if (win_flag) {
	  wmove(win_side, 5, 1);
	  wprintw(win_side, "YOU WIN!");
	  wrefresh(win_side);
	  getch();
     }
     else if (loose_flag) {
	  wmove(win_side, 5, 1);
	  wprintw(win_side, "YOU LOOSE!");
	  wrefresh(win_side);
	  getch();
     }

     cli_exit();
     return 0;
}

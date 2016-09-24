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
     /*
     MsgH_p msgh;
     int64_t connections[10], mid;
     char buf[50];
     */
     Address srv_addr;
     char buf[200], buf2[200];
     char srv_info[200];
     /* setting fifo path */

     sprintf(fin, "/tmp/r%d", getpid());
     sprintf(fout, "/tmp/w%d", getpid());

     signal(SIGINT, sig_handler);

     /* setup communications */
     srv_addr.fifo = "/tmp/mine_serv";
     Connection * c = mm_connect(&srv_addr);
     if (!c) {
	  printf("no se pudo subscribir.\n");
	  cli_exit();
	  return 0;
     }
     printf("suscrito.\n");
     int len, max_size = 100;
     while(1){
       scanf("%s", buf);
  +	  sprintf(buf2, "%s\n", buf);
  +	  mm_write(c, buf2, strlen(buf2)+1);


	  if ((len = mm_read(c, srv_info, max_size)) > 0) {
	       printf("%s lei %d\n",srv_info,len);
	  }
     }


/*
     connections[0] = conn(msgh, "tmp/mine_serv", fin, fout, 10, 0, 5);
     if (connections[0] < 0) {
	  printf("fallo la conexion\n");
	  cli_exit();
	  return 0;
     }
     printf("se establecio conexion. \n");
     getchar();
     */
     /* request player signup */
     /*
     mid = send(PLAYER_SIGNUP);
     if (!wait_ack(mid, 2)){
	  printf("PLAYER SIGNUP FAILED");
     }

     msg = wait_msg(INIT_GAME, timeout);
     if (msg != NULL) {
	  GameInfo g = (GameInfo) msg;
	  rows = g->rows;
	  cols = g->cols;
	  mines = g->mines;
     }
     */
     /* fifo clean up */
     cli_exit();
     /*
     int64_t c, x, y, win_h, win_w, cols, rows, mines, mb_size_1 = 0, mb_size_2 = 0, count = 0, auxi = 0, auxj = 0, marks = 0;
     int64_t auxx, auxy, auxn, utiles = 0;
     int8_t win_flag = FALSE, loose_flag = FALSE;

     int64_t ** mine_buffer_1 = NULL, ** mine_buffer_2 = NULL, (*mine_buffer_aux)[3] = NULL;
     WINDOW * win, * win2, *win3;
     Minefield_p minefield;

     cols = 50;
     rows = 10;
     mines = 25;
     mine_buffer_1 = malloc(sizeof(int64_t *) * (cols-2));
     mine_buffer_2 = malloc(sizeof(int64_t *) * (cols-2));
     mine_buffer_aux = malloc(sizeof(int64_t[3]) * (rows-2) * (cols-2));
     utiles = (cols - 2) * (rows - 2) - mines;
     if (!mine_buffer_1 || !mine_buffer_2 || !mine_buffer_aux){
	  return -1;
     }

     do{
	  mine_buffer_1[auxi] = malloc(sizeof(int64_t) * (rows-2));
	  mine_buffer_2[auxi] = malloc(sizeof(int64_t) * (rows-2));
     } while (mine_buffer_1[auxi] && mine_buffer_2[auxi] && auxi++ < (cols-2));

     for (int i = 0; i < (cols-2); i++) {
	  for (int j = 0; j < (rows-2); j++) {
	       mine_buffer_2[i][j] = -1;
	  }
     }

     minefield = create_minefield( cols - 2, rows - 2, mines);
     mb_size_1 = get_mine_buffer(minefield, mine_buffer_1);

     initscr(); /* ncurses init */
     /****
     if (has_colors() == FALSE) {
	  endwin();
	  printf("no color");
	  return 0;
     }
     /* color initialization */
     /*****
     start_color();
     init_pair(1, COLOR_WHITE, COLOR_BLACK);
     init_pair(2, COLOR_RED, COLOR_BLACK);
     init_pair(3, COLOR_BLACK, COLOR_WHITE);
     init_pair(4, COLOR_RED, COLOR_WHITE);


     x = y = 1;
     win_h = rows;
     win_w = cols;
     cbreak(); /* disable char buffering */
     /****
     keypad(stdscr, TRUE); /* enable keypad, arrow keys
     noecho(); /* disable echoing typed chars*/
     /****
     //curs_set(0); /* set cursor to invisible */
     /****
     refresh();

     win = create_window(win_h, win_w, (LINES - win_h) / 2 - 5, (COLS - win_w) / 2);
     draw_minefield(win, mine_buffer_1, cols-2, rows-2, 1,2);
     wrefresh(win);

     win2 = create_window(win_h, win_w, (LINES - win_h) / 2 + 5, (COLS - win_w) / 2);
     wattrset(win2, COLOR_PAIR(4));
     wrefresh(win2);

     win3 = create_window(win_h * 2, win_w / 2, (LINES - win_h) / 2 - 5, (COLS - win_w) / 2 + win_w);
     update_mines(win3, mines);
     update_utiles(win3, utiles);
     update_marks(win3, marks);
     wmove(win2,1,1);
     wrefresh(win2);

     //wattrset(win, COLOR_PAIR(2));
     while(!win_flag && !loose_flag && (c=toupper(getch())) != 'Q') {
	  switch(c) {
	  case '\n':
	       if (mine_buffer_2[x-1][y-1] == 10){
		    break;
	       }

	       count = 0;
	       count = uncover_sector(minefield, x-1, y-1, mine_buffer_aux);
	       if (count > 0) {
		    for(int i = 0; i < count; i++){
			 auxx = mine_buffer_aux[i][0];
			 auxy = mine_buffer_aux[i][1];
			 auxn = mine_buffer_aux[i][2];
			 if (mine_buffer_2[auxx][auxy] == 10) {
			      marks--;
			      update_marks(win3,marks);
			      wmove(win2,y,x);
			 }
			 if (auxn == 9) {
			      loose_flag = TRUE;
			 }

			 draw_tile(win2, auxy,auxx,auxn, 3,4);
			 mine_buffer_2[auxx][auxy] = auxn;
		    }
		    wmove(win2,y,x);
		    utiles-=count;
		    update_utiles(win3, utiles);
		    if (utiles <= 0) {
			 win_flag = TRUE;
		    }
	       }
	       break;
	  case ' ':
	       if (mine_buffer_2[x-1][y-1] < 0 || mine_buffer_2[x-1][y-1] == 10) {
		    if (mine_buffer_2[x-1][y-1] < 0) {
			 mine_buffer_2[x-1][y-1] = 10;
			 draw_tile(win2, y-1, x-1, 10, 3,4);
			 marks++;
		    }
		    else {
			 mine_buffer_2[x-1][y-1] = -1;
			 draw_tile(win2, y-1, x-1, 0, 1,2);
			 marks--;
		    }
		    update_marks(win3, marks);
		    wmove(win2,y,x);
	       }
	       break;
	  case KEY_UP:
	       y = max(1,y-1);
	       wmove(win2,y,x);
	       break;
	  case KEY_DOWN:
	       /* move cursor down *
	       y = min(win_h - 2, y + 1);
	       wmove(win2,y,x);
	       break;
	  case KEY_RIGHT:
	       /* move cursor right
	       x = min(win_w - 2, x + 1);
	       wmove(win2,y,x);
	       break;
	  case KEY_LEFT:
	       /* move cursor left
	       x = max(1, x - 1);
	       wmove(win2,y,x);
	       break;
	  case KEY_RESIZE:
	       endwin();
	       clear();
	       refresh();
	       win = create_window(win_h, win_w, (LINES - win_h) / 2 - 5, (COLS - win_w) / 2);
	       draw_minefield(win,mine_buffer_1,cols-2, rows-2, 1, 2);
	       wrefresh(win);

	       win2 = create_window(win_h, win_w, (LINES - win_h) / 2 + 5, (COLS - win_w) / 2);
	       wattrset(win2, COLOR_PAIR(4));
	       draw_minefield(win2,mine_buffer_2,cols-2, rows-2, 3, 4);
	       wrefresh(win2);

	       win3 = create_window(win_h * 2, win_w / 2, (LINES - win_h) / 2 - 5, (COLS - win_w) / 2 + win_w);
	       update_mines(win3, mines);
	       update_utiles(win3, utiles);
	       update_marks(win3, marks);
	       wrefresh(win3);
	       wmove(win2,y,x);
	       wrefresh(win2);
	       break;
	  default:
	       break;

	  }
	  wrefresh(win2);
     }
     if (win_flag) {
	  wmove(win3, 5, 1);
	  wprintw(win3, "YOU WIN!");
	  wrefresh(win3);
	  getch();
     }
     else if (loose_flag) {
	  wmove(win3, 5, 1);
	  wprintw(win3, "YOU LOOSE!");
	  wrefresh(win3);
	  getch();
     }

     endwin(); /* reset terminal */
     return 0;
}

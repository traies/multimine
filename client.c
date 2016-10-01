#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

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
#include <configurator.h>

#define min(a,b)    ((a < b)? a:b)
#define max(a,b)    ((a < b)? b:a)
#define clamp(a, b, c) max(min(a,c),b)

static void current_utc_time(struct timespec *ts);
void draw_tile(WINDOW * win, int64_t y, int64_t x, int64_t nearby, int64_t player);

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
void draw_minefield(WINDOW * win, int64_t (** buf)[2], int64_t cols, int64_t rows)
{
     for (int i = 0; i < cols; i++){
	  for (int j = 0; j < rows; j++){
	       draw_tile(win, j, i, buf[i][j][0], buf[i][j][1] + 3);
	  }
     }
     return;
}

void draw_tile(WINDOW * win, int64_t y, int64_t x, int64_t nearby, int64_t player)
{
     if (nearby == 10) {
	  wattron(win,COLOR_PAIR(player));
	  mvwaddch(win,y+1,x+1,'#');
	  wattroff(win,COLOR_PAIR(player));
     }

     if (nearby == 9) {
	  wattron(win,COLOR_PAIR(player));
	  mvwaddch(win,y+1,x+1,'*');
	  wattroff(win,COLOR_PAIR(player));
     }
     else if (nearby > 0 && nearby < 9) {
	  wattron(win,COLOR_PAIR(player));
	  mvwaddch(win,y+1,x+1,(int8_t)('0' + nearby));
	  wattroff(win,COLOR_PAIR(player));
     }
     else if (nearby == 0){
	  wattron(win,COLOR_PAIR(player));
	  mvwaddch(win,y+1,x+1,' ');
	  wattroff(win,COLOR_PAIR(player));
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

void update_scores(WINDOW * win, int64_t players,int64_t (*player_scores)[2], int64_t utiles)
{
     int base = 5;
     if (players == 1 ) {
	  wmove(win, base , 1);
	  wprintw(win, "                ");
	  wmove(win, base, 1);
	  wattron(win,COLOR_PAIR(player_scores[0][0] + 3));
	  wprintw(win, "player %d: %d", player_scores[0][0], player_scores[0][1]);
	  wattroff(win,COLOR_PAIR(player_scores[0][0] + 2));
     }
     else {
	  wmove(win, base , 1);
	  wprintw(win, "            ");
	  wmove(win, base , 1);
	  wprintw(win, "player %d: %d / %d", (int)player_scores[0][0], (int) player_scores[0][1],(int) player_scores[1][1] + utiles);

	  for (int i = 0; i < players; i++) {
	       wmove(win, base + i, 1);
	       wprintw(win, "            ");
	       wmove(win, base + i, 1);

	       if (player_scores[i][1] >= 0) {
		    wprintw(win, "player %d: %d", player_scores[i][0], player_scores[i][1]);
	       }
	       else {
		    wprintw(win, "player %d: LOST", player_scores[i][0]);
	       }

	  }
     }
     wrefresh(win);
}

char fin[30], fout[30];
void cli_exit() {
     char buf[50];
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

void time_diff(struct timespec * diff, struct timespec * init, struct timespec * end)
{
     diff->tv_sec = end->tv_sec - init->tv_sec;
     if ( end->tv_nsec - init->tv_nsec < 0) {
	  diff->tv_nsec = 1000000000 + end->tv_nsec - init->tv_nsec;
	  diff->tv_sec--;
     }
     else {
	  diff->tv_nsec = end->tv_nsec - init->tv_nsec;
     }
     return;
}

void update_scores_to_ids(int64_t *player_ids, int64_t (*player_scores)[2],  int64_t players)
{
     for (int i = 0; i < players; i++) {
	  player_ids[player_scores[i][0]] = i;
     }
}

void sort_scores(int64_t (* player_scores)[2], int64_t players)
{
     int64_t aux0, aux1;
     int j;
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

int8_t check_win_state(int64_t * pids, int64_t (* pscores)[2], int64_t players, int64_t utiles )
{
     int8_t win_flag;
     if (utiles <= 0) {
	  return TRUE;
     }
     if (players > 1) {
	  if (pscores[0][1] > utiles + pscores[1][1]) {
	       return TRUE;
	  }
     }
     return FALSE;
}

int main(int argc, char *argv[])
{
     char * srv_addr=configuration("config",mm_commtype(),0);
     int64_t rows, cols, mines, players, player_id, us_size;
     int64_t player_scores[8][2];
     int64_t player_ids[8];
     QueryStruct qs;
     UpdateStruct  * us;
     InitStruct is;
     Connection * con;

     /*
     if (argc <=1) {
       printf("no anduvo.\n");
       return 0;
     }
     else {
       printf("%s\n", argv[1]);
       srv_addr = argv[1];
     }
     */


     /* setting fifo path */
     sprintf(fin, "/tmp/r%d", getpid());
     sprintf(fout, "/tmp/w%d", getpid());

     signal(SIGINT, sig_handler);
     con = mm_connect(srv_addr);

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
     int8_t auxx, auxy, auxn, auxp;
     int64_t utiles = 0;
     int8_t win_flag = FALSE, loose_flag = FALSE, quit_flag = FALSE;
     struct timespec init_frame_time, end_frame_time, diff_frame_time;
     struct timeval select_timeout;
     int64_t (** mine_buffer)[2] = NULL, (*mine_buffer_aux)[3][2] = NULL;
     WINDOW * win, * win_side;

     cols = is.cols;
     rows = is.rows;
     mines= is.mines;
     players = is.players;
     player_id = is.player_id;

     us_size = sizeof(int64_t) + cols * rows * 4;
     us = malloc(us_size);
     mine_buffer = malloc(sizeof( int64_t * [2]) * (cols));
     mine_buffer_aux = malloc(sizeof(int64_t[3][2]) * (rows) * (cols));
     utiles = (cols) * (rows) - mines;
     if (!us || !mine_buffer || !mine_buffer_aux){
	  return -1;
     }

     do{
	  mine_buffer[auxi] = malloc(sizeof(int64_t[2]) * (rows));
     } while (mine_buffer[auxi] && auxi++ < (cols));

     for (int i = 0; i < (cols); i++) {
	  for (int j = 0; j < (rows); j++) {
	       mine_buffer[i][j][0] = -1;
	  }
     }

     for (int i = 0; i < players; i++) {
	  player_scores[i][0] = i;
	  player_scores[i][1] = 0;
	  player_ids[i] = i;
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
     init_pair(3, COLOR_WHITE, COLOR_BLUE);
     init_pair(4, COLOR_WHITE, COLOR_RED);
     init_pair(5, COLOR_WHITE, COLOR_YELLOW);
     init_pair(6, COLOR_WHITE, COLOR_GREEN);
     init_pair(7, COLOR_WHITE, COLOR_MAGENTA);
     init_pair(8, COLOR_WHITE, COLOR_CYAN);
     init_pair(9, COLOR_BLACK, COLOR_WHITE);

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

     win = create_window(win_h, win_w, (LINES - win_h) / 2, (COLS - win_w - 24) / 2);
     draw_minefield(win, mine_buffer, cols, rows);
     wrefresh(win);

     win_side = create_window(win_h, 24, (LINES - win_h) / 2, (COLS - win_w - 24) / 2 + (win_w + 24 / 2) - 12);

     update_scores(win_side, players, player_scores, utiles);
     update_mines(win_side, mines);
     update_utiles(win_side, utiles);
     update_marks(win_side, marks);
     wmove(win,y,x);
     wrefresh(win);

     /* enable non-blocking getch with delay */
     timeout(5);

     //wattrset(win, COLOR_PAIR(2));
     while(!win_flag && !quit_flag) {
    current_utc_time(&init_frame_time);
	  //clock_gettime(CLOCK_REALTIME,&init_frame_time);
	  select_timeout.tv_sec = 0;
	  select_timeout.tv_usec = 5000L;

	  switch(c=toupper(getch())) {
	  case 'Q':
	       quit_flag = TRUE;
	       break;
	  case 'X':
	       if (mine_buffer[x-1][y-1][0] == 10){
		    break;
	       }
	       qs.x = x-1;
	       qs.y = y-1;
	       mm_write(con, (char *) &qs, sizeof(QueryStruct));
	       break;
	  case ' ':
	       if (mine_buffer[x-1][y-1][0] < 0 || mine_buffer[x-1][y-1][0] == 10) {
		    if (mine_buffer[x-1][y-1][0] < 0) {
			 mine_buffer[x-1][y-1][0] = 10;
			 draw_tile(win, y-1, x-1, 10, 1);
			 marks++;
		    }
		    else {
			 mine_buffer[x-1][y-1][0] = -1;
			 draw_tile(win, y-1, x-1, 0, 1);
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
	       win = create_window(win_h, win_w, (LINES - win_h) / 2 , (COLS - win_w - 24) / 2);
	       draw_minefield(win,mine_buffer,cols,rows);
	       wrefresh(win);

	       win_side = create_window(win_h, 24, (LINES - win_h) / 2, (COLS - win_w - 24) / 2 + (win_w + 24 / 2) - 12);
	       update_mines(win_side, mines);
	       update_utiles(win_side, utiles);
	       update_marks(win_side, marks);
	       wrefresh(win_side);
	       wmove(win,y,x);
	       wrefresh(win);
	       break;
	  case ERR:
	       break;
	  default:
	       break;
	  }

	  if (mm_select(con, &select_timeout) > 0) {
	       mm_read(con, (char *) us, us_size);
	       count = us->len;
	       if (count > 0) {
		    for(int i = 0; i < count; i++){
			 auxx = us->tiles[i].x;
			 auxy = us->tiles[i].y;
			 auxn = us->tiles[i].nearby;
			 auxp = us->tiles[i].player;

			 if (mine_buffer[auxx][auxy][0] == 10) {
			      marks--;
			      update_marks(win_side,marks);
			      wmove(win,y,x);
			 }
			 if (auxn == 9) {
			      if (auxp == player_id) {
				   loose_flag = TRUE;
			      }
			      mines--;
			      player_scores[player_ids[auxp]][1] = -1;
			 }
			 else {
			      player_scores[player_ids[auxp]][1]++;
			 }
			 mine_buffer[auxx][auxy][0] = auxn;
			 mine_buffer[auxx][auxy][1] = us->tiles[i].player;
			 draw_tile(win, auxy,auxx,auxn, us->tiles[i].player + 3);
		    }

		    us->len = 0;
		    wmove(win,y,x);
		    utiles-=count;
		    update_utiles(win_side, utiles);
		    sort_scores(player_scores, players);
		    update_scores_to_ids(player_ids, player_scores, players);
		    update_scores(win_side, players, player_scores,utiles);
		    win_flag = check_win_state(player_ids, player_scores, players, utiles);
	       }
	  }
    current_utc_time(&end_frame_time);
	  //clock_gettime(CLOCK_REALTIME, &end_frame_time);
	  time_diff(&diff_frame_time,&init_frame_time,&end_frame_time);
	  nanosleep(&diff_frame_time,NULL);
	  wrefresh(win);
     }
     /* enable blocking getch() */
     timeout(-1);
     if (win_flag) {
	  wmove(win_side, 5 + players, 1);
	  wprintw(win_side, "YOU WIN!");
	  wrefresh(win_side);
	  getch();
     }
     else if (loose_flag) {
	  wmove(win_side, 5 + players, 1);
	  wprintw(win_side, "YOU LOOSE!");
	  wrefresh(win_side);
	  getch();
     }

     cli_exit();
     return 0;
}


/*
** Enables MacOS compatibility for
** clock_gettime. Obtained at
** https://gist.github.com/jbenet/1087739
*/
static void current_utc_time(struct timespec *ts) {
  #ifdef __MACH__
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
  #else
    clock_gettime(CLOCK_REALTIME, ts);
  #endif
}

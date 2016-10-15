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
#include <msg_structs.h>
#include <client_marsh.h>
#include <common.h>

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
void print_score(WINDOW * win, int base, int position, int player, int score, int total_tiles)
{
	int score_pc = (int) (( (double)score / total_tiles) * 100.0);

	wmove(win, base + position, 1);
	wprintw(win, "            ");
	wmove(win, base + position, 1);
	wattron(win,COLOR_PAIR(player+10));
	if (score >= 0) {
		wprintw(win, "PLAYER %d: %d %%", player, score_pc);
	}
	else {
		wprintw(win, "PLAYER %d: LOST", player);
	}
	wattroff(win,COLOR_PAIR(player + 10));
}

void update_scores(WINDOW * win, int64_t player_scores[8], int64_t players, int64_t utiles, int64_t total_tiles)
{
	int base = 5;
	for (int i = 0; i < players; i++) {
		print_score(win, base, i, i, player_scores[i], total_tiles);
	}
	wrefresh(win);
}

char fin[30], fout[30];
void cli_exit(char * msg) {
	char buf[50];
	sprintf(buf, "rm %s", fin);
	system(buf);
	sprintf(buf, "rm %s", fout);
	system(buf);
	endwin(); /* reset terminal */
	if (msg) {
		printf("%s", msg);
	}
	exit(0);
	return;
}

/* DEBUG: tidy FIFO delete handler */
void sig_handler(int signo)
{
	if (signo == SIGINT) {
		cli_exit("kill by interrupt\n");
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

int main(int argc, char *argv[])
{
	char * srv_addr;
	srv_addr=configuration("config",mm_commtype(),0);
	int64_t rows, cols, mines, players, player_id, us_size;
	int64_t player_scores[8];
	QueryStruct qs;
	UpdateStruct  * us;
	InitStruct *is;
	Connection * con;
	struct timeval timeout;
	int8_t selret;
	char * data_struct;
	int data_size;
	timeout.tv_sec = 20;
	timeout.tv_usec = 0;
	int64_t c, win_h, win_w, count = 0, auxi = 0, marks = 0;
	int8_t auxx, auxy, auxn, auxp;
	int64_t utiles = 0, total_tiles;
	int8_t win_flag = FALSE, loose_flag = FALSE, quit_flag = FALSE;
	struct timespec init_frame_time, end_frame_time, diff_frame_time,init,end;
	struct timeval select_timeout;
	int64_t (** mine_buffer)[2] = NULL, (*mine_buffer_aux)[3][2] = NULL;
	WINDOW * win, * win_side;
	int i;
	int highscores_on = 0;
	int8_t msg_type, x, y;
	EndGameStruct * es;
	BusyStruct * bs;
	void * t_aux;
	Highscore a;
	void * ptr;

	/* setting fifo path */
	sprintf(fin, "/tmp/r%d", getpid());
	sprintf(fout, "/tmp/w%d", getpid());
	signal(SIGINT, sig_handler);
	con = mm_connect(srv_addr);

	if (!con) {
		cli_exit("no se pudo subscribir.\n");
		return 0;
	}
	printf("suscrito.\n");
	selret = receive_init(con, &ptr, &timeout, 5);
	if (selret == INITGAME) {
		is = (InitStruct *) ptr;
		printf("cols: %d rows: %d mines: %d \n", (int) is->cols, (int) is->rows, (int) is->mines);
	}
	else if (selret == NOREAD){
		printf("el tiempo de espera expiro.\n");
		return 0;
	}
	else if (selret == BUSYSERVER) {
		bs = (BusyStruct *) ptr;
		printf("%s\n", bs->message);
		return 0;
	}
	else {
		printf("se produjo un error\n");
		return 0;
	}

	cols = is->cols;
	rows = is->rows;
	mines= is->mines;
	players = is->players;
	player_id = is->player_id;
	free(is);

	us_size = sizeof(UpdateStruct) + cols * rows * 4;
	us = malloc(us_size);
	data_size = sizeof(UpdateStruct) + cols * rows * 4 + 1;
	data_struct = malloc(data_size);
	if (!data_struct) {
		cli_exit("memory error\n");
		return 0;
	}
	mine_buffer = malloc(sizeof( int64_t * [2]) * (cols));
	mine_buffer_aux = malloc(sizeof(int64_t[3][2]) * (rows) * (cols));
	utiles = (cols) * (rows) - mines;
	total_tiles = cols * rows - mines;
	if (!us || !mine_buffer || !mine_buffer_aux){
		return -1;
	}

	do{
		t_aux = mine_buffer[auxi] = malloc(sizeof(int64_t[2]) * (rows));
	} while (t_aux && mine_buffer[auxi] && auxi++ < (cols));
	if (!t_aux) {
		printf("fracaso en %d \n", (int)auxi);
		return -1;
	}
	for (int i = 0; i < (cols); i++) {
		for (int j = 0; j < (rows); j++) {
			mine_buffer[i][j][0] = -1;
		}
	}

	for (int i=0; i<players;i++) {
		player_scores[i] = 0;
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
	init_pair(10, COLOR_BLUE, COLOR_BLACK);
	init_pair(11, COLOR_RED, COLOR_BLACK);
	init_pair(12, COLOR_YELLOW, COLOR_BLACK);
	init_pair(13, COLOR_GREEN, COLOR_BLACK);
	init_pair(14, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(15, COLOR_CYAN, COLOR_BLACK);

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
	refresh();

	win = create_window(win_h, win_w, (LINES - win_h) / 2, (COLS - win_w - 24) / 2);
	draw_minefield(win, mine_buffer, cols, rows);
	wrefresh(win);

	win_side = create_window(win_h, 24, (LINES - win_h) / 2, (COLS - win_w - 24) / 2 + (win_w + 24 / 2) - 12);


	update_scores(win_side, player_scores, players, utiles, total_tiles);

	update_mines(win_side, mines);
	update_utiles(win_side, utiles);
	update_marks(win_side, marks);
	wmove(win,y,x);
	wrefresh(win);

	/* enable non-blocking getch with delay */
	timeout(5);
	current_utc_time(&init);
	while(!win_flag && !quit_flag &&!loose_flag) {
		current_utc_time(&init_frame_time);
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
			send_query(con, &qs);
			break;
		case 'H':
			if(!highscores_on){
				send_h_query(con);
				highscores_on = 1;
			}
			else {
				wclear(win_side);
				win_side = create_window(win_h, 24, (LINES - win_h) / 2, (COLS - win_w - 24) / 2 + (win_w + 24 / 2) - 12);
				update_mines(win_side, mines);
				update_utiles(win_side, utiles);
				update_marks(win_side, marks);
				update_scores(win_side, player_scores, players, utiles, total_tiles);
				wmove(win,x,y);
				wrefresh(win);
				highscores_on = 0;
			}
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
				if(!highscores_on){
					update_marks(win_side, marks);
					wmove(win,y,x);
				}
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

		msg_type = receive_update(con, data_struct, data_size, &select_timeout);

		if (msg_type == UPDATEGAME) {
			us = (UpdateStruct *) &data_struct[1];
			count = us->len;
			if (count > 0) {
				for(int i = 0; i < count; i++){
					auxx = us->tiles[i].x;
					auxy = us->tiles[i].y;
					auxn = us->tiles[i].nearby;
					auxp = us->tiles[i].player;
					if (mine_buffer[auxx][auxy][0] == 10) {
						marks--;
						if(!highscores_on){
							update_marks(win_side,marks);
							wmove(win,y,x);
						}
					}
					if (auxn == 9) {
						if (auxp == player_id) {
							loose_flag = TRUE;
						}
						mines--;
						utiles++;
					}
					mine_buffer[auxx][auxy][0] = auxn;
					mine_buffer[auxx][auxy][1] = us->tiles[i].player;
					draw_tile(win, auxy,auxx,auxn, us->tiles[i].player + 3);

				}

				us->len = 0;
				wmove(win,y,x);
				utiles-=count;
				if(!highscores_on) {
					update_utiles(win_side, utiles);
				}
			}
			for (int i = 0; i < us->players; i++) {
				player_scores[i] = us->player_scores[i];
			}
			if(!highscores_on) {
				update_scores(win_side, player_scores, players, utiles, total_tiles);
			}
		}
		else if (msg_type == DISCONNECT) {
			cli_exit("desconectado\n");
		}
		else if(msg_type == HIGHSCORES){
			Highscore * h = (Highscore*) &data_struct[1];
			int j = 0;
			wclear(win_side);
			win_side = create_window(win_h, 24, (LINES - win_h) / 2, (COLS - win_w - 24) / 2 + (win_w + 24 / 2) - 12);
			wmove(win_side,1,1);
			wprintw(win_side,"Highscores:");
			i = 2;
			while(strcmp(h[j].name,"")!=0){
				wmove(win_side,i++,1);
				wprintw(win_side,"%s %d",h[j].name,h[j].score);//print scores
				j++;
			}
			wmove(win_side,i+1,1);
			wprintw(win_side,"Presione H para volver");
			wrefresh(win_side);
		}
		else if (msg_type == ENDGAME) {
			es = (EndGameStruct *) &data_struct[1];
			for (int i = 0; i < es->players; i++) {
				player_scores[i] = us->player_scores[i];
			}
			update_scores(win_side, player_scores, players, utiles, total_tiles);
			wmove(win_side, 5 + es->players + 1, 1);

			if (es->winner_id == player_id) {
				win_flag = 1;
			}
			else {
				loose_flag= 1;
			}
		}
		current_utc_time(&end_frame_time);
		time_diff(&diff_frame_time,&init_frame_time,&end_frame_time);
		nanosleep(&diff_frame_time,NULL);
		wrefresh(win);
	}

	/* enable blocking getch() */
	timeout(-1);
	current_utc_time(&end);

	wclear(win_side);
	win_side = create_window(win_h, 24, (LINES - win_h) / 2, (COLS - win_w - 24) / 2 + (win_w + 24 / 2) - 12);
	wmove(win_side,1,1);

	if(win_flag){
		int time = (end.tv_sec - init.tv_sec);
		char nombre[100] = " ";char d;
		int j = 0;
		wprintw(win_side,"YOU WIN!");
		wmove(win_side,2,1);
		wprintw(win_side,"Su tiempo es : %d",time);
		wmove(win_side,3,1);
		wprintw(win_side,"Ingrese un nombre de ");
		wmove(win_side,4,1);
		wprintw(win_side,"hasta 10 caracteres:");
		wmove(win_side,6,1);
		wrefresh(win_side);
		while((d=toupper(getch())) != '\n' ){
			if(d == 127 && j>0){//backspace
				wmove(win_side,6,j);
				wprintw(win_side," ");
				wmove(win_side,6,j--);
				wrefresh(win_side);
			}
			else if(j<10 && d != 127){
				nombre[j++]=d;
				wprintw(win_side,"%c",d);
				wrefresh(win_side);
			 }
		}
		wmove(win_side,10,1);
		wprintw(win_side,"Se ha agregado %s",nombre);
		wmove(win_side,11,1);
		wprintw(win_side,"correctamente! %s",nombre);
		wmove(win_side, 12, 1);
		sprintf(a.name,"%s", nombre);
		a.score = time;
		//send_highscore(con,&a);

		wprintw(win_side, "PRESS ENTER TO EXIT");
		wrefresh(win_side);
	}
	else{
		wmove(win_side,i+2,1);
		wprintw(win_side, "YOU LOSE!");
		wmove(win_side,i+3,1);
		wprintw(win_side, "PRESS ENTER TO EXIT!");
		if(players == 1){
			sprintf(a.name," ");
			a.score = -1;
			//send_highscore(con,&a);
		}
	}

	wrefresh(win_side);
	timeout(-1);
	while(getch()!='\n');
	cli_exit("termino el juego\n");
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

#include <msg_structs.h>
#include <stdint.h>


int open_database();
void close_database();
void insert_highscore(char * name,int score);
Highscore * get_highscores(int * count);
int clear_highscores();

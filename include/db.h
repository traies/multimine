#include <msg_structs.h>
#include <stdint.h>
#define LOSE 0
#define WIN 1

int open_database();
void insert_highscore(char * name,int score);
Highscore * get_highscores(int * count);
void add_id(char * name);
void update_id(char * name,int type);
Stat * get_stats(int * count);

#define LOSE 0
#define WIN 1
typedef struct Highscore{
  char name[20];
  int score;
}Highscore;

typedef struct Stat{
  char name[20];
  int wins;
  int loses;
}Stat;

int open_database();
void insert_highscore(char * name,int score);
Highscore * get_highscores(int * count);
void add_id(char * name);
void update_id(char * name,int type);
Stat * get_stats(int * count);

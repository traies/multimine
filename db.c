#include <math.h>
#include <sqlite3.h>
#include <stdio.h>
#include "./include/db.h"
#include <string.h>
#include <stdlib.h>
#define MAX_SIZE 1000
#define max_highscores 10
#define max_stats 10

static int save_highscores(void *unused, int count, char **data, char **columns);
static int get_number_from(char * str);
static int save_stats(void *unused, int count, char **data, char **columns);

static sqlite3 * db;
static Highscore h[max_highscores];
static Stat s[max_stats];
static int idx = 0;

int open_database(){
  char * error;int i;
    if (sqlite3_open("example.db", &db) != SQLITE_OK)
        return -1;
    sqlite3_exec(db, "drop TABLE HIGHSCORES", NULL, NULL, &error);
    sqlite3_exec(db, "CREATE TABLE HIGHSCORES(name text ,score int not null,primary key(name,score))", NULL, NULL, &error);
    sqlite3_exec(db, "drop TABLE WIN_LOSE", NULL, NULL, &error);

    sqlite3_exec(db, "CREATE TABLE WIN_LOSE(name text ,wins int not null,loses int not null,primary key(name,wins,loses))", NULL, NULL, &error);
    return 0;
}

void add_id(char * name){
  char * error;
char * query = malloc(MAX_SIZE*sizeof(char));
sprintf(query,"INSERT INTO WIN_LOSE values ('%s',0,0)",name);
sqlite3_exec(db,query , NULL, NULL, &error);
}

void update_id(char * name,int type){
    char * error;
  char * query = malloc(MAX_SIZE*sizeof(char));
  if(type == WIN)
    sprintf(query,"UPDATE WIN_LOSE set wins=wins+1 where name = '%s'",name);
  else
    sprintf(query,"UPDATE WIN_LOSE set loses=loses+1 where name = '%s'",name);
  sqlite3_exec(db,query , NULL, NULL, &error);
}

void insert_highscore(char * name,int score){
    char * error;
  char * query = malloc(MAX_SIZE*sizeof(char));
  sprintf(query,"INSERT INTO HIGHSCORES values ('%s',%d)",name,score);
  sqlite3_exec(db,query , NULL, NULL, &error);
}

Highscore * get_highscores(int * count){
  char * error;
  sqlite3_exec(db, "SELECT * FROM HIGHSCORES ORDER BY score DESC", save_highscores, NULL, &error);
  *count = idx;
  idx = 0;
  return h;
}

Stat * get_stats(int * count){
  char * error;
  sqlite3_exec(db, "SELECT * FROM WIN_LOSE ORDER BY name ASC", save_stats, NULL, &error);
  *count = idx;
  idx = 0;
  return s;
}


/*
 * Arguments:
 *
 *   unused - Ignored in this case, see the documentation for sqlite3_exec
 *    count - The number of columns in the result set
 *     data - The row's data
 *  columns - The column names
 */
 static int save_highscores(void *unused, int count, char **data, char **columns)
 {
   if(idx < 10){
      sprintf(h[idx].name,data[0]);
      h[idx].score = get_number_from(data[1]);
   }
   idx++;
      return 0;
 }

 static int save_stats(void *unused, int count, char **data, char **columns)
 {
   if(idx < 10){
      sprintf(s[idx].name,data[0]);
      s[idx].wins = get_number_from(data[1]);
      s[idx].loses = get_number_from(data[2]);
   }
   idx++;
      return 0;
 }

 static int get_number_from(char * str){
   int i;
   int times = strlen(str);
   int aux = pow(10,times -1);
   int ans = 0;

   for(i=0;i< times;i++){
     ans += (str[i]-'0')*aux;
     aux /= 10;
   }
   return ans;

 }

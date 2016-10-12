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

static sqlite3 * db;
static Highscore h[max_highscores];
static int idx = 0;

int open_database(){
  char * error;
    if (sqlite3_open("example.db", &db) != SQLITE_OK)
        return -1;
    sqlite3_exec(db, "CREATE TABLE HIGHSCORES(name text,score int not null,primary key(name,score))", NULL, NULL, &error);
    insert_highscore("ALEJO",100);
    insert_highscore("TOMAS",100);
    insert_highscore("NICOLAS",100);
    insert_highscore("",10000);
    return 0;
}


int clear_highscores(){
  char * error;
  sqlite3_exec(db,"drop TABLE HIGHSCORES",NULL,NULL,&error);
  return 0;
}


void insert_highscore(char * name,int score){
    char * error;
  char * query = malloc(MAX_SIZE*sizeof(char));
  sprintf(query,"INSERT INTO HIGHSCORES values ('%s',%d)",name,score);
  sqlite3_exec(db,query , NULL, NULL, &error);
}

Highscore * get_highscores(int * count){
  char * error;
  sqlite3_exec(db, "SELECT * FROM HIGHSCORES ORDER BY score ASC", save_highscores, NULL, &error);
  *count = idx;
  idx = 0;
  return h;
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
      sprintf(h[idx].name, "%s", data[0]);
      h[idx].score = get_number_from(data[1]);
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

#include <sqlite3.h>
#include <stdio.h>
int main(){
  sqlite3 * myDatabase;

    if (sqlite3_open("example.db", &myDatabase) == SQLITE_OK)
        printf("Database Opened\n");
    else
        printf("Failed to Open\n");
}

#include "./include/db.h"
#include <stdio.h>
int main(){
  int ans;
  ans = open_database();
  if(ans != 0){
    return 0;
  }
  insert_highscore("nico",10);
  insert_highscore("tomi",1);
  insert_highscore("alejo",9);
  add_id("nico");
  update_id("nico",WIN);
  update_id("nico",LOSE);
    int count;
    Stat * s = get_stats(&count);
    int i = 0;
    printf("%d\n",count );
    for(;i < count;i++){
      printf("%s %d %d\n",s[i].name,s[i].wins,s[i].loses );
    }
  Highscore * h = get_highscores(&count);


  printf("%d\n",count );
  for(i=0;i < count;i++){
    printf("%s %d\n",h[i].name,h[i].score );
  }

  return 0;

}

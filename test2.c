#include "comms.h"
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
int main()
{
  char * path = "/tmp/p2";
  Listener_p l=mm_listen((Address *)path);
  Connection * c = mm_connect((Address *)"/tmp/p1");
  mm_write(c,"hola",5);
}

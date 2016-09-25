#include "comms.h"
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
int main()
{
  char * path = "/tmp/p1";
  Listener_p l=mm_listen((Address *)path);
  Connection * c = mm_accept(l);
  char buf[20];
  mm_read(c,buf,20);
  puts(buf);
}

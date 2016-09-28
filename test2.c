#include "comms.h"
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
int main()
{
  Connection * c = mm_connect("/tmp/p1");
  mm_write(c,"hola",5);
}

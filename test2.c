#include "comms.h"
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char ** argv)
{
  Connection * c = mm_connect(argv[1]);
  mm_write(c,"hola",5);
}

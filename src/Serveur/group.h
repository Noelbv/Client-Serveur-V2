#ifndef GROUP_H
#define GROUP_H

#include "client2.h"

typedef struct
{
   char name[BUF_SIZE];
   char members[10][BUF_SIZE];
   int size;
} Group;

#endif /* guard */
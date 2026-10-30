// SPDX-License-Identifier: GPL-3.0-or-later
// Optional GUI-test interposer only; never linked into or installed with the app.
#define _GNU_SOURCE
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <errno.h>
static atomic_int claimed=0;
int fsync(int fd){
  int result=(int)syscall(SYS_fsync,fd);if(result)return result;
  const char *directory=getenv("DOME_TEST_EXPORT_GATE_DIR");
  const char *output=getenv("DOME_TEST_EXPORT_OUTPUT");
  if(!directory || !output || atomic_load(&claimed))return result;
  char descriptor[64],path[4096];snprintf(descriptor,sizeof(descriptor),"/proc/self/fd/%d",fd);
  ssize_t size=readlink(descriptor,path,sizeof(path)-1);if(size<0)return result;path[size]=0;
  size_t prefix=strlen(output);
  if(strncmp(path,output,prefix) || strncmp(path+prefix,".tmp-",5))return result;
  int expected=0;if(!atomic_compare_exchange_strong(&claimed,&expected,1))return result;
  char marker[4096];snprintf(marker,sizeof(marker),"%s/entered",directory);
  int signal=open(marker,O_CREAT|O_WRONLY|O_CLOEXEC,0600);if(signal<0)return -1;close(signal);
  snprintf(marker,sizeof(marker),"%s/release",directory);
  struct timespec start,now,delay={0,5000000};clock_gettime(CLOCK_MONOTONIC,&start);
  while(access(marker,F_OK)){
    clock_gettime(CLOCK_MONOTONIC,&now);if(now.tv_sec-start.tv_sec>=90){errno=ETIMEDOUT;return -1;}
    nanosleep(&delay,0);
  }
  return result;
}

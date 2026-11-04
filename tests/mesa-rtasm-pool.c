/* Test-only native harness for the unmodified/patched Mesa allocator TUs.
 * Link wrapping observes allocation ownership; no allocator source is mocked.
 */
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#define HEAP_SIZE (10u * 1024u * 1024u)
void *rtasm_exec_malloc(size_t);
void rtasm_exec_free(void *);
void *__real_calloc(size_t, size_t);
void __real_free(void *);
void *__real_mmap(void *, size_t, int, int, int, off_t);
void *__real_mmap64(void *, size_t, int, int, int, off64_t);
int __real_munmap(void *, size_t);
static atomic_long blocks, mappings;
static _Thread_local unsigned fail_calloc, calloc_calls;
static _Thread_local int fail_mapping;
static unsigned checks;
static void check(int ok, const char *label) {
   if (!ok) {
      fprintf(stderr,"FAIL %s (metadata=%ld mappings=%ld)\n",label,
              atomic_load(&blocks),atomic_load(&mappings));
      exit(1);
   }
   ++checks;
}
void *__wrap_calloc(size_t n, size_t size) {
   if (fail_calloc && ++calloc_calls == fail_calloc) return NULL;
   void *p = __real_calloc(n,size);
   if (p) atomic_fetch_add(&blocks,1);
   return p;
}
void __wrap_free(void *p) {
   if (p) atomic_fetch_sub(&blocks,1);
   __real_free(p);
}
void *__wrap_mmap(void *p,size_t size,int prot,int flags,int fd,off_t offset) {
   if (fail_mapping) { fail_mapping=0; return MAP_FAILED; }
   void *result=__real_mmap(p,size,prot,flags,fd,offset);
   if (result!=MAP_FAILED) atomic_fetch_add(&mappings,1);
   return result;
}
void *__wrap_mmap64(void *p,size_t size,int prot,int flags,int fd,off64_t offset) {
   if (fail_mapping) { fail_mapping=0; return MAP_FAILED; }
   void *result=__real_mmap64(p,size,prot,flags,fd,offset);
   if (result!=MAP_FAILED) atomic_fetch_add(&mappings,1);
   return result;
}
int __wrap_munmap(void *p,size_t size) {
   int result=__real_munmap(p,size);
   if (result==0) atomic_fetch_sub(&mappings,1);
   return result;
}
void _debug_vprintf(const char *format, va_list args) { vfprintf(stderr,format,args); }
static void empty(const char *label) {
   check(atomic_load(&blocks)==0 && atomic_load(&mappings)==0,label);
}
static void *stress(void *arg) {
   uintptr_t salt=(uintptr_t)arg;
   for (unsigned j=0;j<500;++j) {
      unsigned char *p=rtasm_exec_malloc(32+salt);
      unsigned char *q=rtasm_exec_malloc(65+salt);
      if (!p || !q) return (void *)1;
      p[0]=(unsigned char)salt; q[0]=(unsigned char)(salt+1);
      sched_yield();
      if (p[0]!=salt || q[0]!=(unsigned char)(salt+1)) return (void *)2;
      rtasm_exec_free(j%2?p:q);
      sched_yield();
      if ((j%2?q:p)[0]!=(j%2?(unsigned char)(salt+1):salt)) return (void *)3;
      rtasm_exec_free(j%2?q:p);
   }
   return NULL;
}
int main(void) {
   unsigned char *a=rtasm_exec_malloc(33), *b=rtasm_exec_malloc(65);
   check(a && b && a!=b && (uintptr_t)a%32==0 && (uintptr_t)b%32==0,"aligned independent live blocks");
   memset(a,0x5a,33);memset(b,0xa5,65);
   rtasm_exec_free(a);
   check(atomic_load(&mappings)==1 && b[0]==0xa5 && b[64]==0xa5,"freeing one block preserves other executable owner");
   rtasm_exec_free(b);empty("last owner frees both mapping and heap metadata");
   for (unsigned prefix=1;prefix<=3;++prefix) {
      fail_calloc=prefix;calloc_calls=0;
      check(rtasm_exec_malloc(64)==NULL,"actual metadata allocation failure refuses");
      fail_calloc=0;empty("each metadata allocation failure releases partial ownership");
      a=rtasm_exec_malloc(64);check(a!=NULL,"same allocator retries after metadata failure");
      rtasm_exec_free(a);empty("metadata failure retry drains completely");
   }
   fail_mapping=1;
   check(rtasm_exec_malloc(64)==NULL,"actual mmap failure refuses");
   empty("mmap failure destroys already allocated heap metadata");
   a=rtasm_exec_malloc(64);check(a!=NULL,"mmap failure retry succeeds");
   a[0]=47; fail_calloc=1;calloc_calls=0;
   check(rtasm_exec_malloc(64)==NULL,"split failure with another live owner refuses");
   fail_calloc=0;
   check(a[0]==47 && atomic_load(&mappings)==1,"split failure preserves another live allocation");
   b=rtasm_exec_malloc(64);check(b!=NULL,"split failure retries without losing live owner");
   rtasm_exec_free(b);rtasm_exec_free(a);empty("split failure retry drains exact pool");
   check(rtasm_exec_malloc(0)==NULL && rtasm_exec_malloc(SIZE_MAX)==NULL && rtasm_exec_malloc(HEAP_SIZE+1)==NULL,"zero overflow and over-capacity requests refuse");
   empty("invalid requests acquire no pool");
   a=rtasm_exec_malloc(HEAP_SIZE);check(a!=NULL,"full capacity allocation succeeds");a[0]=13;a[HEAP_SIZE-1]=29;
   check(rtasm_exec_malloc(1)==NULL && a[0]==13 && a[HEAP_SIZE-1]==29,"exhausted pool preserves live whole-heap allocation");
   rtasm_exec_free(a);empty("whole-heap allocation releases after exhaustion");
#if defined(__x86_64__) || defined(__i386__)
   a=rtasm_exec_malloc(6);check(a!=NULL,"executable allocation succeeds");
   const unsigned char machine[]={0xb8,42,0,0,0,0xc3};memcpy(a,machine,sizeof(machine));
   __builtin___clear_cache((char *)a,(char *)a+sizeof(machine));
   check(((int(*)(void))a)()==42,"allocated generated machine code actually executes");
   b=rtasm_exec_malloc(32);check(b!=NULL,"second owner with live function succeeds");rtasm_exec_free(b);
   check(((int(*)(void))a)()==42,"freeing another owner keeps generated function executable");
   rtasm_exec_free(a);empty("generated function last-free drains");
#endif
   for(unsigned i=0;i<100;++i){a=rtasm_exec_malloc(i*31+1);check(a!=NULL,"repeated context-style allocation succeeds");rtasm_exec_free(a);empty("repeated context-style last-free drains");}
   pthread_t threads[4];
   for(uintptr_t i=0;i<4;++i)check(pthread_create(&threads[i],NULL,stress,(void *)(i+1))==0,"concurrent worker starts");
   for(unsigned i=0;i<4;++i){void *result=NULL;check(pthread_join(threads[i],&result)==0 && !result,"concurrent owners retain data and complete");}
   empty("all concurrent owners release final pool");
   printf("PASS %u allocator checks; 2000 concurrent cycles; no retained metadata or executable mappings\n",checks);
   return 0;
}

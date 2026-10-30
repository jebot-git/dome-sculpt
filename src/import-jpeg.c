// SPDX-License-Identifier: GPL-3.0-or-later
#include "import-jpeg.h"
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
struct decode {
  struct jpeg_error_mgr error;
  struct jpeg_decompress_struct jpeg;
  jmp_buf jump;
  unsigned char *pixels;
  int created;
  size_t allocated;
  char message[JMSG_LENGTH_MAX];
  struct jpeg_progress_mgr progress;
  int (*cancelled)(void*);
  void *cancel_context;
  void *(*small)(j_common_ptr,int,size_t);
  void *(*large)(j_common_ptr,int,size_t);
};
static void fail(struct decode *d,const char *message){snprintf(d->message,sizeof(d->message),"%s",message);longjmp(d->jump,1);}
static void error_exit(j_common_ptr jpeg){struct decode *d=(struct decode*)jpeg->client_data;(*jpeg->err->format_message)(jpeg,d->message);longjmp(d->jump,1);}
static void emit_message(j_common_ptr jpeg,int level){if(level<0)fail((struct decode*)jpeg->client_data,"Malformed or truncated JPEG input");}
static void progress_monitor(j_common_ptr jpeg){
  struct decode*d=(struct decode*)jpeg->client_data;
  if(d->cancelled && d->cancelled(d->cancel_context))fail(d,"JPEG import cancelled");
  if(((j_decompress_ptr)jpeg)->input_scan_number>64)fail(d,"JPEG progressive scan budget exceeded");
}
static void *small(j_common_ptr jpeg,int pool,size_t size){struct decode*d=(struct decode*)jpeg->client_data;if(size>256u*1024u*1024u-d->allocated)fail(d,"JPEG decoder memory budget exceeded");d->allocated+=size;return d->small(jpeg,pool,size);}
static void *large(j_common_ptr jpeg,int pool,size_t size){struct decode*d=(struct decode*)jpeg->client_data;if(size>256u*1024u*1024u-d->allocated)fail(d,"JPEG decoder memory budget exceeded");d->allocated+=size;return d->large(jpeg,pool,size);}
int dome_import_jpeg(const uint8_t *input,size_t size,size_t max_pixels,uint8_t **rgba,unsigned *width,unsigned *height,char *message,size_t message_size,int (*cancelled)(void*),void *cancel_context){
  // Heap state survives setjmp/longjmp without indeterminate automatic fields.
  struct decode *d=(struct decode*)calloc(1,sizeof(struct decode));
  if(rgba)*rgba=NULL;
  *width=0;*height=0;
  if(!d){snprintf(message,message_size,"JPEG decoder allocation failed");return 0;}
  d->jpeg.err=jpeg_std_error(&d->error);d->error.error_exit=error_exit;d->error.emit_message=emit_message;d->jpeg.client_data=d;
  if(setjmp(d->jump)){
    snprintf(message,message_size,"%s",d->message);
    if(d->created)jpeg_destroy_decompress(&d->jpeg);
    free(d->pixels);free(d);return 0;
  }
  d->cancelled=cancelled;d->cancel_context=cancel_context;
  if(cancelled&&cancelled(cancel_context))fail(d,"JPEG import cancelled");
  d->created=1;jpeg_create_decompress(&d->jpeg);
  d->progress.progress_monitor=progress_monitor;d->jpeg.progress=&d->progress;
  d->small=d->jpeg.mem->alloc_small;d->large=d->jpeg.mem->alloc_large;
  d->jpeg.mem->alloc_small=small;d->jpeg.mem->alloc_large=large;
  d->jpeg.mem->max_memory_to_use=64u*1024u*1024u;
  jpeg_mem_src(&d->jpeg,input,(unsigned long)size);
  if(jpeg_read_header(&d->jpeg,TRUE)!=JPEG_HEADER_OK)fail(d,"Invalid JPEG header");
  if(!d->jpeg.image_width || !d->jpeg.image_height || d->jpeg.image_width>8192 || d->jpeg.image_height>8192 ||
      d->jpeg.image_height>max_pixels/d->jpeg.image_width || max_pixels>SIZE_MAX/4)
    fail(d,"JPEG image dimension/pixel budget exceeded");
  if(d->jpeg.jpeg_color_space!=JCS_RGB && d->jpeg.jpeg_color_space!=JCS_YCbCr && d->jpeg.jpeg_color_space!=JCS_GRAYSCALE)
    fail(d,"JPEG image requires standard RGB or grayscale color space");
  if(!rgba){*width=d->jpeg.image_width;*height=d->jpeg.image_height;jpeg_destroy_decompress(&d->jpeg);free(d);return 1;}
  d->jpeg.out_color_space=JCS_RGB;
  if(!jpeg_start_decompress(&d->jpeg))fail(d,"JPEG decompression did not start");
  size_t count=(size_t)d->jpeg.output_width*d->jpeg.output_height;
  d->pixels=(unsigned char*)malloc(count*4);
  if(!d->pixels)fail(d,"JPEG image allocation failed");
  JSAMPARRAY row=(*d->jpeg.mem->alloc_sarray)((j_common_ptr)&d->jpeg,JPOOL_IMAGE,d->jpeg.output_width*3,1);
  while(d->jpeg.output_scanline<d->jpeg.output_height){
    size_t y=d->jpeg.output_scanline;
    if(jpeg_read_scanlines(&d->jpeg,row,1)!=1)fail(d,"Truncated JPEG scanline");
    for(size_t x=0;x<d->jpeg.output_width;++x){size_t at=(y*d->jpeg.output_width+x)*4;d->pixels[at]=row[0][x*3];d->pixels[at+1]=row[0][x*3+1];d->pixels[at+2]=row[0][x*3+2];d->pixels[at+3]=255;}
  }
  if(!jpeg_finish_decompress(&d->jpeg))fail(d,"Incomplete JPEG image");
  *width=d->jpeg.output_width;*height=d->jpeg.output_height;*rgba=d->pixels;d->pixels=NULL;
  jpeg_destroy_decompress(&d->jpeg);free(d);return 1;
}

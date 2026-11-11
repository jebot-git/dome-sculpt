// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/render.hpp"
#include <algorithm>
#include <vector>
namespace dome::detail {
 struct PaintCompilerState {
  GLint fbo=0,viewport[4]{},program=0,vao=0,active=0,unpack=0,buffer=0;GLint copy_read=0,copy_write=0;GLint polygon[2]{};std::vector<GLboolean> clips,blending;std::vector<std::array<GLboolean,4>> colours;
  static constexpr std::array<GLenum,10> switches{GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_FRAMEBUFFER_SRGB,GL_RASTERIZER_DISCARD,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE,GL_DITHER,GL_SAMPLE_MASK};
  std::array<GLboolean,switches.size()> enabled{};std::array<GLint,8> two_d{},tbo{},sampler{};
  static constexpr std::array<GLenum,6> stores{GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_IMAGE_HEIGHT,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_IMAGES};std::array<GLint,stores.size()> values{};
  PaintCompilerState(){GLint draw=0,clip=0;glGetIntegerv(GL_MAX_DRAW_BUFFERS,&draw);glGetIntegerv(GL_MAX_CLIP_DISTANCES,&clip);colours.resize(size_t(std::max(0,draw)));blending.resize(colours.size());clips.resize(size_t(std::max(0,clip)));glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&copy_read);glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING,&copy_write);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);glGetIntegerv(GL_TEXTURE_BUFFER_BINDING,&buffer);for(size_t i=0;i<colours.size();++i){glGetBooleani_v(GL_COLOR_WRITEMASK,GLuint(i),colours[i].data());blending[i]=glIsEnabledi(GL_BLEND,GLuint(i));}glDisable(GL_BLEND);glGetIntegerv(GL_POLYGON_MODE,polygon);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);for(size_t i=0;i<clips.size();++i){clips[i]=glIsEnabled(GL_CLIP_DISTANCE0+i);glDisable(GL_CLIP_DISTANCE0+i);}
   for(size_t i=0;i<switches.size();++i){enabled[i]=glIsEnabled(switches[i]);glDisable(switches[i]);}glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
   for(size_t i=0;i<stores.size();++i){glGetIntegerv(stores[i],&values[i]);glPixelStorei(stores[i],i==0?1:0);}
   for(size_t i=0;i<8;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&two_d[i]);glGetIntegerv(GL_TEXTURE_BINDING_BUFFER,&tbo[i]);glGetIntegeri_v(GL_SAMPLER_BINDING,GLuint(i),&sampler[i]);glBindSampler(GLuint(i),0);}
  }
  ~PaintCompilerState(){glBindBuffer(GL_COPY_READ_BUFFER,GLuint(copy_read));glBindBuffer(GL_COPY_WRITE_BUFFER,GLuint(copy_write));glPolygonMode(GL_FRONT_AND_BACK,GLenum(polygon[0]));for(size_t i=0;i<clips.size();++i)if(clips[i])glEnable(GL_CLIP_DISTANCE0+i);else glDisable(GL_CLIP_DISTANCE0+i);for(size_t i=0;i<8;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,GLuint(two_d[i]));glBindTexture(GL_TEXTURE_BUFFER,GLuint(tbo[i]));glBindSampler(GLuint(i),GLuint(sampler[i]));}glActiveTexture(GLenum(active));glBindBuffer(GL_TEXTURE_BUFFER,GLuint(buffer));glBindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(unpack));for(size_t i=0;i<stores.size();++i)glPixelStorei(stores[i],values[i]);for(size_t i=0;i<switches.size();++i)if(enabled[i])glEnable(switches[i]);else glDisable(switches[i]);for(size_t i=0;i<colours.size();++i){glColorMaski(GLuint(i),colours[i][0],colours[i][1],colours[i][2],colours[i][3]);if(blending[i])glEnablei(GL_BLEND,GLuint(i));else glDisablei(GL_BLEND,GLuint(i));}glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(fbo));glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glUseProgram(GLuint(program));glBindVertexArray(GLuint(vao));}
 };
}

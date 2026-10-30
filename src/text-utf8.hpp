// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>
namespace dome::text {
inline bool valid_utf8(std::string_view text) {
  for(size_t i=0;i<text.size();) {
    auto first=static_cast<unsigned char>(text[i++]);
    if(first<0x80)continue;
    unsigned count=first>=0xc2&&first<=0xdf?1:first>=0xe0&&first<=0xef?2:first>=0xf0&&first<=0xf4?3:0;
    if(!count || text.size()-i<count)return false;
    unsigned value=first&((1u<<(6-count))-1),minimum=count==1?0x80:count==2?0x800:0x10000;
    for(unsigned n=0;n<count;++n) {
      auto next=static_cast<unsigned char>(text[i++]);
      if((next&0xc0)!=0x80)return false;
      value=(value<<6)|(next&0x3f);
    }
    if(value<minimum || value>0x10ffff || (value>=0xd800&&value<=0xdfff))return false;
  }
  return true;
}
// Generated-name byte budgets preserve complete scalars. Interactive visual
// clipping/shaping uses the separate grapheme-aware palette text implementation.
inline std::string utf8_prefix(std::string_view text,size_t bytes) {
  if(text.size()<=bytes)return std::string(text);
  while(bytes && (static_cast<unsigned char>(text[bytes])&0xc0)==0x80)--bytes;
  return std::string(text.substr(0,bytes));
}
} // namespace dome::text

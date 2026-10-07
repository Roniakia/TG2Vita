#pragma once
#include "emoji_catalog.hpp"
#include <algorithm>
#include <cstring>
#include <string>
#include <iterator>

namespace ui::emoji {
struct Token { std::size_t bytes; int tile; };
inline std::size_t codepoint_end(const std::string& value,std::size_t offset) {
    auto end=offset+1;
    while (end<value.size() && (static_cast<unsigned char>(value[end])&0xc0)==0x80) ++end;
    return end;
}
// Longest supported sequence wins, keeping flags, modifiers and ZWJ chains together.
inline Token next(const std::string& value,std::size_t offset) {
    Token result{codepoint_end(value,offset)-offset,-1};
    for (auto end=offset;end<value.size() && end-offset<128;) {
        end=codepoint_end(value,end);
        const auto key=value.substr(offset,end-offset);
        auto entry=std::lower_bound(std::begin(catalog),std::end(catalog),key,
            [](const Entry& a,const std::string& b) { return std::strcmp(a.sequence,b.c_str())<0; });
        if (entry==std::end(catalog) || std::strncmp(entry->sequence,key.c_str(),key.size())!=0) break;
        if (key==entry->sequence) result={end-offset,static_cast<int>(entry->tile)};
    }
    // VS15 explicitly requests text presentation.
    if (value.compare(offset+result.bytes,3,"\xef\xb8\x8e")==0) return {result.bytes+3,-1};
    return result;
}
template<class Measure>
float width(const std::string& value,float emoji_width,Measure measure) {
    float total=0; std::string run;
    for (std::size_t i=0;i<value.size();) {
        const auto token=next(value,i);
        if (token.tile>=0) { total+=measure(run)+emoji_width; run.clear(); }
        else run+=value.substr(i,token.bytes);
        i+=token.bytes;
    }
    return total+measure(run);
}
}

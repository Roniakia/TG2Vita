#pragma once
#include <vector>
struct vita2d_texture { unsigned w,h,stride; std::vector<unsigned char> data; };
constexpr int SCE_GXM_TEXTURE_FILTER_LINEAR=1;
vita2d_texture* vita2d_create_empty_texture(unsigned,unsigned);
void vita2d_free_texture(vita2d_texture*);
void* vita2d_texture_get_datap(const vita2d_texture*);
unsigned vita2d_texture_get_stride(const vita2d_texture*);
void vita2d_texture_set_filters(vita2d_texture*,int,int);

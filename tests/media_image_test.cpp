#include <vita2d.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cassert>
#include <jpeglib.h>
#include <png.h>
#include <iostream>
namespace ui { vita2d_texture* load_media_image(const std::string&); }
static int live=0,allocations=0;
vita2d_texture* vita2d_create_empty_texture(unsigned w,unsigned h) {
    ++live; ++allocations; auto t=new vita2d_texture{w,h,((w+7)&~7u)*4,{}};
    t->data.resize(t->stride*h,0xcd); return t;
}
void vita2d_free_texture(vita2d_texture* t) { --live; delete t; }
void* vita2d_texture_get_datap(const vita2d_texture* t) { return const_cast<unsigned char*>(t->data.data()); }
unsigned vita2d_texture_get_stride(const vita2d_texture* t) { return t->stride; }
void vita2d_texture_set_filters(vita2d_texture*,int,int) {}
void write_jpeg(const std::string& path,unsigned width=3,unsigned height=2) {
    auto file=fopen(path.c_str(),"wb"); assert(file);
    jpeg_compress_struct c{}; jpeg_error_mgr error{};
    c.err=jpeg_std_error(&error); jpeg_create_compress(&c); jpeg_stdio_dest(&c,file);
    c.image_width=width; c.image_height=height; c.input_components=3; c.in_color_space=JCS_RGB;
    jpeg_set_defaults(&c); jpeg_start_compress(&c,TRUE);
    std::vector<unsigned char> row(width*3,127); JSAMPROW p=row.data();
    while(c.next_scanline<c.image_height) jpeg_write_scanlines(&c,&p,1);
    jpeg_finish_compress(&c); jpeg_destroy_compress(&c); fclose(file);
}
int main(int argc,char** argv) {
    assert(argc==2); const std::string dir=argv[1];
    const auto jpg=dir+"/sample.jpg",png=dir+"/sample.png",bad=dir+"/bad.jpg",big=dir+"/big.jpg";
    write_jpeg(jpg);
    auto t=ui::load_media_image(jpg); assert(t && t->w==3 && t->h==2 && live==1);
    // GPU stride padding is honored on the second row, and JPEG is opaque.
    assert(t->data[3]==255 && t->data[t->stride+3]==255 && t->data[12]==0xcd);
    vita2d_free_texture(t);
    png_image image{}; image.version=PNG_IMAGE_VERSION; image.width=3; image.height=2; image.format=PNG_FORMAT_RGBA;
    std::vector<unsigned char> pixels(24,128); assert(png_image_write_to_file(&image,png.c_str(),0,pixels.data(),0,nullptr));
    t=ui::load_media_image(png); assert(t && t->data[3]==128 && t->data[t->stride+3]==128); vita2d_free_texture(t);
    auto f=fopen(bad.c_str(),"wb"); assert(f); const unsigned char broken[]={0xff,0xd8,0xff,0xc0,0,2,0,0}; fwrite(broken,1,sizeof(broken),f); fclose(f);
    assert(!ui::load_media_image(bad) && live==0);
    write_jpeg(big,3000,1); const auto count=allocations;
    assert(!ui::load_media_image(big) && allocations==count && live==0);
    assert(!ui::load_media_image(dir+"/missing.jpg"));
    std::cout<<"Media decoding, alpha, stride, corrupt data and dimension limits passed\n";
}

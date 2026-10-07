#include <vita2d.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdint>
#include <csetjmp>
#include <jpeglib.h>
#include <png.h>

namespace ui {
namespace {
bool dimensions(unsigned w,unsigned h) { return w && h && w<=2048 && h<=2048 && std::uint64_t(w)*h<=2*1024*1024; }
struct JpegState {
    jpeg_decompress_struct decoder{};
    jpeg_error_mgr error{};
    std::jmp_buf jump;
    vita2d_texture* texture=nullptr;
    unsigned char* row=nullptr;
};
void jpeg_failure(j_common_ptr decoder) { auto state=reinterpret_cast<JpegState*>(decoder); std::longjmp(state->jump,1); }
void quiet(j_common_ptr) {}
vita2d_texture* jpeg_image(FILE* file) {
    // Heap state remains valid across libjpeg's error longjmp; no private path
    // or decoder diagnostic is printed, and damaged images cannot exit the app.
    auto state=new JpegState;
    state->decoder.err=jpeg_std_error(&state->error);
    state->error.error_exit=jpeg_failure; state->error.output_message=quiet;
    if (setjmp(state->jump)) {
        jpeg_destroy_decompress(&state->decoder);
        if (state->texture) vita2d_free_texture(state->texture);
        free(state->row); delete state; return nullptr;
    }
    jpeg_create_decompress(&state->decoder); jpeg_stdio_src(&state->decoder,file);
    jpeg_read_header(&state->decoder,TRUE);
    if (!dimensions(state->decoder.image_width,state->decoder.image_height)) { jpeg_destroy_decompress(&state->decoder); delete state; return nullptr; }
    state->decoder.out_color_space=JCS_RGB;
    jpeg_start_decompress(&state->decoder);
    state->texture=vita2d_create_empty_texture(state->decoder.output_width,state->decoder.output_height);
    state->row=static_cast<unsigned char*>(malloc(state->decoder.output_width*3));
    if (!state->texture || !state->row) { jpeg_destroy_decompress(&state->decoder); if (state->texture) vita2d_free_texture(state->texture); free(state->row); delete state; return nullptr; }
    auto pixels=static_cast<unsigned char*>(vita2d_texture_get_datap(state->texture));
    const auto stride=vita2d_texture_get_stride(state->texture);
    while (state->decoder.output_scanline<state->decoder.output_height) {
        const auto y=state->decoder.output_scanline;
        JSAMPROW row=state->row;
        if (jpeg_read_scanlines(&state->decoder,&row,1)!=1) jpeg_failure(reinterpret_cast<j_common_ptr>(&state->decoder));
        for (unsigned x=0;x<state->decoder.output_width;++x) {
            auto out=pixels+y*stride+x*4;
            memcpy(out,row+x*3,3); out[3]=255;
        }
    }
    jpeg_finish_decompress(&state->decoder); jpeg_destroy_decompress(&state->decoder);
    auto result=state->texture; free(state->row); delete state; return result;
}
}
vita2d_texture* load_media_image(const std::string& path) {
    auto file=fopen(path.c_str(),"rb"); if (!file) return nullptr;
    fseek(file,0,SEEK_END); const auto bytes=ftell(file); rewind(file);
    unsigned char signature[8]{};
    if (bytes<=0 || bytes>8*1024*1024 || fread(signature,1,8,file)!=8) { fclose(file); return nullptr; }
    rewind(file);
    vita2d_texture* texture=nullptr;
    if (signature[0]==0xff && signature[1]==0xd8) texture=jpeg_image(file);
    else if (!png_sig_cmp(signature,0,8)) {
        png_image image{}; image.version=PNG_IMAGE_VERSION;
        if (png_image_begin_read_from_stdio(&image,file) && dimensions(image.width,image.height)) {
            image.format=PNG_FORMAT_RGBA;
            texture=vita2d_create_empty_texture(image.width,image.height);
            if (texture && !png_image_finish_read(&image,nullptr,vita2d_texture_get_datap(texture),vita2d_texture_get_stride(texture),nullptr)) {
                vita2d_free_texture(texture); texture=nullptr;
            }
        }
        png_image_free(&image);
    }
    fclose(file);
    if (texture) vita2d_texture_set_filters(texture,SCE_GXM_TEXTURE_FILTER_LINEAR,SCE_GXM_TEXTURE_FILTER_LINEAR);
    return texture;
}
}

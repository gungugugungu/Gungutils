//
// Created by gungu on 8/26/25.
//

#ifndef SURFACE_H
#define SURFACE_H

class Surface {
public:
    vector<vector<HMM_Vec4>> pixels;
    HMM_Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    mutable vector<uint8_t> sokol_data_u8;

    void clear(int w, int h, HMM_Vec4 color = {1.0f, 1.0f, 1.0f, 0.0f}) {
        pixels.resize(h);
        for (int i = 0; i < h; i++) {
            pixels[i].resize(w);
            for (int j = 0; j < w; j++) {
                pixels[i][j] = color;
            }
        }
    }

    void draw(const Surface& other_surf, HMM_Vec2 pos) {
        int dest_x = static_cast<int>(pos.X);
        int dest_y = static_cast<int>(pos.Y);

        if (pixels.empty() || pixels[0].empty() || other_surf.pixels.empty() || other_surf.pixels[0].empty()) return;

        int dest_height = pixels.size();
        int dest_width = pixels[0].size();
        int src_height = other_surf.pixels.size();
        int src_width = other_surf.pixels[0].size();

        for (int src_y = 0; src_y < src_height; src_y++) {
            for (int src_x = 0; src_x < src_width; src_x++) {
                int target_x = dest_x + src_x;
                int target_y = dest_y + (src_height - 1 - src_y);

                if (target_x >= 0 && target_x < dest_width && target_y >= 0 && target_y < dest_height) {
                    HMM_Vec4 src_pixel = other_surf.pixels[src_y][src_x];
                    HMM_Vec4 final_pixel = {
                        src_pixel.X * other_surf.color.X,
                        src_pixel.Y * other_surf.color.Y,
                        src_pixel.Z * other_surf.color.Z,
                        src_pixel.W * other_surf.color.W
                    };

                    pixels[target_y][target_x] = final_pixel;
                }
            }
        }
    }

    void draw_text(const stbtt_fontinfo* font, const string& text, HMM_Vec2 pos, float scale, HMM_Vec4 text_color = {1.0f, 1.0f, 1.0f, 1.0f}) {
        if (pixels.empty() || pixels[0].empty() || !font) return;

        int dest_width = pixels[0].size();
        int dest_height = pixels.size();

        float x = pos.X;
        float baseline_y = pos.Y;

        int ascent, descent, line_gap;
        stbtt_GetFontVMetrics(font, &ascent, &descent, &line_gap);
        float scaled_ascent = ascent * scale;
        float scaled_descent = descent * scale;

        float total_width = 0.0f;
        for (size_t i = 0; i < text.length(); i++) {
            int advance, lsb;
            stbtt_GetCodepointHMetrics(font, text[i], &advance, &lsb);
            total_width += advance * scale;

            if (i < text.length() - 1) {
                int kern = stbtt_GetCodepointKernAdvance(font, text[i], text[i + 1]);
                total_width += kern * scale;
            }
        }

        x += total_width;

        for (int i = (int)text.length() - 1; i >= 0; i--) {
            int advance, lsb;
            stbtt_GetCodepointHMetrics(font, text[i], &advance, &lsb);

            x -= advance * scale;

            if (i > 0) {
                int kern = stbtt_GetCodepointKernAdvance(font, text[i-1], text[i]);
                x -= kern * scale;
            }

            int glyph_width, glyph_height, glyph_xoff, glyph_yoff;
            unsigned char* glyph_bitmap = stbtt_GetCodepointBitmap(font, 0, scale, text[i], &glyph_width, &glyph_height, &glyph_xoff, &glyph_yoff);

            if (glyph_bitmap) {
                int start_x = (int)(x + glyph_xoff);
                int start_y = (int)(baseline_y - scaled_descent - glyph_yoff - glyph_height);

                for (int gy = 0; gy < glyph_height; gy++) {
                    for (int gx = 0; gx < glyph_width; gx++) {
                        int target_x = start_x + gx;
                        int target_y = start_y + gy;

                        if (target_x >= 0 && target_x < dest_width && target_y >= 0 && target_y < dest_height) {
                            float alpha = glyph_bitmap[gy * glyph_width + gx] / 255.0f;

                            if (alpha > 0.0f) {
                                HMM_Vec4 bg_pixel = pixels[target_y][target_x];
                                HMM_Vec4 final_pixel = {
                                    bg_pixel.X * (1.0f - alpha) + text_color.X * alpha,
                                    bg_pixel.Y * (1.0f - alpha) + text_color.Y * alpha,
                                    bg_pixel.Z * (1.0f - alpha) + text_color.Z * alpha,
                                    bg_pixel.W * (1.0f - alpha) + text_color.W * alpha
                                };
                                pixels[target_y][target_x] = final_pixel;
                            }
                        }
                    }
                }

                stbtt_FreeBitmap(glyph_bitmap, nullptr);
            }
        }
    }

    void load_from_file(string path) {
        int img_width = 0, img_height = 0, num_channels = 0;
        const int desired_channels = 4;
        stbi_uc* stb_pixels = stbi_load(path.c_str(), &img_width, &img_height, &num_channels, desired_channels);

        if (!stb_pixels) {
            return;
        }

        pixels.resize(img_height);
        for (int i = 0; i < img_height; i++) {
            pixels[i].resize(img_width);
            for (int j = 0; j < img_width; j++) {
                int idx = (i * img_width + j) * 4;
                pixels[i][j] = {
                    stb_pixels[idx] / 255.0f,
                    stb_pixels[idx + 1] / 255.0f,
                    stb_pixels[idx + 2] / 255.0f,
                    stb_pixels[idx + 3] / 255.0f
                };
            }
        }

        stbi_image_free(stb_pixels);
    }

    void resize(int new_width, int new_height) {
        if (pixels.empty() || pixels[0].empty()) return;

        int old_width = pixels[0].size();
        int old_height = pixels.size();

        if (old_width == new_width && old_height == new_height) return;

        vector<uint8_t> input_data(old_width * old_height * 4);
        for (int y = 0; y < old_height; y++) {
            for (int x = 0; x < old_width; x++) {
                int idx = (y * old_width + x) * 4;
                input_data[idx] = static_cast<uint8_t>(pixels[y][x].X * 255.0f);
                input_data[idx + 1] = static_cast<uint8_t>(pixels[y][x].Y * 255.0f);
                input_data[idx + 2] = static_cast<uint8_t>(pixels[y][x].Z * 255.0f);
                input_data[idx + 3] = static_cast<uint8_t>(pixels[y][x].W * 255.0f);
            }
        }

        vector<uint8_t> output_data(new_width * new_height * 4);

        stbir_resize_uint8_linear(
            input_data.data(), old_width, old_height, 0,
            output_data.data(), new_width, new_height, 0,
            STBIR_RGBA
        );

        pixels.resize(new_height);
        for (int y = 0; y < new_height; y++) {
            pixels[y].resize(new_width);
            for (int x = 0; x < new_width; x++) {
                int idx = (y * new_width + x) * 4;
                pixels[y][x] = {
                    output_data[idx] / 255.0f,
                    output_data[idx + 1] / 255.0f,
                    output_data[idx + 2] / 255.0f,
                    output_data[idx + 3] / 255.0f
                };
            }
        }
    }

    void resize_percentage(float width_percent, float height_percent) {
        if (pixels.empty() || pixels[0].empty()) return;

        int old_width = pixels[0].size();
        int old_height = pixels.size();

        int new_width = static_cast<int>(old_width * width_percent);
        int new_height = static_cast<int>(old_height * height_percent);

        if (new_width <= 0) new_width = 1;
        if (new_height <= 0) new_height = 1;

        resize(new_width, new_height);
    }

    sg_image_data get_sokol_image_data() const {
        sg_image_data img_data = {};

        if (pixels.empty() || pixels[0].empty()) {
            return img_data;
        }

        int height = pixels.size();
        int width = pixels[0].size();
        sokol_data_u8.resize(height * width * 4);

        for (int i = 0; i < height; i++) {
            int src_y = height - 1 - i;
            for (int j = 0; j < width; j++) {
                int idx = (i * width + j) * 4;
                sokol_data_u8[idx] = static_cast<uint8_t>(pixels[src_y][j].X * 255.0f);
                sokol_data_u8[idx + 1] = static_cast<uint8_t>(pixels[src_y][j].Y * 255.0f);
                sokol_data_u8[idx + 2] = static_cast<uint8_t>(pixels[src_y][j].Z * 255.0f);
                sokol_data_u8[idx + 3] = static_cast<uint8_t>(pixels[src_y][j].W * 255.0f);
            }
        }

        img_data.subimage[0][0] = {sokol_data_u8.data(), sokol_data_u8.size()};

        return img_data;
    }

    sg_image_data get_sokol_image_data_unflipped() const {
        sg_image_data img_data = {};

        if (pixels.empty() || pixels[0].empty()) {
            return img_data;
        }

        int height = pixels.size();
        int width = pixels[0].size();
        sokol_data_u8.resize(height * width * 4);

        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                int idx = (i * width + j) * 4;
                sokol_data_u8[idx] = static_cast<uint8_t>(pixels[i][j].X * 255.0f);
                sokol_data_u8[idx + 1] = static_cast<uint8_t>(pixels[i][j].Y * 255.0f);
                sokol_data_u8[idx + 2] = static_cast<uint8_t>(pixels[i][j].Z * 255.0f);
                sokol_data_u8[idx + 3] = static_cast<uint8_t>(pixels[i][j].W * 255.0f);
            }
        }

        img_data.subimage[0][0] = {sokol_data_u8.data(), sokol_data_u8.size()};

        return img_data;
    }
};

Surface from_text(const stbtt_fontinfo* font, const std::string& text, float scale, HMM_Vec4 text_color = {1.0f, 1.0f, 1.0f, 1.0f}) {
    Surface surf;
    if (!font || text.empty()) return surf;

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &line_gap);
    float scaled_ascent = static_cast<float>(ascent) * scale;
    float scaled_descent = static_cast<float>(descent) * scale;
    float line_height = scaled_ascent - scaled_descent;
    int height = static_cast<int>(std::ceil(line_height));

    float total_width = 0.0f;
    for (size_t i = 0; i < text.size(); ++i) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(font, text[i], &advance, &lsb);
        total_width += static_cast<float>(advance) * scale;
        if (i + 1 < text.size()) {
            total_width += static_cast<float>(stbtt_GetCodepointKernAdvance(font, text[i], text[i + 1])) * scale;
        }
    }
    int width = static_cast<int>(std::ceil(total_width));
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    surf.clear(width, height, {0.0f, 0.0f, 0.0f, 0.0f});
    surf.draw_text(font, text, {0.0f, 0.0f}, scale, text_color);
    return surf;
}

#endif //SURFACE_H
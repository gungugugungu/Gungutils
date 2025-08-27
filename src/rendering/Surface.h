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
                int target_y = dest_y + src_y;

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

#endif //SURFACE_H
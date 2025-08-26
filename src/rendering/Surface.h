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

    void initialize(int w, int h) {
        pixels.resize(h);
        for (int i = 0; i < h; i++) {
            pixels[i].resize(w);
            for (int j = 0; j < w; j++) {
                pixels[i][j] = {1.0f, 1.0f, 0.0f, 0.0f};
            }
        }
    }

    void draw(Surface other_surf, HMM_Vec2 pos) {
        for (int i = 0; i < other_surf.pixels.size(); i++) {
            for (int j = 0; j < other_surf.pixels[i].size(); j++) {
                HMM_Vec4 pixel = other_surf.pixels[i][j];
                pixel.X *= other_surf.color.X;
                pixel.Y *= other_surf.color.Y;
                pixel.Z *= other_surf.color.Z;
                pixel.W *= other_surf.color.W;
                pixels[i+static_cast<int>(pos.X)][j+static_cast<int>(pos.Y)] = pixel;
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
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <limits>
#include <SDL.h>

int width = 400;
int height = 400;
bool running = true;
bool solid = true;

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Vec2 {
    float x;
    float y;
};

class Light {
public:
    Vec3 position;
    float intensity;
    Light(Vec3 position, float intensity = 1.0f)
        : position(position), intensity(intensity) {}
};

void quit(SDL_Window* window, SDL_Renderer* renderer) {
    if (window) {
        SDL_DestroyWindow(window);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    SDL_Quit();
}

uint32_t color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return ((uint32_t) r << 24) | ((uint32_t) g << 16) | ((uint32_t) b << 8) | a;
}

void drawPixel(std::vector<uint32_t> &pixels, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        pixels[y * width + x] = color(r, g, b);
    }
}

void drawLine(std::vector<uint32_t>& pixels, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int error = dx + dy;
    while (true) {
        drawPixel(pixels, x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * error;
        if (e2 >= dy) {
            error += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void clear(std::vector<uint32_t> &pixels) {
    for (int i = 0; i < width * height; ++i) {
        pixels[i] = color(0, 0, 0, 0);
    }
}

float edge(Vec2 p1, Vec2 p2, Vec2 p3) {
    return (p3.x - p1.x) * (p2.y - p1.y) - (p3.y - p1.y) * (p2.x - p1.x);
}

void drawTriangle(std::vector<uint32_t>& pixels, std::vector<float>& depthBuffer, std::vector<Light>& lights, Vec2 p1, Vec2 p2, Vec2 p3, Vec3 transA, Vec3 transB, Vec3 transC, uint8_t r, uint8_t g, uint8_t b) {
    Vec3 edge1 = {transB.x - transA.x, transB.y - transA.y, transB.z - transA.z};
    Vec3 edge2 = {transC.x - transA.x, transC.y - transA.y, transC.z - transA.z};

    Vec3 normal = {
        edge1.y * edge2.z - edge1.z * edge2.y,
        edge1.z * edge2.x - edge1.x * edge2.z,
        edge1.x * edge2.y - edge1.y * edge2.x
    };

    float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (len > 0.0f) { normal.x /= len; normal.y /= len; normal.z /= len; }

    float ambient = 0.15f;
    float diffuse = 0.0f;

    Vec3 faceCenter = {
        (transA.x + transB.x + transC.x) / 3.0f,
        (transA.y + transB.y + transC.y) / 3.0f,
        (transA.z + transB.z + transC.z) / 3.0f
    };

    for (const auto& light : lights) {
        Vec3 lightDir = {light.position.x - faceCenter.x, light.position.y - faceCenter.y, light.position.z - faceCenter.z };
        float lightDist = std::sqrt(lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z);
        if (lightDist > 0.0f) { lightDir.x /= lightDist; lightDir.y /= lightDist; lightDir.z /= lightDist; }

        float dotProd = normal.x * lightDir.x + normal.y * lightDir.y + normal.z * lightDir.z;
        float intensity = std::max(dotProd, 0.0f) * light.intensity;
        diffuse += intensity;
    }

    float totalLighting = std::min(ambient + diffuse, 1.0f);
    uint8_t finalR = static_cast<uint8_t>(r * std::pow(totalLighting, 0.165));
    uint8_t finalG = static_cast<uint8_t>(g * std::pow(totalLighting, 0.165));
    uint8_t finalB = static_cast<uint8_t>(b * std::pow(totalLighting, 0.165));

    float minX = std::min({p1.x, p2.x, p3.x});
    float maxX = std::max({p1.x, p2.x, p3.x});
    float minY = std::min({p1.y, p2.y, p3.y});
    float maxY = std::max({p1.y, p2.y, p3.y});

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            Vec2 p = {
                static_cast<float>(x),
                static_cast<float>(y)
            };
            float w1 = edge(p2, p3, p);
            float w2 = edge(p3, p1, p);
            float w3 = edge(p1, p2, p);
            if ((w1 >= 0 && w2 >= 0 && w3 >= 0) ||
                (w1 <= 0 && w2 <= 0 && w3 <= 0))
            {
                float area = edge(p1, p2, p3);
                float alpha = w1 / area;
                float beta = w2 / area;
                float gamma = w3 / area;
                float z = alpha * transA.z + beta * transB.z + gamma * transC.z;
                int index = y * width + x;
                if (z < depthBuffer[index]) {
                    depthBuffer[index] = z;
                    drawPixel(pixels, x, y, finalR, finalG, finalB);
                }
            }
        }
    }
}

Vec3 rotateY(Vec3 p, float angle) {
    Vec3 result;
    result.x = p.x * std::cos(angle) - p.z * std::sin(angle);
    result.y = p.y;
    result.z = p.x * std::sin(angle) + p.z * std::cos(angle);
    return result;
}

Vec2 project(Vec3 p) {
    float focalLength = 500.0f;
    Vec2 result;
    result.x = (p.x / p.z) * focalLength;
    result.y = (p.y / p.z) * focalLength;
    result.x += width / 2.0f;
    result.y = height / 2.0f - result.y;
    return result;
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "Failed to Init SDL. " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Renderer* renderer = nullptr;
    SDL_Window* window = nullptr;
    window = SDL_CreateWindow("Spinning Cube", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_BORDERLESS);
    if (!window) {
        std::cout << "Failed to Init SDL Window. " << SDL_GetError() << '\n';
        quit(window, renderer);
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cout << "Failed to Init SDL Renderer. " << SDL_GetError() << '\n';
        quit(window, renderer);
        return 1;
    }

    SDL_Texture* pixelBufferTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );
    std::vector<uint32_t> pixels(width * height);
    std::vector<float> depthBuffer(width * height);
    Vec3 cube[8] = {
        {-1, -1, -1},
        { 1, -1, -1},
        { 1,  1, -1},
        {-1,  1, -1},

        {-1, -1,  1},
        { 1, -1,  1},
        { 1,  1,  1},
        {-1,  1,  1}
    };
    int triangles[12][3] = {
        {0, 1, 2},
        {0, 2, 3},
        {4, 6, 5},
        {4, 7, 6},
        {0, 3, 7},
        {0, 7, 4},
        {1, 5, 6},
        {1, 6, 2},
        {0, 4, 5},
        {0, 5, 1},
        {3, 2, 6},
        {3, 6, 7}
    };
    int triangleColors[12][3] = {
        {255, 0, 0},
        {255, 0, 0},

        {0, 255, 0},
        {0, 255, 0},

        {0, 0, 255},
        {0, 0, 255},

        {255, 255, 0},
        {255, 255, 0},

        {0, 255, 255},
        {0, 255, 255},

        {255, 0, 255},
        {255, 0, 255}
    };
    std::vector<Light> lights;
    lights.push_back(Light({2.5f, 3.0f, 3.5f}, 1.0f));
    float angle = 0.0f;
    SDL_Event e;
    while (running) {
        std::fill(depthBuffer.begin(), depthBuffer.end(), std::numeric_limits<float>::infinity());
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                    case SDLK_TAB:
                        solid = !solid;
                }
            }
        }
        angle += 0.01f;
        clear(pixels);
        Vec3 transformed[8];
        Vec2 projected[8];
        for (int i = 0; i < 8; ++i) {
            transformed[i] = rotateY(cube[i], angle);
            transformed[i].z += 6.0f;
            projected[i] = project(transformed[i]);
        }
        if (!solid) {
            for (int i = 0; i < 8; ++i) {
                if (!solid) {
                    drawPixel(
                        pixels,
                        static_cast<int>(projected[i].x),
                        static_cast<int>(projected[i].y),
                        255, 255, 255
                    );
                    for (int x = 0; x < 8; ++x) {
                        if (x != i) {
                            drawLine(pixels, static_cast<int>(projected[i].x), static_cast<int>(projected[i].y), static_cast<int>(projected[x].x), static_cast<int>(projected[x].y), 255, 255, 255);
                        }
                    }
                }
            }
        } else {
            for (int i = 0; i < 12; ++i) {
                int a = triangles[i][0];
                int b = triangles[i][1];
                int c = triangles[i][2];
                int colorR = triangleColors[i][0];
                int colorG = triangleColors[i][1];
                int colorB = triangleColors[i][2];

                drawTriangle(
                    pixels,
                    depthBuffer,
                    lights,
                    projected[a],
                    projected[b],
                    projected[c],
                    transformed[a],
                    transformed[b],
                    transformed[c],
                    colorR,
                    colorG,
                    colorB
                );
            }
        }
        SDL_UpdateTexture(pixelBufferTexture, nullptr, pixels.data(), width * sizeof(uint32_t));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, pixelBufferTexture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
    quit(window, renderer);
    return 0;
}

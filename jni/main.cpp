#include <GLES/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include "hook.h"

// 简化的Android Log宏
#define LOG_TAG "4399_ESP"
#define LOGD(fmt, ...) printf("[%s] " fmt "\n", LOG_TAG, ##__VA_ARGS__)

struct Vector3 {
    float x, y, z;
};

struct Vector2 {
    float x, y;
};

struct Matrix4x4 {
    float m[16];
};

struct PlayerObject {
    Vector3 position;
    float hp;
    float max_hp;
    Vector2 screen_pos;
    bool is_visible;
};

std::vector<PlayerObject> g_players;
int g_screen_width = 1080;
int g_screen_height = 2400;
bool g_esp_enabled = true;

uintptr_t g_libunity_base = 0;
uintptr_t g_libil2cpp_base = 0;

// 获取so基址
uintptr_t GetModuleBase(const char* module_name) {
    char line[512];
    uintptr_t base = 0;
    
    FILE* fp = fopen("/proc/self/maps", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, module_name)) {
                sscanf(line, "%lx", &base);
                break;
            }
        }
        fclose(fp);
    }
    return base;
}

// 读取指针链
uintptr_t ReadPointerChain(uintptr_t base, const std::vector<uintptr_t>& offsets) {
    uintptr_t addr = base;
    for (size_t i = 0; i < offsets.size(); i++) {
        if (addr == 0) return 0;
        addr += offsets[i];
        if (i < offsets.size() - 1) {
            uintptr_t next = 0;
            memcpy(&next, (void*)addr, sizeof(uintptr_t));
            addr = next;
        }
    }
    return addr;
}

// 获取视图矩阵
Matrix4x4 GetViewProjectionMatrix() {
    Matrix4x4 matrix = {0};
    
    std::vector<uintptr_t> offsets = {0x60568, 0x20, 0x8, 0x30C};
    uintptr_t matrix_addr = ReadPointerChain(g_libunity_base, offsets);
    
    if (matrix_addr != 0) {
        memcpy(&matrix, (void*)matrix_addr, sizeof(Matrix4x4));
    }
    
    return matrix;
}

// 3D转2D坐标
bool WorldToScreen(const Vector3& world_pos, const Matrix4x4& matrix, Vector2& screen_pos) {
    float w = matrix.m[3] * world_pos.x + matrix.m[7] * world_pos.y + 
              matrix.m[11] * world_pos.z + matrix.m[15];
    
    if (w < 0.01f) return false;
    
    float x = matrix.m[0] * world_pos.x + matrix.m[4] * world_pos.y + 
              matrix.m[8] * world_pos.z + matrix.m[12];
    float y = matrix.m[1] * world_pos.x + matrix.m[5] * world_pos.y + 
              matrix.m[9] * world_pos.z + matrix.m[13];
    
    x /= w;
    y /= w;
    
    screen_pos.x = (x + 1.0f) * 0.5f * g_screen_width;
    screen_pos.y = (1.0f - y) * 0.5f * g_screen_height;
    
    return (screen_pos.x >= 0 && screen_pos.x <= g_screen_width &&
            screen_pos.y >= 0 && screen_pos.y <= g_screen_height);
}

// 读取玩家数据
void ReadPlayerData() {
    if (g_libil2cpp_base == 0) return;
    
    g_players.clear();
    
    std::vector<uintptr_t> array_offsets = {0x1A618, 0x6B0, 0xB8, 0xD0, 0x20};
    uintptr_t array_addr = ReadPointerChain(g_libil2cpp_base, array_offsets);
    
    if (array_addr == 0) return;
    
    int array_size = 0;
    memcpy(&array_size, (void*)(array_addr - 4), sizeof(int));
    
    if (array_size <= 0 || array_size > 100) return;
    
    Matrix4x4 view_matrix = GetViewProjectionMatrix();
    
    for (int i = 0; i < array_size; i++) {
        uintptr_t obj_ptr = 0;
        memcpy(&obj_ptr, (void*)(array_addr + i * 8), sizeof(uintptr_t));
        
        if (obj_ptr == 0) continue;
        
        PlayerObject player;
        memcpy(&player.position.x, (void*)(obj_ptr + 0x3CC), sizeof(float));
        memcpy(&player.position.z, (void*)(obj_ptr + 0x3D0), sizeof(float));
        memcpy(&player.position.y, (void*)(obj_ptr + 0x3D4), sizeof(float));
        memcpy(&player.hp, (void*)(obj_ptr + 0x118), sizeof(float));
        memcpy(&player.max_hp, (void*)(obj_ptr + 0x3C), sizeof(float));
        
        player.is_visible = WorldToScreen(player.position, view_matrix, player.screen_pos);
        
        if (player.is_visible && player.hp > 0 && player.hp <= player.max_hp) {
            g_players.push_back(player);
        }
    }
}

// 绘制函数
void DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a) {
    glDisable(GL_TEXTURE_2D);
    glLineWidth(3.0f);
    
    GLfloat vertices[] = {
        x1 / g_screen_width * 2.0f - 1.0f, 1.0f - y1 / g_screen_height * 2.0f,
        x2 / g_screen_width * 2.0f - 1.0f, 1.0f - y2 / g_screen_height * 2.0f
    };
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glColor4f(r, g, b, a);
    glDrawArrays(GL_LINES, 0, 2);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void DrawBox(float x, float y, float width, float height, float r, float g, float b, float a) {
    DrawLine(x, y, x + width, y, r, g, b, a);
    DrawLine(x + width, y, x + width, y + height, r, g, b, a);
    DrawLine(x + width, y + height, x, y + height, r, g, b, a);
    DrawLine(x, y + height, x, y, r, g, b, a);
}

void DrawFilledRect(float x, float y, float width, float height, float r, float g, float b, float a) {
    glDisable(GL_TEXTURE_2D);
    
    GLfloat vertices[] = {
        x / g_screen_width * 2.0f - 1.0f, 1.0f - y / g_screen_height * 2.0f,
        (x + width) / g_screen_width * 2.0f - 1.0f, 1.0f - y / g_screen_height * 2.0f,
        x / g_screen_width * 2.0f - 1.0f, 1.0f - (y + height) / g_screen_height * 2.0f,
        (x + width) / g_screen_width * 2.0f - 1.0f, 1.0f - (y + height) / g_screen_height * 2.0f
    };
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glColor4f(r, g, b, a);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void DrawHealthBar(float x, float y, float width, float hp_percent) {
    DrawFilledRect(x - width/2, y - 5, width, 8, 0.2f, 0.2f, 0.2f, 0.8f);
    
    float r = 1.0f - hp_percent;
    float g = hp_percent;
    DrawFilledRect(x - width/2 + 2, y - 3, (width - 4) * hp_percent, 4, r, g, 0.0f, 1.0f);
}

// ESP主绘制
void DrawESP() {
    if (!g_esp_enabled) return;
    
    ReadPlayerData();
    
    for (const auto& player : g_players) {
        if (!player.is_visible) continue;
        
        float box_height = 180.0f;
        float box_width = 80.0f;
        
        // 方框
        DrawBox(player.screen_pos.x - box_width/2, 
                player.screen_pos.y - box_height/2,
                box_width, box_height, 
                1.0f, 0.0f, 0.0f, 1.0f);
        
        // 血条
        float hp_percent = player.hp / player.max_hp;
        if (hp_percent > 1.0f) hp_percent = 1.0f;
        DrawHealthBar(player.screen_pos.x, player.screen_pos.y - box_height/2 - 15, box_width, hp_percent);
        
        // 射线
        DrawLine(g_screen_width/2, g_screen_height, 
                player.screen_pos.x, player.screen_pos.y,
                0.0f, 1.0f, 1.0f, 0.6f);
    }
}

// Hook函数
typedef void (*eglSwapBuffers_t)(void* dpy, void* surface);
eglSwapBuffers_t orig_eglSwapBuffers = nullptr;

void hook_eglSwapBuffers(void* dpy, void* surface) {
    DrawESP();
    orig_eglSwapBuffers(dpy, surface);
}

// 初始化
void InitESP() {
    sleep(5); // 等待游戏加载
    
    LOGD("[+] ESP Init started");
    
    g_libunity_base = GetModuleBase("libunity.so");
    g_libil2cpp_base = GetModuleBase("libil2cpp.so");
    
    LOGD("[+] libunity.so base: 0x%lx", g_libunity_base);
    LOGD("[+] libil2cpp.so base: 0x%lx", g_libil2cpp_base);
    
    void* egl_swap = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
    if (egl_swap) {
        if (InlineHook(egl_swap, (void*)hook_eglSwapBuffers, (void**)&orig_eglSwapBuffers)) {
            LOGD("[+] eglSwapBuffers hooked!");
        } else {
            LOGD("[-] Hook failed!");
        }
    } else {
        LOGD("[-] eglSwapBuffers not found!");
    }
}

// 入口
__attribute__((constructor))
void lib_main() {
    LOGD("[*] 4399 ESP loaded!");
    
    pthread_t thread;
    pthread_create(&thread, nullptr, (void*(*)(void*))InitESP, nullptr);
    pthread_detach(thread);
}

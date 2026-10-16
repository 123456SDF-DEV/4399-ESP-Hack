#include "hook.h"
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

// ARM64 inline hook实现
bool InlineHook(void* target_addr, void* replace_func, void** origin_func) {
    if (!target_addr || !replace_func) return false;
    
    // 获取页面大小
    size_t page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page_start = ((uintptr_t)target_addr) & ~(page_size - 1);
    
    // 修改内存保护
    if (mprotect((void*)page_start, page_size * 2, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        return false;
    }
    
    // 保存原始指令（前16字节）
    uint32_t* original_instructions = new uint32_t[4];
    memcpy(original_instructions, target_addr, 16);
    
    // 构建跳转代码（ARM64）
    // LDR X9, #8
    // BR X9
    // [address]
    uint32_t* instructions = (uint32_t*)target_addr;
    instructions[0] = 0x58000049;  // LDR X9, #8
    instructions[1] = 0xD61F0120;  // BR X9
    *(uint64_t*)(instructions + 2) = (uint64_t)replace_func;
    
    // 清除指令缓存
    __builtin___clear_cache((char*)target_addr, (char*)target_addr + 16);
    
    // 如果需要原始函数，创建跳板
    if (origin_func) {
        void* trampoline = mmap(NULL, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (trampoline == MAP_FAILED) {
            delete[] original_instructions;
            return false;
        }
        
        uint32_t* tramp = (uint32_t*)trampoline;
        
        // 复制原始指令
        memcpy(tramp, original_instructions, 16);
        
        // 添加跳回原函数的指令
        tramp[4] = 0x58000049;  // LDR X9, #8
        tramp[5] = 0xD61F0120;  // BR X9
        *(uint64_t*)(tramp + 6) = (uint64_t)target_addr + 16;
        
        __builtin___clear_cache((char*)trampoline, (char*)trampoline + page_size);
        
        *origin_func = trampoline;
    }
    
    delete[] original_instructions;
    
    // 恢复只读保护
    mprotect((void*)page_start, page_size * 2, PROT_READ | PROT_EXEC);
    
    return true;
}
#ifndef HOOK_H
#define HOOK_H

#include <stdint.h>
#include <stdbool.h>

// Inline Hook函数
// target_addr: 要hook的目标函数地址
// replace_func: 替换函数地址
// origin_func: 输出参数，保存原始函数跳板地址
bool InlineHook(void* target_addr, void* replace_func, void** origin_func);

#endif // HOOK_H
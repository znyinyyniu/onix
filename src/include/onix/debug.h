
#ifndef ONIX_DEBUG_H
#define ONIX_DEBUG_H

// 编译期日志级别（makefile 通过 -DONIX_LOG_LEVEL=N 传入）
#ifndef ONIX_LOG_LEVEL
#define ONIX_LOG_LEVEL 4
#endif

#define LOG_LVL_ERROR 1
#define LOG_LVL_WARN  2
#define LOG_LVL_INFO  3
#define LOG_LVL_DEBUG 4
#define LOG_LVL_TRACE 5

void debugk(char *file, int line, const char *fmt, ...);

#define BMB asm volatile("xchgw %bx, %bx") // bochs magic breakpoint

#if ONIX_LOG_LEVEL >= LOG_LVL_ERROR
#define LOGE(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)
#else
#define LOGE(fmt, args...) ((void)0)
#endif

#if ONIX_LOG_LEVEL >= LOG_LVL_WARN
#define LOGW(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)
#else
#define LOGW(fmt, args...) ((void)0)
#endif

#if ONIX_LOG_LEVEL >= LOG_LVL_INFO
#define LOGI(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)
#else
#define LOGI(fmt, args...) ((void)0)
#endif

#if ONIX_LOG_LEVEL >= LOG_LVL_DEBUG
#define LOGK(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)
#define DEBUGK(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)
#else
#define LOGK(fmt, args...) ((void)0)
#define DEBUGK(fmt, args...) ((void)0)
#endif

#if ONIX_LOG_LEVEL >= LOG_LVL_TRACE
#define LOG_TRACE(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)
#else
#define LOG_TRACE(fmt, args...) ((void)0)
#endif

#endif

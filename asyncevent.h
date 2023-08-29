#ifndef __ASYNCEVENT_H__
#define __ASYNCEVENT_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "list.h"

#define USE_MUTI_THREAD 1 /* 是否使用多线程 */

/**
 * 信号与回调函数结构体
*/
typedef struct eventinfo_t
{
    void (*func)(void* args); /* 事件回调函数 */
    int sig;                  /* 信号值 */  
}eventinfo_t;

/**
 * 信号链表结构体
*/
typedef struct eventlist_t
{
    void *args;            /* 传递的参数 */
    int sig;               /* 信号值 */
    struct list_head list; /* 双向链表 */
}eventlist_t;

/**
 * 异步事件结构体
*/
typedef struct asyncevent_t
{
    struct list_head hlist; /* 信号链表头 */
    eventinfo_t map[1024]; /* 信号与函数映射 */
    #if USE_MUTI_THREAD
    pthread_mutex_t lock; /* 互斥锁 */
    pthread_cond_t cond;  /* 条件变量 */
    #endif
}asyncevent_t;


asyncevent_t* create_async_event(void);
int async_event_emit(asyncevent_t* handle, int priority, int sig, void* args);
int async_event_bind(asyncevent_t* handle, int sig, void (*func)(void* args));
int async_event_process(asyncevent_t* handle);
int async_event_destory(asyncevent_t* handle);

#endif // !__ASYNCEVENT_H__

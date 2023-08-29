#include "asyncevent.h"

/**
 * @brief:  异步事件绑定信号和回调函数
 * @handle: 事件句柄
 * @sig:  信号值
 * @func: 处理该信号的函数
 * @return: 0:成功
*/
int async_event_bind(asyncevent_t* handle, int sig, void (*func)(void* args))//绑定信号和函数
{
    if(!handle || !func) return -1;     /* 句柄不存在 */
    if(sig<0 || sig>1024) return -2;    /* 信号值超过有效范围 */
    if(handle->map[sig].func) return -3;/* 信号以及被绑定过了 */

    handle->map[sig].func = func;       /* 绑定函数 */
    handle->map[sig].sig = sig;         /* 绑定信号 */

    return 0;
}

/**
 * @brief: 发射信号，触发事件
 * @priority: 0:添加到链表尾部 非0:添加到链表头部(能及时响应)
 * @sig: 对应的信号
 * @args: 传递的参数
 * @return: 0:成功
*/
int async_event_emit(asyncevent_t* handle, int priority, int sig, void* args) 
{
    if(!handle) return -1;              /* 事件句柄不存在 */
    if(sig<0 || sig>1024) return -2;    /* 信号值超过有效范围 */
    if(!handle->map[sig].func) return -3; /* 该信号未绑定,不能触发事件 */
     
    eventlist_t* node = (eventlist_t*)malloc(sizeof(eventlist_t));
    if(!node) return -1;
    node->args = args;
    node->sig = sig;

    #if USE_MUTI_THREAD
    pthread_mutex_lock(&handle->lock);
    #endif

    if(priority)
        list_add(&node->list, &handle->hlist);      /* 往头部添加 */
    else
        list_add_tail(&node->list, &handle->hlist); /* 往尾部添加 */
    #if USE_MUTI_THREAD
    pthread_cond_signal(&handle->cond);
    pthread_mutex_unlock(&handle->lock);
    #endif

    return 0;
}

/**
 * @brief: 调度处理所有的事件任务
 * @handle: 事件句柄
 * @return: 0:成功
*/
int async_event_process(asyncevent_t* handle) //调度处理
{
    if(!handle) return -1;
    struct list_head *pos, *n;
    eventlist_t *node;
    int sig = 0;

    #if USE_MUTI_THREAD
    pthread_mutex_lock(&handle->lock);
    while (list_empty(&handle->hlist)){
        pthread_cond_wait(&handle->cond, &handle->lock);
    }
    pthread_mutex_unlock(&handle->lock);
    #endif

    list_for_each_safe(pos, n, &handle->hlist){
        node = list_entry(pos, eventlist_t, list);
        #if USE_MUTI_THREAD
        pthread_mutex_lock(&handle->lock);
        list_del(&node->list); /* 从链表中删除节点 */
        pthread_mutex_unlock(&handle->lock);
        #else
        list_del(&node->list); /* 从链表中删除节点 */
        #endif
        sig = node->sig;
        handle->map[sig].func(node->args); /* 调用事件函数 */

        free(node);
    }
    
    return 0;
}

/**
 * @brief: 创建一个异步事件处理句柄
 * @return：异步事件句柄
*/
asyncevent_t* create_async_event(void)
{
    asyncevent_t *handle = (asyncevent_t*)malloc(sizeof(asyncevent_t));
    if(!handle) return NULL;
    memset(handle, 0, sizeof(handle));
    INIT_LIST_HEAD(&handle->hlist);
    #if USE_MUTI_THREAD
    pthread_mutex_init(&handle->lock, NULL);
    pthread_cond_init(&handle->cond, NULL);
    #endif
    return handle;
}

/**
 * @brief: 释放异步事件资源
 * @return: 0:成功
*/
int async_event_destory(asyncevent_t* handle)
{
    if(!handle) return -1;
    struct list_head *pos, *n;
    eventlist_t *node;

    #if USE_MUTI_THREAD
    pthread_mutex_lock(&handle->lock);
    #endif
    list_for_each_safe(pos, n, &handle->hlist){
        node = list_entry(pos, eventlist_t, list);
        list_del(&node->list); /* 从链表中删除节点 */
        free(node);
    }
    #if USE_MUTI_THREAD
    pthread_mutex_unlock(&handle->lock);
    pthread_mutex_destroy(&handle->lock);
    pthread_cond_destroy(&handle->cond);
    #endif

    free(handle);

    return 0;
}
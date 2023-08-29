# 一百多行C语言代码实现一个简单异步事件触发机制

# 一、简介
QT中有一种通信机制叫做信号和槽函数，通过将信号与槽函数进行绑定连接，后续若该信号触发，会自动调用对应的槽函数。这种机制很适合处理很繁琐的逻辑程序，例如我点击界面的close按钮，便触发close信号，自动调用close绑定的槽函数，关闭界面。这种使用流程简便快捷。这种处理机制可称作异步处理，C语言中也有一下异步处理开源的库，例如libevent、libev等，前者功能丰富，技术框架较为成熟，在许多项目中都见到它身影。这些开源库成熟，但是也庞大，能不能搞一个简洁的异步事件库呢？接下来我们就实现一个简单异步事件处理。

# 二、设计实现
我们做的是一个简单异步事件，根据信号触发对应事件，实现原理很简单：1.绑定信号和对应的回调函数; 2.检测信号队列或者链表，若有信号触发，便取出链表中的节点处理对应的回调函数。本设计中采用是双向链表存储信号，为了方便（偷懒），就不自己造链表的轮子了，这里使用Linux内核源码中的双向链表(list.h)。

## 1.结构体定义
这个``eventinfo_t``结构体里面包含对应的信号值和函数指针。
```C++
typedef struct eventinfo_t
{
    void (*func)(void* args); /* 事件回调函数 */
    int sig;                  /* 信号值 */  
}eventinfo_t;
```
这个``eventlist_t``结构体是定义信号链表的，里面包含了触发信号时候传递的参数，信号值，一个链表。
```C++
typedef struct eventlist_t
{
    void *args;            /* 传递的参数 */
    int sig;               /* 信号值 */
    struct list_head list; /* 双向链表 */
}eventlist_t;
```
这个``asyncevent_t``结构体是异步事件处理句柄定义，包含所有信息。
```C++
typedef struct asyncevent_t
{
    struct list_head hlist; /* 信号链表头 */
    eventinfo_t map[1024]; /* 信号与函数映射 */
    #if USE_MUTI_THREAD   /* 是否使用多线程 */
    pthread_mutex_t lock; /* 互斥锁 */
    pthread_cond_t cond;  /* 条件变量 */
    #endif
}asyncevent_t;
```
## 2.定义的函数
这是定义的所有函数，每个函数都有具体的注释，如下：
```C++

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
```

# 三、使用说明
## 1.编译运行
目标平台：Linux；编译器：GCC。
输入:make进行编译，输入:make clean清除生成文件，输入：./mainApp执行本例中的测试程序。
## 2.使用流程
通过修改asyncevent.h文件中``#define USE_MUTI_THREAD 1`` 的宏定义决定是否使用多线程，为1使用多线程。
### a.单线程模式测试
首先定义枚举类型的信号(也可不定义,直接写数值,为了规范还是建议定义)，编写对应的事件处理函数如``void event_click_func(void *args)``，然后创建句柄，绑定信号，在while循环里面调用``async_event_process(handle);``处理函数，至于信号什么时候发射完全由外部决定，本例直接在循环里面一直发射信号。注：不能在自己信号处理函数中发射自己信号，这样会导致一直循环发射处理，造成死循环，但是可以发射除自身以外的其他信号。
```C++
enum SIG_TYPE //信号类型
{
    CLICK=1,  //单击
    MOVE,     //拖动
    PRESS,    //按下
    RELEASE,  //释放
};

asyncevent_t* handle;

void event_click_func(void *args)
{
    printf("Click Event Trigger, Times=%d...\n", *(int*)args);

    //可以在事件处理函数中触发其他的信号！
    //不能在自己事件函数中触发自己，这样会一直循环触发自己，造成死循环!
    // async_event_emit(handle, 1, CLICK, args); //错误
    async_event_emit(handle, 1, MOVE, args);
    async_event_emit(handle, 1, PRESS, args);
    async_event_emit(handle, 1, RELEASE, args);
}

void event_move_func(void* args)
{
    printf("Move Event Trigger, Times=%d...\n", *(int*)args);
}

void event_press_func(void *args)
{
    printf("Press Event Trigger, Times=%d...\n", *(int*)args);
}

void event_release_func(void *args)
{
    printf("Release Event Trigger, Times=%d...\n", *(int*)args);
}

int main(int argc, char **argv)
{
    int cnt = 0;
    //1.创建事件句柄
    handle = create_async_event();

    //2.绑定信号
    async_event_bind(handle, CLICK, event_click_func);
    async_event_bind(handle, MOVE, event_move_func);
    async_event_bind(handle, PRESS, event_press_func);
    async_event_bind(handle, RELEASE, event_release_func);

    //3.循环调度执行
    while(1)
    {
        async_event_process(handle);
        async_event_emit(handle, 0, CLICK, &cnt);
    }

    return 0;
}

```
### b.多线程模式测试
处理main函数与上面不同，其他定义是一样的。本例使用多线程测试，开启一个线程一直调用``async_event_process(handle)``处理函数，然后main函数中采用输入a-c字符触发信号。

```C++
void* process_event_thread(void *args)
{
    asyncevent_t* handle = (asyncevent_t*)args;
    
    //循环调度执行
    while(1)
    {
        async_event_process(handle);
    }
}

int main(int argc, char** argv)
{
    pthread_t th;
    char c=0;
    int cnt_click = 0, cnt_move=0, cnt_press=0, cnt_release=0;

    //1.创建事件句柄
    handle = create_async_event();

    //2.绑定信号
    async_event_bind(handle, CLICK, event_click_func);
    async_event_bind(handle, MOVE, event_move_func);
    async_event_bind(handle, PRESS, event_press_func);
    async_event_bind(handle, RELEASE, event_release_func);

    //3.创建一个线程去处理事件
    pthread_create(&th, NULL, process_event_thread, handle);

    while(1)
    {
        //4.根据自己时机去触发信号
        scanf("%c", &c);
        switch (c)
        {
        case 'a':
            cnt_click++;
            async_event_emit(handle, 0, CLICK, &cnt_click);
            break;
        case 'b':
            cnt_move++;
            async_event_emit(handle, 0, MOVE, &cnt_move);
            break;
        case 'c':
            cnt_press++;
            async_event_emit(handle, 0, PRESS, &cnt_press);
            break;
        case 'd':
            cnt_release++;
            async_event_emit(handle, 0, RELEASE, &cnt_release);
            break;
        default:
            break;
        } 
        
    }

    pthread_join(th, NULL);

    return 0;
}
```

# 四、总结
这个异步事件处理程序还不过完善，欢迎大家尝试运行一下，有什么问题和我相互讨论，或者在群里面交流。
获取工程代码方式 在微信公众号【Linux编程用C】回复 **event** 即可获取。
#include "asyncevent.h"

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

#if !USE_MUTI_THREAD //是否使用多线程
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

#else

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

#endif
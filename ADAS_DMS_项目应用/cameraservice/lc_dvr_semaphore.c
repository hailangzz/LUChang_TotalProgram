#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <assert.h>
#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MY_SEM_KEY 0x3534ABCD
 
 
// 在Linux宿主机输入命令: man semctl 可以看到这个结构体的定义
union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};

static int sem_id = -1;
// semget() 并不初始化各个信号量的值
// 这个初始化必须通过 SETVAL (设置集合中的一个值) 或 SETALL (设置集合中的所有值) 调用 semctl() 来完成
// SystemV信号量的设计中, 创建一个信号量集并将它初始化需要两次函数调用是一个致命的缺陷
// 一个不完备的解决方案是: 在调用 semget() 时指定 IPC_CREAT | IPC_EXCL 标志
// 这样只有一个进程(首先调用 semget() 的那个进程)创建所需信号量, 该进程随后初始化该信号量
 
// 设置信号量
static int SetSemValue(void)
{
    int sem_num = 0; // sem_id 信号量集里的第一个信号量编号是 0
    union semun sem_union;
    sem_union.val = 1; // 设置编号为 0 的信号量初始值为 1
    assert(-1 != sem_id);
    if(semctl(sem_id, sem_num, SETVAL, sem_union) == -1)
    {
        printf("Failed to set semaphore\n");
        return -1;
    }
    return 0;
}

// 删除信号量
static int DelSemValue(void)
{
    int sem_num = 0; // sem_id 信号量集里的第一个信号量编号是 0
    union semun sem_union;
    assert(-1 != sem_id);
    if(semctl(sem_id, sem_num, IPC_RMID, sem_union) == -1)
    {
        printf("Failed to delete semaphore\n");
        return -1;
    }
    return 0;
}
 
// 为了防止因多个进程同时访问一个共享资源而引发的一系列问题, 将用到Linux的信号量(semaphore)
// 信号量是用来调协进程对共享资源的访问, 保证同一时间只有一个进程(线程)在访问共享资源
 
// IPC(包括消息队列, 共享内存, 信号量)的 xxxget() 创建操作时, 可以指定 IPC_CREAT 和 IPC_EXCL 选项.
// 以共享内存为例:
// 当只有 IPC_CREAT 选项时, 不管是否已存在该块共享内存, 都返回该共享内存的 ID, 若不存在则创建共享内存.
// 当只有 IPC_EXCL 选项时, 不管有没有该快共享内存, shmget() 都返回 -1.
// 当 IPC_CREAT | IPC_EXCL 时, 如果没有该块共享内存则创建并返回共享内存ID, 若已有该块共享内存则返回-1并设置errno = EEXIST.
 
// 创建信号量
int CreateSemaphore(void)
{
    int nRet = 0;
    int nsem = 1;
 
    if(sem_id == -1)
    {
        // semget() 函数创建一个信号量集或打开一个已存在的信号量集
        // nsem 指定集合中的信号量数
        // 返回的 sem_id 是一个信号量集, 而不是一个单独的信号量
        sem_id = semget((key_t)MY_SEM_KEY, nsem, 0666 | IPC_CREAT | IPC_EXCL);
        printf("semget id = %d\n", sem_id);
 
        if(sem_id == -1)
        {
            if(errno == EEXIST)
            {
                printf("sem key [0x%X] exist, error: %s\n", MY_SEM_KEY, strerror(errno));
                sem_id = semget((key_t)MY_SEM_KEY, nsem, 0666 | IPC_CREAT);
                if(sem_id == -1)
                {
                    printf("semget failed: %s\n", strerror(errno));
                    nRet = -1;
                }
            }
            else
            {
                printf("semget failed: %s\n", strerror(errno));
                nRet = -1;
            }
        }
        else
        {
            printf("create sem key [0x%X] succeed\n", MY_SEM_KEY);
            nRet = SetSemValue();
            if(0 > nRet)
            {
                printf("In CreateSemaphore(), call SetSemValue failed!\n");
            }
        }
    }
    return nRet;
}
   
// 由于信号量只能进行两种操作等待和发送信号, 即P(sv)和V(sv), 它们的行为是这样的:
// P(sv): 如果sv的值大于 0, 就给它减1, 如果它的值为 0, 就挂起该进程的执行(阻塞)
// V(sv): 如果有其他进程因等待 sv 而被挂起, 就让它恢复运行, 如果没有进程因等待sv而挂起, 就给它加 1
// 举个例子:
// 两个进程共享信号量 sv, 一旦其中一个进程执行了 P(sv) 操作, 它将得到信号量, 使 sv 减 1
// 而第二个进程当它试图执行 P(sv) 时, 由于 sv 为 0, 它会被挂起(阻塞)
// 待第一个进程执行 V(sv) 释放信号量之后, 这时第二个进程就可以恢复执行
 
// 设置阻塞
int SemaphoreP(void)
{
    struct sembuf sem_b;
    sem_b.sem_num = 0; // 指定要操作集合中编号为 0 的那个信号量
    sem_b.sem_op = -1; // 因该信号量初始值为 1, 此时给它减 1 让其值为 0
    sem_b.sem_flg = SEM_UNDO;
    assert(-1 != sem_id);
    if(semop(sem_id, &sem_b, 1) == -1)
    {
        printf("SemaphoreP failed\n");
        return -1;
    }
	//printf("S_P\n");
    return 0;
}
 
// 清除阻塞
int SemaphoreV(void)
{
    struct sembuf sem_b;
    sem_b.sem_num = 0; // 指定要操作集合中编号为 0 的那个信号量
    sem_b.sem_op = 1;  // 因该信号量之前被减过 1 了, 值为 0了, 此时给它加 1 让其值恢复为 1
    sem_b.sem_flg = SEM_UNDO;
    assert(-1 != sem_id);
    if(semop(sem_id, &sem_b, 1) == -1)
    {
        printf("SemaphoreV failed\n");
        return -1;
    }
	//printf("S_V\n");
    return 0;
}



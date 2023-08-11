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
 
 
// ��Linux��������������: man semctl ���Կ�������ṹ��Ķ���
union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};

static int sem_id = -1;
// semget() ������ʼ�������ź�����ֵ
// �����ʼ������ͨ�� SETVAL (���ü����е�һ��ֵ) �� SETALL (���ü����е�����ֵ) ���� semctl() �����
// SystemV�ź����������, ����һ���ź�������������ʼ����Ҫ���κ���������һ��������ȱ��
// һ�����걸�Ľ��������: �ڵ��� semget() ʱָ�� IPC_CREAT | IPC_EXCL ��־
// ����ֻ��һ������(���ȵ��� semget() ���Ǹ�����)���������ź���, �ý�������ʼ�����ź���
 
// �����ź���
static int SetSemValue(void)
{
    int sem_num = 0; // sem_id �ź�������ĵ�һ���ź�������� 0
    union semun sem_union;
    sem_union.val = 1; // ���ñ��Ϊ 0 ���ź�����ʼֵΪ 1
    assert(-1 != sem_id);
    if(semctl(sem_id, sem_num, SETVAL, sem_union) == -1)
    {
        printf("Failed to set semaphore\n");
        return -1;
    }
    return 0;
}

// ɾ���ź���
static int DelSemValue(void)
{
    int sem_num = 0; // sem_id �ź�������ĵ�һ���ź�������� 0
    union semun sem_union;
    assert(-1 != sem_id);
    if(semctl(sem_id, sem_num, IPC_RMID, sem_union) == -1)
    {
        printf("Failed to delete semaphore\n");
        return -1;
    }
    return 0;
}
 
// Ϊ�˷�ֹ��������ͬʱ����һ��������Դ��������һϵ������, ���õ�Linux���ź���(semaphore)
// �ź�����������Э���̶Թ�����Դ�ķ���, ��֤ͬһʱ��ֻ��һ������(�߳�)�ڷ��ʹ�����Դ
 
// IPC(������Ϣ����, �����ڴ�, �ź���)�� xxxget() ��������ʱ, ����ָ�� IPC_CREAT �� IPC_EXCL ѡ��.
// �Թ����ڴ�Ϊ��:
// ��ֻ�� IPC_CREAT ѡ��ʱ, �����Ƿ��Ѵ��ڸÿ鹲���ڴ�, �����ظù����ڴ�� ID, ���������򴴽������ڴ�.
// ��ֻ�� IPC_EXCL ѡ��ʱ, ������û�иÿ칲���ڴ�, shmget() ������ -1.
// �� IPC_CREAT | IPC_EXCL ʱ, ���û�иÿ鹲���ڴ��򴴽������ع����ڴ�ID, �����иÿ鹲���ڴ��򷵻�-1������errno = EEXIST.
 
// �����ź���
int CreateSemaphore(void)
{
    int nRet = 0;
    int nsem = 1;
 
    if(sem_id == -1)
    {
        // semget() ��������һ���ź��������һ���Ѵ��ڵ��ź�����
        // nsem ָ�������е��ź�����
        // ���ص� sem_id ��һ���ź�����, ������һ���������ź���
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
   
// �����ź���ֻ�ܽ������ֲ����ȴ��ͷ����ź�, ��P(sv)��V(sv), ���ǵ���Ϊ��������:
// P(sv): ���sv��ֵ���� 0, �͸�����1, �������ֵΪ 0, �͹���ý��̵�ִ��(����)
// V(sv): ���������������ȴ� sv ��������, �������ָ�����, ���û�н�����ȴ�sv������, �͸����� 1
// �ٸ�����:
// �������̹����ź��� sv, һ������һ������ִ���� P(sv) ����, �����õ��ź���, ʹ sv �� 1
// ���ڶ������̵�����ͼִ�� P(sv) ʱ, ���� sv Ϊ 0, ���ᱻ����(����)
// ����һ������ִ�� V(sv) �ͷ��ź���֮��, ��ʱ�ڶ������̾Ϳ��Իָ�ִ��
 
// ��������
int SemaphoreP(void)
{
    struct sembuf sem_b;
    sem_b.sem_num = 0; // ָ��Ҫ���������б��Ϊ 0 ���Ǹ��ź���
    sem_b.sem_op = -1; // ����ź�����ʼֵΪ 1, ��ʱ������ 1 ����ֵΪ 0
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
 
// �������
int SemaphoreV(void)
{
    struct sembuf sem_b;
    sem_b.sem_num = 0; // ָ��Ҫ���������б��Ϊ 0 ���Ǹ��ź���
    sem_b.sem_op = 1;  // ����ź���֮ǰ������ 1 ��, ֵΪ 0��, ��ʱ������ 1 ����ֵ�ָ�Ϊ 1
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



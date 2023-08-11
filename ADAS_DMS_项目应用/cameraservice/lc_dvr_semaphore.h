#ifndef LC_DVR_SEMAPHORE_H
#define LC_DVR_SEMAPHORE_H

#ifdef __cplusplus
extern "C" {
#endif

extern int CreateSemaphore(void);
extern int SemaphoreP(void);
extern int SemaphoreV(void);

#ifdef __cplusplus
}
#endif
#endif


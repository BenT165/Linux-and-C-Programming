/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>

#define HELD 1
#define FREE 0


////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
	struct semaphore *sem;

	assert(initial_count >= 0);

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
		return NULL;
	}

	sem->name = kstrdup(namearg);
	if (sem->name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->count = initial_count;
	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	spl = splhigh();
	assert(thread_hassleepers(sem)==0);
	splx(spl);

	/*
	 * Note: while someone could theoretically start sleeping on
	 * the semaphore after the above test but before we free it,
	 * if they're going to do that, they can just as easily wait
	 * a bit and start sleeping on the semaphore after it's been
	 * freed. Consequently, there's not a whole lot of point in 
	 * including the kfrees in the splhigh block, so we don't.
	 */

	kfree(sem->name);
	kfree(sem);
}

void 
P(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	assert(in_interrupt==0);

	spl = splhigh();
	while (sem->count==0) {
		thread_sleep(sem);
	}
	assert(sem->count>0);
	sem->count--;
	splx(spl);
}

void
V(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);
	spl = splhigh();
	sem->count++;
	assert(sem->count>0);
	thread_wakeup(sem);
	splx(spl);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(struct lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->name = kstrdup(name);
	if (lock->name == NULL) {
		kfree(lock);
		return NULL;
	}
	
	// intialize lock to be unlocked and have no holder
    lock->status = FREE;
    lock->holder = NULL;

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	int spl;
    assert(lock != NULL);
    assert(in_interrupt == 0);

    //make sure no threads are waiting to acquire the lock
	spl = splhigh();
	assert(thread_hassleepers(lock)==0);
	
    //Check if any thread holding the lock:
	assert(lock->status==FREE);
	splx(spl);

    //free dynamically allocated pointers
	kfree(lock->name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
    int spl;
    assert(lock!= NULL);
    assert(in_interrupt == 0);
    spl = splhigh();
    
    //sleep while another thread holds this lock
    while(lock->status == HELD){
        thread_sleep(lock); //Sleep on the address of the lock
    }

    //grab lock once no thread is holding it
    assert(lock->status == FREE); //Double check if the status is free before take the lock
    lock->status = HELD;
    lock->holder = curthread;
    
    //make sure this thread holds the lock
    assert(lock_do_i_hold(lock)==1); 
    splx(spl);
}

void
lock_release(struct lock *lock)
{
    int spl;
    assert(lock != NULL);
    spl=splhigh();
    
    //make sure current thread is holding lock before trying to release it
    assert(lock_do_i_hold(lock)); 
    assert(lock->status == HELD);

    //free lock and set fields to indicate no thread is holding it
    lock->status=FREE;
    lock->holder=NULL;
    assert(lock->status == FREE);

    //wake up threads sleeping on the lock
    thread_wakeup(lock);
    splx(spl);

}

int
lock_do_i_hold(struct lock *lock)
{
    int do_i_hold;

    assert(lock != NULL);
    do_i_hold = lock->holder == curthread? 1 : 0;
    
    return do_i_hold; //return whether or not the current thread holds the lock

}


////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(struct cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->name = kstrdup(name);
	if (cv->name==NULL) {
		kfree(cv);
		return NULL;
	}
	
	// add stuff here as needed
	
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	// add stuff here as needed
	
	kfree(cv->name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	// Write this
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	// Write this
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}

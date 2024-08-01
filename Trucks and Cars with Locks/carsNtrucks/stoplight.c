/* 
 * stoplight.c
 *
 * 31-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: You can use any synchronization primitives available to solve
 * the stoplight problem in this file.
 */

/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>
#include <kern/errno.h>
#include <machine/spl.h>


/*
 * Constants 
 */
#define NVEHICLES 20 
#define RIGHT 0
#define LEFT 1
#define CAR 0
#define TRUCK 1
#define LANEA 0
#define LANEB 1
#define LANEC 2
#define APR 0
#define ENT 1
#define EXT 2

/*
 * Three locks for different sections of intersection. 
 */

static struct lock *lt_AB = NULL; 
static struct lock *lt_BC = NULL;
static struct lock *lt_CA = NULL;
static struct lock *left_lock=NULL;

/*
 * Locks that block future trucks from proceeding for each lane
 */
 
static struct lock *truck_lock_A =NULL;
static struct lock *truck_lock_B= NULL;
static struct lock *truck_lock_C= NULL;



/*
 * Restricts access to global array of cars that have yet to finish a turn
 */
static struct lock *total_car_lock = NULL; //Total vehicle remain bodyguard
static struct lock *car_arr_lock = NULL; // Each exit count bodyguard


/*
 * Vehicle count.
 */
static volatile int vehicles_remaining;

/*
 * Represents number of cars in each lane that have not yet completed a turn
 */
static int num_cars_waiting[3]={0,0,0}; //Keep track of count in each lanes.

/*
 * Sleep variable for the parent thread, parent thread will sleep on this address until all cars are done
 */
int mom;


/*
 * Function protoypes
 */
void print_info(long type, long number, long origin, long turn_dir, char *section, int phase);
void car_approach(int lane);
int car_waiting(int lane);
void direction(unsigned long vehicledirection, unsigned long vehiclenumber, unsigned long vehicletype, int turndirection);
static void turnright(unsigned long vehicledirection, unsigned long vehiclenumber, unsigned long vehicletype);
static void turnleft(unsigned long vehicledirection, unsigned long vehiclenumber, unsigned long vehicletype);
void remove_vehicle();

/*
 * Displays information about a vehicle approaching, entering, or leaving an intersection space
 */
void print_info(long type, long number, long origin, long turn_dir, char *section, int phase){

    //is vehicle car or truck
    const char *car_truck = type == CAR ? "Car" : "Truck";

    //where is vehicle coming from
    const char *start = origin == LANEA? "Route A" : (origin == LANEB? "Route B" : "Route C");

    //is car going left or right
    const char *left_right = turn_dir == LEFT? "LEFT" : "RIGHT";

    //which destination number a vehicle is headed to
    long end_route = (origin + (turn_dir+1))%3;

    //string representation of the destination number a vehicle is headed to
    const char *dest = end_route == LANEA? "Route A" : (end_route == LANEB? "Route B" : "Route C");
    
    //display vehicle info
    switch (phase){
        case APR:
            kprintf("%s %lu from %s to turn %s to %s APPROACHES the intersection\n", car_truck, number, start, left_right, dest);
            break;
        case ENT:
            kprintf("%s %lu from %s to turn %s to %s ENTERED %s\n", car_truck, number, start, left_right, dest, section);
            break;
        case EXT:
            kprintf("%s %lu from %s to turn %s to %s LEFT %s\n", car_truck, number, start, left_right, dest, section);
            break;
    }
}

/*
 * remove_vehicle()
 * 
 * Atomically decrements number of vehicles parent thread is waiting on to finish.
 */
void remove_vehicle(){
   struct lock *vehicle_count;
    vehicle_count= total_car_lock;
    assert(vehicle_count!=NULL);

    //Atomic update to global variable
    lock_acquire(vehicle_count);
    vehicles_remaining--; 
    lock_release(vehicle_count);
}

/* 
 * car_approach():
 *
 * Atomically decrements the number of cars from a given lane that have not yet completed their route.
 */
void car_approach(int lane){
    struct lock *car_tracker = car_arr_lock;
    assert(car_tracker!=NULL);
    lock_acquire(car_tracker);
    num_cars_waiting[lane]=num_cars_waiting[lane]+1;
    lock_release(car_tracker);
}

/*
 * car_waiting()
 *
 * Returns: false if all cars from the lane have completed their routes, 
 *          true at least 1 car from the lane has yet to complete its route.
 */
int car_waiting(int lane){
    struct lock *car_tracker= car_arr_lock;
    assert(car_tracker!=NULL);
    lock_acquire(car_tracker);

    if (num_cars_waiting[lane]==0){
        lock_release(car_tracker);
        return 0;
    }
    else {
        lock_release(car_tracker);
        return 1;
    }
}


/*
* Makes a vehicle turn either right or left depending on vehicle direction
*/
void direction(unsigned long vehicledirection, unsigned long vehiclenumber, unsigned long vehicletype, int turndirection){
            if (turndirection==LEFT){
                turnleft(vehicledirection, vehiclenumber, vehicletype);
            }
            else if (turndirection==RIGHT){
                turnright(vehicledirection, vehiclenumber, vehicletype);
            }
}

/*
 * turnleft()
 *
 * Arguments:
 *      unsigned long vehicledirection: the direction from which the vehicle
 *              approaches the intersection.
 *      unsigned long vehiclenumber: the vehicle id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a left turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static void turnleft(unsigned long vehicledirection,
        unsigned long vehiclenumber,
        unsigned long vehicletype)
{

    struct lock *first_lock; 
    struct lock *second_lock;
    struct lock *duo_lock= left_lock;
    assert(duo_lock!=NULL);
    
    if (vehicledirection==LANEA){
        first_lock=lt_AB;
        assert(first_lock!=NULL);
        second_lock=lt_BC;
        assert(second_lock!=NULL);
    }
    else if (vehicledirection==LANEB){ //LANE B toward Lane A.
        first_lock=lt_BC;
        assert(first_lock!=NULL);
        second_lock=lt_CA;
        assert(second_lock!=NULL);
    }

    else if (vehicledirection==LANEC){
        first_lock=lt_CA;
        assert(first_lock!=NULL);
        second_lock=lt_AB;
        assert(second_lock!=NULL);
    }
    
    lock_acquire(duo_lock); //This will make sure this thread will turn left.
    lock_acquire(first_lock); //This will make sure this thread will have the first lock.

        print_info(vehicletype, vehiclenumber, vehicledirection, LEFT, first_lock->name, ENT);//Enter space 1 
        print_info(vehicletype, vehiclenumber, vehicledirection, LEFT, first_lock->name, EXT);//Exit space 1
        lock_acquire(second_lock); //Try to grab the second lock
   
        lock_release(duo_lock);
        lock_release(first_lock);
        print_info(vehicletype, vehiclenumber, vehicledirection, LEFT, second_lock->name, ENT);//Exit space 1
        print_info(vehicletype, vehiclenumber, vehicledirection, LEFT, second_lock->name, EXT);//Exit space 1


    remove_vehicle(); //Decrement the total number of car
    lock_release(second_lock);
}


/*
 * turnright()
 *
 * Arguments:
 *      unsigned long vehicledirection: the direction from which the vehicle
 *              approaches the intersection.
 *      unsigned long vehiclenumber: the vehicle id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a right turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static void turnright(unsigned long vehicledirection,
        unsigned long vehiclenumber,
        unsigned long vehicletype)
{
    struct lock *rLock; //lock for the right turn

    if (vehicledirection==LANEA){
        rLock=lt_AB;
    }
    else if (vehicledirection==LANEB){	
        rLock=lt_BC;
    }
    else if (vehicledirection==LANEC){
        rLock=lt_CA;
    }
    assert(rLock != NULL);
    lock_acquire(rLock);

    //if vehicle has the lock it needs to turn
    if(lock_do_i_hold(rLock)){

        //display info for entering/exiting
        print_info(vehicletype, vehiclenumber, vehicledirection, RIGHT, rLock->name, ENT);
        print_info(vehicletype, vehiclenumber, vehicledirection, RIGHT, rLock->name, EXT);
        remove_vehicle(); //Decrement the total number of car in the lane.
        lock_release(rLock);
    }
}


    /*
     * approachintersection()
     *
     * Arguments: 
     *      void * unusedpointer: currently unused. 
    //This might be used for thread struct of vehicle.
     *      unsigned long vehiclenumber: holds vehicle id number.
     *
     * Returns:
     *      nothing.
     *
     * Notes:
     *      Change this function as necessary to implement your solution. These
     *      threads are created by createvehicles().  Each one must choose a direction
     *      randomly, approach the intersection, choose a turn randomly, and then
     *      complete that turn.  The code to choose a direction randomly is
     *      provided, the rest is left to you to implement.  Making a turn
     *      or going straight should be done by calling one of the functions
     *      above.
     */

    static void approachintersection(void * vehicle, //Thread struct
                unsigned long vehiclenumber)
        {
            (void) vehicle;
            int vehicledirection, turndirection, vehicletype;
            int spl;
            struct lock *truck_lock; //Lock to make sure no more truck cut infront of the first waiting truck
            struct lock *arr_car_lock=car_arr_lock;
            assert(arr_car_lock!=NULL);

            struct lock *vehicle_lock= total_car_lock;
            assert(vehicle_lock!=NULL);
            
            /*
             * vehicledirection is set randomly.
             */
            vehicledirection = random() % 3; //Which lanes the vehicle is in
            turndirection = random() % 2; //Turning direction / left or right
            vehicletype = random() % 2; //Truck or Car

             if (vehicledirection==LANEA){
                    truck_lock=truck_lock_A;
                }
                else if (vehicledirection==LANEB){
                    truck_lock=truck_lock_B;
                }
                else if (vehicledirection==LANEC){
                    truck_lock=truck_lock_C;
                }

            assert(truck_lock!=NULL);

            //tell the world that you're approaching the intersection 
            print_info(vehicletype, vehiclenumber, vehicledirection, turndirection, NULL, APR);
            
            //if car, update number of cars waiting for lane
            if(vehicletype == CAR){
                spl=splhigh();
                car_approach(vehicledirection); //update the lane count up, this will have the lock inside based on the lane:           
                splx(spl);
                /*
                 * Inside the function:
                 * lock array -> car count on the lane ++ -> unlock array
                 */
                
                direction(vehicledirection, vehiclenumber, vehicletype, turndirection); 

                //update array atomically
               lock_acquire(arr_car_lock);
               num_cars_waiting[vehicledirection]--;
               lock_release(arr_car_lock);

                //wake up sleeping truck if there are no cars in lane (sleep address is num_cars_waiting)

             spl=splhigh();
                if (car_waiting(vehicledirection)==0){
                    if (thread_hassleepers(&num_cars_waiting[vehicledirection])!=0){
                        thread_wakeup(&num_cars_waiting[vehicledirection]);                     
                    }

                }

                splx(spl);
          }
          else if (vehicletype==TRUCK){
          
                //get truck lock               
                lock_acquire(truck_lock);

                //sleep if there are cars waiting
                spl=splhigh();
                if(car_waiting(vehicledirection)==1){
                    thread_sleep(&num_cars_waiting[vehicledirection]);
                }
                splx(spl);

                if (lock_do_i_hold(truck_lock)){
                   lock_release(truck_lock);
                }
    
                 //GO TO TRUCK PARADISE :>
                direction(vehicledirection, vehiclenumber, vehicletype, turndirection);
          }
               
             
          //wake up mama thread if we are last vehicle
          spl=splhigh();
          if(vehicles_remaining==0){
           // lock_release(vehicle_lock);
            if (thread_hassleepers(&mom)!=0){
            thread_wakeup(&mom);
            }
            else {
                panic("Where is mom :( ?\n");
            }
          }
          splx(spl);
        

        }


    /*
     * createvehicles()
     *
     * Arguments:
     *      int nargs: unused.
     *      char ** args: unused.
     *
     * Returns:
     *      0 on success.
     *
     * Notes:
     *      Driver code to start up the approachintersection() threads.  You are
     *      free to modiy this code as necessary for your solution.
     */

    int createvehicles(int nargs, char ** args){


        int index, error;

        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;

        //bunch of locks
        struct lock *lockAB, *lockBC, *lockCA;
        
        struct lock *left;

        struct lock *truckA, *truckB, *truckC;

        struct lock *arr_lock;

        struct lock *veh_lock;

        /*
         * Initialize the intersection lock:
         */
        lockAB = lock_create("AB");
        if (lockAB == NULL) {	
            return ENOMEM;
        }
        lockBC = lock_create("BC");
        if (lockBC == NULL) {
            lock_destroy(lockAB);
            return ENOMEM;
        }

        lockCA = lock_create("CA");	
        if (lockCA == NULL) {
            lock_destroy(lockBC);
            lock_destroy(lockAB);
            return ENOMEM;
        }

        //set global lock
        lt_AB=lockAB;
        lt_BC=lockBC;
        lt_CA=lockCA;
        
        left=lock_create("Left lock");
        if (left==NULL){
            return ENOMEM;
        }
        left_lock=left;

        /*
         *
         * Creating lane lock:
         */
        truckA=lock_create("Lane A truck");
        if(truckA==NULL){
            return ENOMEM;
        }
        truckB=lock_create("Lane B truck");
        if (truckB==NULL){
            lock_destroy(truckA);
            return ENOMEM;
        }
        truckC=lock_create("Lane C truck");
        if (truckC==NULL){
            lock_destroy(truckA);
            lock_destroy(truckB);
            return ENOMEM;
        }

        //Link the lane lock with global lock:
        truck_lock_A= truckA;
        truck_lock_B= truckB;
        truck_lock_C= truckC;

        /*
         * Car tracker: Array lock:
         */

        veh_lock= lock_create("Seven Corner");
        if (veh_lock ==NULL){
            return ENOMEM;
        }
        total_car_lock= veh_lock;


        arr_lock= lock_create("Koons");
        if (arr_lock ==NULL){
            return ENOMEM;
        }
        car_arr_lock= arr_lock;

       
        vehicles_remaining=NVEHICLES;

        /*
         * Start NVEHICLES approachintersection() threads.
         */
        for (index = 0; index < NVEHICLES; index++) {

            error = thread_fork("approachintersection thread",
                    NULL,
                    index,
                    approachintersection,
                    NULL
                    );
            /*
             * panic() on error.
             */

            if (error) {

                panic("approachintersection: thread_fork failed: %s\n",
                        strerror(error)
                     );
            }

        }

        int spl;
        spl=splhigh();
        if (vehicles_remaining != 0){
            thread_sleep(&mom);
           splx(spl);

        }
        else{
            splx(spl);
        }
        //Factory Reset:
        vehicles_remaining = NVEHICLES;
        num_cars_waiting[0]=0;
        num_cars_waiting[1]=0;
        num_cars_waiting[2]=0;
        //Clean up after done:
        lock_destroy(lt_AB);
        lock_destroy(lt_BC);
        lock_destroy(lt_CA);
        lock_destroy(left_lock);
        lock_destroy(truck_lock_A);
        lock_destroy(truck_lock_B);
        lock_destroy(truck_lock_C);
        lock_destroy(total_car_lock);
        lock_destroy(car_arr_lock);
        return 0;
    }

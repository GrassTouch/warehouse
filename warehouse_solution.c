#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

int trailer_count;
int total_containers;
int trailer_at_sec_post;

void* trailer(void*);
void* security();
void* forklift(void*);
int random_range(int, int);

sem_t sec_avail_sem;        //signal 1 security post availability 
sem_t loading_bay_sem;      //signal 2 loading bays availability
sem_t check_start_sem;      //signals security to start checking if trailers pnt at security post 
sem_t check_done_sem;       //signal trailer to leave security post
sem_t container_sem;        //ensure mutual exclusion to total_containers variable
sem_t trailer_sem;          //ensure mutual exclusion to trailer_at_sec_post variable
sem_t container_count_sem;  //signals forklift to start moving if containers present at loading bay

int main(int argc, char* argv[])
{
    //store user input into int variable trailer_count
    trailer_count = atoi(argv[1]);
    //Input validation
    if (trailer_count < 3 || trailer_count > 10) {
        printf("Number of trailers must be between 3 and 10.\n");
        return 1;
    }
    //Print input number of trailers
    printf("total number of trailers: %d\n", trailer_count);

    int res;

    //Initialise semaphores
    if (sem_init(&sec_avail_sem, 0, 1) != 0 ||
        sem_init(&loading_bay_sem, 0, 2) != 0 ||
        sem_init(&check_start_sem, 0, 0) != 0 ||
        sem_init(&check_done_sem, 0, 0) != 0 ||
        sem_init(&container_sem, 0, 1) != 0 ||
        sem_init(&trailer_sem, 0, 1) != 0 ||
        sem_init(&container_count_sem, 0, 0) != 0)
    {
        perror("Semaphore initialization failed");
        exit(EXIT_FAILURE);
    }

    //declare threads 
    pthread_t trailer_thread[trailer_count];
    pthread_t security_thread;
    pthread_t forklift_thread[2];

    //Create security thread
    res = pthread_create(&security_thread, NULL, security, NULL);
    if (res != 0) { //error handling for thread creation
        perror("Thread creation failed!: security thread");
        exit(EXIT_FAILURE);
    }

    //Create forklift thread  
    int forklift_id[2];
    for (int i = 1; i <= 2; i++) {
        forklift_id[i - 1] = i;
        res = pthread_create(&forklift_thread[i - 1], NULL, forklift, &forklift_id[i-1]);
        if (res != 0) { //error handling for thread creation
            perror("Thread creation failed!: forklift thread");
            exit(EXIT_FAILURE);
        }
    }

    //Create trailer threads
    for (int i = 1; i <= trailer_count; i++) {

        //Create a trailer thread every 3-4s 
        res = pthread_create(&trailer_thread[i - 1], NULL, trailer, &i); //trailer threads creation
        if (res != 0) {//error handling for thread creation
            perror("Thread creation failed!: trailer thread");
            exit(EXIT_FAILURE);
        }
        sleep(random_range(3, 4));
    }

    //Join trailer threads
    for (int i = 0; i < trailer_count; i++) {

        sleep(random_range(2, 3));
        res = pthread_join(trailer_thread[i], NULL);
        if (res != 0) {
            perror("Trailer threads join failed.");
            exit(EXIT_FAILURE);
        }
    }
    printf("\033[1;97mAll trailer joined\n");

    //Join security threads
    res = pthread_join(security_thread, NULL);
    if (res != 0) {
        perror("Security thread join failed.");
        exit(EXIT_FAILURE);
    }
    printf("\033[1;97mSecurity joined\n");

    //Join forklift threads
    for (int i = 0; i < 2; i++) {

        res = pthread_join(forklift_thread[i], NULL);
        if (res != 0) {
            perror("Forklift threads join failed.");
            exit(EXIT_FAILURE);
        }
    }
    printf("\033[1;97mAll forklift joined\n");

    //Destroy semaphores
    sem_destroy(&sec_avail_sem);
    sem_destroy(&loading_bay_sem);
    sem_destroy(&check_start_sem);
    sem_destroy(&check_done_sem);
    sem_destroy(&container_sem);
    sem_destroy(&trailer_sem);
    sem_destroy(&container_count_sem);


    printf("\033[1;97mProgram terminates.\n");
    exit(EXIT_SUCCESS);

}

// Trailer function definition
void* trailer(void* num_input) {
    int id = *(int*)num_input;

    //if security is available, begin the operations below
    sem_wait(&sec_avail_sem);

    //trailer_sem ensure mutual exclusion to trailer_at_sec_post variable
    sem_wait(&trailer_sem);
    trailer_at_sec_post = id;
    sem_post(&trailer_sem);

    //at security post
    printf("\033[1;33mTrailer-%d: Arrived.\n", id);
    printf("\033[1;33mTrailer-%d: Under checking...\n", id);
    sem_post(&check_start_sem); //signal security to start checking
    sem_wait(&check_done_sem);  //wait for checking to be done

    //at loading bay
    printf("\033[1;33mTrailer-%d: Waiting for loading bay...\n", id);
    sem_wait(&loading_bay_sem);
    sleep(random_range(2, 4));
    sem_wait(&container_sem);
    total_containers += 2;
    sem_post(&container_sem);
    printf("\033[1;33mTotal containers = %d\n", total_containers);

    //increment container count
    sem_post(&container_count_sem);
    sem_post(&container_count_sem);

    sem_post(&loading_bay_sem);
    printf("\033[1;33mTrailer-%d: Unloaded. Leaving...\n", id);
    return NULL;
}

//Security function defintion
void* security() {
    for (int i = 0; i < trailer_count; i++) {
        sem_wait(&check_start_sem);      //check if there are any trailers in queue
        printf("\033[0;34mSecurity: Stanby\n");
        printf("\033[0;34mSecurity: Checking Trailer-%d\n", trailer_at_sec_post);
        sleep(random_range(3, 4));
        printf("\033[0;34mSecurity: Checked & Released\n");
        sem_post(&check_done_sem);       //allow trailer to leave
        sem_post(&sec_avail_sem);        //operation finished, allow security for next trailer 
    }

    printf("\033[1;34mSecurity: Exit\n");
    return NULL;
}

//Forklift function definition
void* forklift(void* num_input) {
    int id = *(int*)num_input;
    struct timespec timeout;

    while (1) {
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 10;

        if (sem_timedwait(&container_count_sem, &timeout) == -1) {
            printf("\033[1;32mForklift-%d: Time out. Exit\n", id);
            break;
        }

        printf("\033[1;32mForklift-%d: Waiting for containers\n", id);
        printf("\033[1;32mForklift-%d: Moving a container\n", id);
        sleep(3);//forklift need 3 seconds for moving

        sem_wait(&container_sem);
        total_containers--;
        printf("\033[1;32mForklift-%d: Remaining = %d\n", id, total_containers);
        sem_post(&container_sem);
    }
    return NULL;
}

// Random number generator within range
int random_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

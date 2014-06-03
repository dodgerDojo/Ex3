// TODOS:
// 1. remove errno and debug prints.
// 2. check fork() return value!
// 3. unlink fifo in failures after mkfifo
/********************************/

// Includes:

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>

/********************************/

// Defines:

#define FIFO_NAME           ("ex3FIFO")

#define ALLOW_READ_WRITE_TO_ALL (0666)

#define SHM_SIZE_IN_BYTES       (1024)

#define SHMAT_FAILED        ((void *)-1)

#define NUM_OF_SEMAPHORES   (1)
#define SEM_INIT            (1)

#define NOF_INPUTS_ERROR    ("wrong number of inputs.\n")
#define OPEN_ERROR          ("open() failed.\n")
#define MKFIFO_ERROR        ("mkfifo() failed.\n")
#define WRITE_ERROR         ("write() failed.\n")
#define CLOSE_ERROR         ("close() failed.\n")
#define READ_ERROR          ("read() failed.\n")
#define SIGACTION_ERROR     ("sigaction() failed.\n")
#define SIGFILLSET_ERROR    ("sigfillset() failed.\n")
#define UNLINK_ERROR        ("unlink() failed.\n")
#define FTOK_ERROR          ("ftok() failed.\n")
#define SHMGET_ERROR        ("shmget() failed.\n")
#define SHMAT_ERROR         ("shmat() failed.\n")
#define SEMGET_ERROR        ("shmget() failed.\n")
#define SHMCTL_ERROR        ("shmctl() failed.\n")
#define SEMCTL_ERROR        ("semctl() failed.\n")
#define KILL_ERROR          ("kill() failed.\n")
#define SEMOP_ERROR         ("semop() failed.\n")

#define EXIT_ERROR_CODE     (-1)
#define EXIT_OK_CODE        (0)

/********************************/

// Types:

union semun
{
    int val;
    struct semid_ds* buf;
    unsigned short *array;
};

/********************************/

// Static Variables:

static pid_t Queue_Pid = 0;
volatile sig_atomic_t Got_Signal = 0;

/********************************/

// Static Declarations:

static void sigusr1Handler(int signum, siginfo_t *info, void *ptr);
static void initSigactions(void);
static void waitForSignal(void);

static void deleteFifo(void);
static void writePidToFifo(void);

char *createSharedMemory(const char *key_file_name, key_t *key, int *shm_id);
static int createBinarySemaphore(key_t sem_key);

static int readQueueLengthFromFifo(void);

static void deleteSharedMemory(int shm_id);
static void deleteResources(int shm_id, int sem_id);

static void semLock(int sem_id, int shm_id);
static void semUnlock(int sem_id, int shm_id);

static void handleGame(int shm_id, char *shm_addr, int sem_id, int queue_size);

/********************************/

// Functions:

/************************************************************************
* function name: sigusr1Handler
* The Input: sig - no. of signal
* The output: -
* The Function operation: notifies that the SIGUSR1 was received by rising a flag.
*************************************************************************/
static void sigusr1Handler(int signum, siginfo_t *info, void *ptr)
{
    Got_Signal = 1;
    Queue_Pid = info->si_pid;
}

static void initSigactions(void)
{
    struct sigaction usrAction;
    sigset_t blockMask;

    // Unblock SIGINT
    if(sigfillset(&blockMask) < 0)
    {
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SIGFILLSET_ERROR, sizeof(SIGFILLSET_ERROR) - 1);
        exit(EXIT_ERROR_CODE);
    }

    // Establish the SIGUSR1 signal handler.
    usrAction.sa_sigaction = sigusr1Handler;
    usrAction.sa_mask = blockMask;
    usrAction.sa_flags = SA_SIGINFO;
    if(sigaction(SIGUSR1, &usrAction, NULL) < 0)
    {
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SIGACTION_ERROR, sizeof(SIGACTION_ERROR) - 1);
        exit(EXIT_ERROR_CODE);
    }
}

static void waitForSignal(void)
{
    // Wait for SIGUSR1.
    while(!Got_Signal);
    Got_Signal = 0;
}

static void deleteFifo(void)
{
    if(unlink(FIFO_NAME) < 0)
    {
        // No checking needed, exits with error code.
        write(STDERR_FILENO, UNLINK_ERROR, sizeof(UNLINK_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    printf("deleted.");
    fflush(stdout);
}

static void writePidToFifo(void)
{
    pid_t my_pid = 0;
    int fifo_fd = 0;

    // Get self PID.
    my_pid = getpid();

    // Open FIFO.
    fifo_fd = open(FIFO_NAME, O_WRONLY);
    if(fifo_fd < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, OPEN_ERROR, sizeof(OPEN_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    // Write process PID to FIFO.
    if(write(fifo_fd, (void*)&my_pid, sizeof(my_pid)) != sizeof(my_pid))
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, WRITE_ERROR, sizeof(WRITE_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    // Close FIFO.
    if(close(fifo_fd) < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, CLOSE_ERROR, sizeof(CLOSE_ERROR));
        exit(EXIT_ERROR_CODE);
    }
}

char *createSharedMemory(const char *key_file_name, key_t *key, int *shm_id)
{
    const unsigned char KEY_CHAR = 'H';

    char *p_shm_data = NULL;

    // Generate key from the given file
    *key = ftok(key_file_name, KEY_CHAR);
    if(*key < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, FTOK_ERROR, sizeof(FTOK_ERROR));
        exit(EXIT_ERROR_CODE);       
    }

    // Create shared memory
    *shm_id = shmget(*key, SHM_SIZE_IN_BYTES, IPC_CREAT|ALLOW_READ_WRITE_TO_ALL);
    if(*shm_id < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SHMGET_ERROR, sizeof(SHMGET_ERROR));
        exit(EXIT_ERROR_CODE);             
    }

    // Connect to the shared memory.
    p_shm_data = (char *)shmat(*shm_id, 0, 0);
    if(SHMAT_FAILED == p_shm_data)
    {
        deleteFifo();
        deleteSharedMemory(*shm_id);
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SHMAT_ERROR, sizeof(SHMAT_ERROR));
        exit(EXIT_ERROR_CODE);      
    }

    return p_shm_data;
}

static int createBinarySemaphore(key_t sem_key)
{
    union semun sem_conf;

    int sem_id = semget(sem_key, NUM_OF_SEMAPHORES, IPC_CREAT|ALLOW_READ_WRITE_TO_ALL);
    if(sem_id < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SEMGET_ERROR, sizeof(SEMGET_ERROR));
        exit(EXIT_ERROR_CODE);         
    }

    sem_conf.val = SEM_INIT;
    if(semctl(sem_id, 0, SETVAL, sem_conf) < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SEMCTL_ERROR, sizeof(SEMCTL_ERROR));
        exit(EXIT_ERROR_CODE);   
    }

    return sem_id;
}

static int readQueueLengthFromFifo(void)
{
    int queue_length = 0;

    int fifo_fd = open(FIFO_NAME, O_RDWR);
    if(fifo_fd < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, OPEN_ERROR, sizeof(OPEN_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    // Read the queue length.
    if(read(fifo_fd, (void*)&queue_length, sizeof(queue_length)) != sizeof(queue_length))
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, READ_ERROR, sizeof(READ_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    // Close FIFO.
    if(close(fifo_fd) < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, CLOSE_ERROR, sizeof(CLOSE_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    return queue_length;
}

static void deleteSharedMemory(int shm_id)
{
    if(shmctl(shm_id, IPC_RMID, 0) < 0)
    {
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SHMCTL_ERROR, sizeof(SHMCTL_ERROR));
        exit(EXIT_ERROR_CODE);
    }
}

static void deleteResources(int shm_id, int sem_id)
{
    union semun ignored_arg;

    deleteSharedMemory(shm_id);

    if(semctl(sem_id, NUM_OF_SEMAPHORES, IPC_RMID, ignored_arg) < 0)
    {
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SEMCTL_ERROR, sizeof(SEMCTL_ERROR));
        exit(EXIT_ERROR_CODE);
    }
}

static void semLock(int sem_id, int shm_id)
{
    struct sembuf sem_opt;

    sem_opt.sem_num = 0;
    sem_opt.sem_flg = 0;

    // Lock!
    sem_opt.sem_op = -1;

    if(semop(sem_id, &sem_opt, NUM_OF_SEMAPHORES) < 0)
    {
        deleteResources(shm_id, sem_id);
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SEMOP_ERROR, sizeof(SEMOP_ERROR));
        exit(EXIT_ERROR_CODE);
    }
}

static void semUnlock(int sem_id, int shm_id)
{
    struct sembuf sem_opt;

    sem_opt.sem_num = 0;
    sem_opt.sem_flg = 0;

    // Unlock!
    sem_opt.sem_op = 1;

    if(semop(sem_id, &sem_opt, NUM_OF_SEMAPHORES) < 0)
    {
        deleteResources(shm_id, sem_id);
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SEMOP_ERROR, sizeof(SEMOP_ERROR));
        exit(EXIT_ERROR_CODE);
    }
}

static void handleGame(int shm_id, char *shm_addr, int sem_id, int queue_size)
{
    char game_ended = 0;

    char old_image[SHM_SIZE_IN_BYTES] = {0};
    char current_image[SHM_SIZE_IN_BYTES] = {0};

    while(!game_ended)
    {
        semLock(sem_id, shm_id);

        // Read the shared memory data.
        memcpy((void *)current_image, (void *)shm_addr, SHM_SIZE_IN_BYTES);

        // If there's nothing new, continue.
        if(0 == memcmp((void *)current_image, (void *)old_image, SHM_SIZE_IN_BYTES))
        {
            semUnlock(sem_id, shm_id);
            continue;
        }

        // Otherwise, fetch the data.
        printf("%s", current_image);
        fflush(stdout);

        // Update the old image.
        memcpy((void *)old_image, (void *)current_image, SHM_SIZE_IN_BYTES);

        semUnlock(sem_id, shm_id);
    }
}

/********************************/

// Main:

int main(int argc, char *argv[])
{
    const unsigned int EXPECTED_ARGC = 2;
    const unsigned int KEY_FILE_ARG_INDEX = 1;

    key_t key = 0;
    char *shm_addr = NULL;
    int queue_length = 0, shm_id = 0, sem_id = 0;

    initSigactions();

    if(argc != EXPECTED_ARGC)
    {
        // No checking needed, exits with error code.
        write(STDERR_FILENO, NOF_INPUTS_ERROR, sizeof(NOF_INPUTS_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    // Create fifo.
    if(mkfifo(FIFO_NAME, ALLOW_READ_WRITE_TO_ALL) < 0)
    {
        // No checking needed, exits with error code.
        write(STDERR_FILENO, MKFIFO_ERROR, sizeof(MKFIFO_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    writePidToFifo();

    // Wait for the queue process' singal.
    waitForSignal();

    printf("got signal from %d!\n", Queue_Pid);

    // Create shared memory.
    shm_addr = createSharedMemory(argv[KEY_FILE_ARG_INDEX], &key, &shm_id);

    // Create semaphore.
    sem_id = createBinarySemaphore(key);

    // Notify the queue.out process.
    if(kill(Queue_Pid, SIGUSR1) < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, KILL_ERROR, sizeof(KILL_ERROR) - 1);
        exit(EXIT_ERROR_CODE);
    }

    queue_length = readQueueLengthFromFifo();

    printf("Got N: %d\n", queue_length);

    // Done using the fifo - delete it.
    deleteFifo();

    // Game Started!
    handleGame(shm_id, shm_addr, sem_id, queue_length);

    deleteResources(shm_id, sem_id);
    return EXIT_OK_CODE;
}

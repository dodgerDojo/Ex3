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

/********************************/

// Defines:

#define FIFO_NAME           ("ex3FIFO")

#define ALLOW_READ_WRITE_TO_ALL (0666)

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
#define SEMCTL_ERROR        ("semctl() failed.\n")
#define KILL_ERROR          ("kill() failed.\n")

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

static key_t createSharedMemory(const char *key_file_name);
static void createBinarySemaphore(key_t sem_key);

static int readQueueLengthFromFifo(void);

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

static key_t createSharedMemory(const char *key_file_name)
{
    const unsigned char KEY_CHAR = 'H';
    const unsigned int SHM_SIZE_IN_BYTES = 1024;

    int shm_id = 0;
    char *p_shm_data = NULL;

    // Generate key from the given file
    key_t shm_key = ftok(key_file_name, KEY_CHAR);
    if(shm_key < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, FTOK_ERROR, sizeof(FTOK_ERROR));
        exit(EXIT_ERROR_CODE);       
    }

    // Create shared memory
    shm_id = shmget(shm_key, SHM_SIZE_IN_BYTES, IPC_CREAT|ALLOW_READ_WRITE_TO_ALL);
    if(shm_id < 0)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SHMGET_ERROR, sizeof(SHMGET_ERROR));
        exit(EXIT_ERROR_CODE);             
    }

    // Connect to the shared memory.
    p_shm_data = (char *)shmat(shm_id, 0, 0);
    if(SHMAT_FAILED == p_shm_data)
    {
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, SHMAT_ERROR, sizeof(SHMAT_ERROR));
        exit(EXIT_ERROR_CODE);      
    }

    return shm_key;
}

static void createBinarySemaphore(key_t sem_key)
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
/********************************/

// Main:

int main(int argc, char *argv[])
{
    const unsigned int EXPECTED_ARGC = 2;
    const unsigned int KEY_FILE_ARG_INDEX = 1;

    key_t key = 0;
    int queue_length = 0;

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
    key = createSharedMemory(argv[KEY_FILE_ARG_INDEX]);

    // Create semaphore.
    createBinarySemaphore(key);

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

    //signalSigusr1ToProcess(proc_pid);
    //queue_length = readFromFifo();

    return EXIT_OK_CODE;
}

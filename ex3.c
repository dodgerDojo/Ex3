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

/********************************/

// Defines:

#define FIFO_NAME           ("ex3FIFO‬‬")

#define OPEN_ERROR          ("open() failed.\n")
#define MKFIFO_ERROR        ("mkfifo() failed.\n")
#define WRITE_ERROR         ("write() failed.\n")
#define CLOSE_ERROR         ("close() failed.\n")
#define SIGACTION_ERROR     ("sigaction() failed.\n")
#define SIGFILLSET_ERROR    ("sigfillset() failed.\n")
#define UNLINK_ERROR        ("unlink() failed.\n")

#define EXIT_ERROR_CODE     (-1)
#define EXIT_OK_CODE        (0)

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
    usrAction.sa_flags = 0;
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
}
/********************************/

// Main:

int main(int argc, char *argv[])
{
    pid_t my_pid = 0;
    int fifo_fd = 0, queue_length = 0;

    initSigactions();

    // Create fifo.
    if(mkfifo(FIFO_NAME, 0777) < 0)
    {
        perror("mkfifo()");
        // No checking needed, exits with error code.
        write(STDERR_FILENO, MKFIFO_ERROR, sizeof(MKFIFO_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    sleep(1);

    // Get self PID.
    my_pid = getpid();

    // Open FIFO.
    fifo_fd = open(FIFO_NAME, O_WRONLY);
    if(fifo_fd < 0)
    {
        perror("open()");
        deleteFifo();
        // No checking needed, exits with error code.
        write(STDERR_FILENO, OPEN_ERROR, sizeof(OPEN_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    if(write(fifo_fd, (void*)&my_pid, sizeof(my_pid)))
    {
        perror("write()");
        // No checking needed, exits with error code.
        write(STDERR_FILENO, WRITE_ERROR, sizeof(WRITE_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    if(close(fifo_fd) < 0)
    {
        perror("close()");
        // No checking needed, exits with error code.
        write(STDERR_FILENO, CLOSE_ERROR, sizeof(CLOSE_ERROR));
        exit(EXIT_ERROR_CODE);
    }

    printf("hey4!");
    fflush(stdout);

    waitForSignal();

    deleteFifo();

    printf("got signal from %d!\n", Queue_Pid);
    fflush(stdout);
    //createSharedMemory();

    //createBinarySemaphore();

    //signalSigusr1ToProcess(proc_pid);

    //queue_length = readFromFifo();

    return EXIT_OK_CODE;
}

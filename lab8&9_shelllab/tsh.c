/* 
 * tsh - A tiny shell program with job control
 * 
 * Yoonhyeok Lee, 20220923
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    /*
    params:
        char *cmdline: raw commandline input

    return:
        void: 
    */
    
    char* argv[MAXARGS];
    int is_background_job, is_built_in;
    int emptyset_result, addset_result, block_result, procmask_result;
    int setpgid_result, addjob_result;
    int external_command_result;
    pid_t pid;
    sigset_t mask;

    // Parse the command line and check it is a background job call
    is_background_job = parseline(cmdline, argv);
    
    // Check if command entered in the shell.
    if (argv[0] == NULL)
        return; // No command entered

    // Check the command is built-in command. If it so, run the corresponding built-in command
    is_built_in = builtin_cmd(argv);
    
    // Not a built-in command
    if (!is_built_in){
        
        // Blocking
        emptyset_result = sigemptyset(&mask); // Clear
        addset_result = sigaddset(&mask, SIGCHLD); // Add SIGCHLD to mask
        procmask_result = sigprocmask(SIG_BLOCK, &mask, NULL); // Block SIGCHLD

        /* FOR DEBUGGING PURPOSE
    
        if (emptyset_result < 0) 
            unix_error("EmptysetError");
    
        if (addset_result < 0) 
            unix_error("AddsetError");
    
        if (procmask_result < 0) 
            unix_error("ProcmaskError");

        */
        pid = fork();

        /* FOR DEBUGGING PURPOSE

        if (pid < 0) 
            unix_error("ForkingError");
        
        */
        
        // This process is child
        if (pid == 0) {
            
            // Unblock
            procmask_result = sigprocmask(SIG_UNBLOCK, &mask, NULL);
            
            setpgid(0, 0); // Seperate process group
            external_command_result = execve(argv[0], argv, environ); // Run external command
            
            /* FOR DEBUGGING PURPOSE

            if (procmask_result < 0)
                unix_error("ProcmaskError");

            */
            
            // External command didn't run properly
            if (external_command_result < 0) {
                printf("%s: Command not found\n", argv[0]); // Print error message
                exit(0);
            }
        }
        
        // This process is parent
        else if (pid > 0) {

            // The process is running on background
            if (is_background_job)
                addjob_result = addjob(jobs, pid, BG, cmdline); // Add job as background process
            
            // The process is running on foreground
            else 
                addjob_result = addjob(jobs, pid, FG, cmdline); // Add job as foreground process

            procmask_result = sigprocmask(SIG_UNBLOCK, &mask, NULL);

            /* FOR DEBUGGING PURPOSE
            
            if (addjob_result == 0)
                unix_error("AddjobError");
            
            if (procmask_result < 0)
                unix_error("ProcmaskError");

            */

            // The process is running on background
            if (is_background_job)
                printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline); // Print running state of background
            
            // The process is running on foreground
            else
                waitfg(pid); // Block until job is not running on foreground 
        }
    }

    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    /*
    params:
        char** argv: arguements vector entered as shell input
    
    return:
        int : 1 if first-entered argument is built-in function, else 0
    */

    // Built-in command quit
    if (strcmp(argv[0], "quit") == 0) {
        exit(0); // Quit program
    }
    
    // Built-in command jobs
    else if (strcmp(argv[0], "jobs") == 0){
        listjobs(jobs); // List jobs
        return 1;
    }

    // Built-in command bg
    else if (strcmp(argv[0], "bg") == 0) {
        do_bgfg(argv); // Run process in background
        return 1;
    }

    // Built-in command fg
    else if (strcmp(argv[0], "fg") == 0) {
        do_bgfg(argv); // Run process in foreground
        return 1;
    }

    return 0;
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    /*
    params: 
        char** argv: arguements vector entered as shell input
    
    return:
        void
    */
    
    struct job_t* job;
    int job_id;
    pid_t pid;
    
    int is_job_id = 1, is_pid = 1;
    
    int i;
    int kill_result;
    
    // Check if the first-entered argument is fg or bg
    if (!(strcmp("fg", argv[0]) == 0 || strcmp("bg", argv[0]) == 0))
        return;
    
    // Check if the job id or pid is entered in the shell.
    if (argv[1] == NULL) {
        printf("%s command requires PID or %%jobid argument\n", argv[0]); // Print error message
        return;
    }

    // Check if the second-entered argument is job id
    is_job_id = argv[1][0] == '%';
    for(i = 1; argv[1][i] != '\0' && argv[1][i] != ' '; i++) 
        is_job_id = is_job_id && '0' <= argv[1][i] && argv[1][i] <= '9';
    
    // Check if the second-entered argument is pid
    for (i = 0; argv[1][i] != '\0' && argv[1][i] != ' '; i++)
        is_pid = is_pid && '0' <= argv[1][i] && argv[1][i] <= '9';

    // The second-entered argument is job id
    if (is_job_id) {
        job_id = atoi(argv[1] + 1); // Type cast job id from string to int
        job = getjobjid(jobs, job_id); // Get job using job id
        
        // The corresponding job of entered jid is not valid 
        if (job == NULL){
            printf("%s: No such job\n", argv[1]); // Print error message
            return;
        }

        // The corresponding job of entered jib is valid
        else
            pid = job->pid; // Get pid of job
    }

    // The second-entered argument is pid
    else if (is_pid) {
        pid = atoi(argv[1]); // Type cast pid from string to int
        job = getjobpid(jobs, pid); // Get job using pid

        // The corresponding job of entered pid is not valid 
        if (job == NULL){
            printf("(%s): No such process\n", argv[1]); // Print error message
            return;
        }
        // The corresponding job of entered jib is valid
        else
            job_id = job->jid; // Get job id of job
    }
    
    // The second-entered argument is not jid nor pid
    else{
        printf("%s: argument must be a PID or %%jobid\n", argv[0]); // Print error message
        return;
    }

    // Kill process
    kill_result = kill(-pid, SIGCONT);
    
    /* FOR DEBUGGING PURPOSE
    
    if (kill_result < 0) {
        unix_error("KillError");
    }
    
    */

    // The first-entered argument is foreground process
    if (strcmp("fg", argv[0]) == 0) {
        job->state = FG; // Change state of the job to foreground process
        waitfg(pid); // Block until job is not running on foreground 
    }

    // The first-entered argument is background process
    else if (strcmp("bg", argv[0]) == 0) {
        job->state = BG; // Change state of the job to background process
        printf("[%d] (%d) %s", job_id, pid, job->cmdline); // Print process message
    }

    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    /*
    params:
        pid_t pid: pid of process that will be running on foreground 
    
    return:
        void:
    */

    struct job_t* job;
    job = getjobpid(jobs, pid);

    // The corresponding job of entered pid is not valid 
    if (job == NULL) 
        return;

    // Wait until job that is running on foreground is not running on foreground
    while ((pid == fgpid(jobs)))
        sleep(1);

    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    /*
    params:
        int sig: signal
    
    return:
        void: 
    */

    struct job_t *job;
    int status;
    pid_t pid;
    int job_id;
    int delete_result;

    // One of state of child processes is changed  
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0) {
        
        // Process is stopped
        if (WIFSTOPPED(status)) {
            // Change the state of corresponding job of pid to stopped 
            job = getjobpid(jobs, pid);
            job->state = ST;

            job_id = pid2jid(pid); // Get job id
            
            printf("Job [%d] (%d) Stopped by signal %d\n", job_id, pid, WSTOPSIG(status)); // Print log messsage
        }
        
        // Process is terminated (invalid)
        else if (WIFSIGNALED(status)) {
            job_id = pid2jid(pid); // Get job id
            
            printf("Job [%d] (%d) terminated by signal %d\n", job_id, pid, WTERMSIG(status)); // Print log messsage
            
            // Delete the corresponding job of pid
            delete_result = deletejob(jobs, pid);

            /* FOR DEBUGGING PURPOSE
            
            if (delete_result == 0) {
                unix_error("DeleteError");
            }

            */
        }
        
        // Process is exited
        else if (WIFEXITED(status)) {
            
            // Delete the corresponding job of pid
            delete_result = deletejob(jobs, pid);

            /* FOR DEBUGGING PURPOSE
            
            if (delete_result == 0) {
                unix_error("DeleteError");
            }

            */
        }   
    }
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    /*
    params:
        int sig: signal
    
    return:
        void: 
    */

    pid_t pid;
    int kill_result;

    pid = fgpid(jobs); // Get pid of foreground job 

    // No foreground job detected
    if (pid == 0)
        return;

    // Kill process
    kill_result = kill(-pid, sig);
    
    /* FOR DEBUGGING PURPOSE
    
    if (kill_result < 0) {
        unix_error("KillError");
    }

    */

    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    /*
    params:
        int sig: signal
    
    return:
        void: 
    */
    pid_t pid;
    int kill_result;

    pid = fgpid(jobs); // Get pid foreground job 

    // No foreground job detected
    if (pid == 0)
        return;

    // Kill process
    kill_result = kill(-pid, sig);

    /* FOR DEBUGGING PURPOSE
    
    if (kill_result < 0) {
        unix_error("KillError");
    }

    */
    
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}




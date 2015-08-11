#include     <stdio.h>  
#include     <stdlib.h>   
#include     <signal.h>
#include     <unistd.h>    
#include     <sys/types.h>  
#include     <sys/stat.h>  
#include     <fcntl.h>   
#include     <termios.h>  
#include     <errno.h>  
#include <sys/wait.h>

/*add by tianfy 2014-12-23*/
#define SIGUSR3		33	/*WAN UP*/
#define SIGUSR4		34	/*WAN DOWN*/
#define SIGUSR5		35	/*LAN UP*/
#define SIGUSR6		36	/*LAN DOWN*/
/*end by tianfy*/

/*
 * Function Name: do_system().
 * Description  : launch shell command in the child process.
 * Parameters   : command - shell command to launch.
 * Returns      : status 0 - OK, -1 - ERROR.
*/
int do_system( char *command, int i_debug )
{
   int pid = 0, status = 0;
   extern char **environ;

   if ( command == 0 )
       return 1;

   pid = fork();
   if ( pid == -1 )
      return -1;

   setenv("PATH", "/sbin:/bin:/usr/sbin:/usr/bin", 1);
   if ( pid == 0 ) {
      char *argv[4];
      argv[0] = "sh";
      argv[1] = "-c";
      argv[2] = command;
      argv[3] = 0;

      if( i_debug )
        printf("%s\r\n", command);
      execve("/bin/sh", argv, environ);
      exit(127);
   }  // if ( pid == 0 )

   /* wait for child process return */
   do {
      if ( waitpid(pid, &status, 0) == -1 ) {
         if ( errno != EINTR )
            return -1;
      } else
         return status;
   } while ( 1 );

   return status;
}

/* Signal handling */
void networkprobe_signal(int sig)
{
	 if (sig == SIGUSR3) {
		/*wan up*/
		do_system("/etc/init.d/network restart", 1);
		usleep(100);
		printf("##__%s, recv signal %d__##", __FUNCTION__, sig);
		do_system("/etc/init.d/firewall.sh restart", 1);
		return;
	}
	else if (sig == SIGUSR4) {
		/*wan down*/
		printf("##__%s, recv signal %d__##", __FUNCTION__, sig);
		return;
	}
	else if (sig == SIGUSR5) {
		/*lan up*/
		do_system("/etc/init.d/dnsmasq restart", 1);
		printf("##__%s, recv signal %d__##", __FUNCTION__, sig);
		return;
	}
	else if (sig == SIGUSR6) {
		/*lan down*/
		printf("##__%s, recv signal %d__##", __FUNCTION__, sig);
		return;
	} else {
		printf("##__%s, recv signal NULL##", __FUNCTION__);
		return;
	}
}

/**@internal
 * @brief Handles SIGCHLD signals to avoid zombie processes
 *
 * When a child process exits, it causes a SIGCHLD to be sent to the
 * process. This handler catches it and reaps the child process so it
 * can exit. Otherwise we'd get zombie processes.
 */
void netprobe_sigchld_handler(int s)
{
	int	status;
	pid_t rc;
	
	printf("Handler for SIGCHLD called. Trying to reap a child");

	rc = waitpid(-1, &status, WNOHANG);

	printf("Handler for SIGCHLD reaped child PID %d", rc);
}

/** Use this function anytime you need to exit */
void netprobe_termination_handler(int s)
{
	/*process exit*/
	printf("NetworkProbe Exiting...");
	exit(s == 0 ? 1 : 0);
}



/** @internal 
 * Registers all the signal handlers
 */
static void
init_signals(void)
{
	struct sigaction sa;

	printf("Initializing signal handlers");
	
	sa.sa_handler = netprobe_sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		printf("sigaction(): %s", strerror(errno));
		exit(1);
	}

	/* Trap SIGPIPE */
	/* This is done so that when libhttpd does a socket operation on
	 * a disconnected socket (i.e.: Broken Pipes) we catch the signal
	 * and do nothing. The alternative is to exit. SIGPIPE are harmless
	 * if not desirable.
	 */
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) == -1) {
		printf("sigaction(): %s", strerror(errno));
		exit(1);
	}

	sa.sa_handler = netprobe_termination_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	/* Trap SIGTERM */
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		printf("sigaction(): %s", strerror(errno));
		exit(1);
	}

	/* Trap SIGQUIT */
	if (sigaction(SIGQUIT, &sa, NULL) == -1) {
		printf("sigaction(): %s", strerror(errno));
		exit(1);
	}

	sa.sa_handler = networkprobe_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	/* WAN UP */
	if (sigaction(SIGUSR3, &sa, NULL) == -1) {
		printf("sigaction(): %s", strerror(errno));
		exit(1);
	}

	/* WAN DOWN */
	if (sigaction(SIGUSR4, &sa, NULL) == -1) {
		printf("sigaction(): %s", strerror(errno));
		exit(1);
	}

	/* LAN UP */
	if (sigaction(SIGUSR5, &sa, NULL) == -1) {
		printf("sigaction(): %s", strerror(errno));
		exit(1);
	}

	/* LAN DOWN */
	if (sigaction(SIGUSR6, &sa, NULL) == -1) {
		printf("sigaction(): %s", strerror(errno));
		exit(1);
	}
}


int main()  
{  
	char tmpbuf[16] = { 0 }; 
	static int countFlag = 0;

	do_system("/etc/init.d/dnsmasq restart", 1);
	init_signals();
	init_netprobe_netlink();
	memset(tmpbuf, 0, sizeof(tmpbuf));
	sprintf(tmpbuf, "%d", getpid());
	if(adddata_tokernel(tmpbuf, sizeof(tmpbuf)) != 0)
		printf("set pid to kernel fail !");
	/* Loop forever */
	while (1) {
		if(countFlag < 3) {
			memset(tmpbuf, 0, sizeof(tmpbuf));
			sprintf(tmpbuf, "%d", getpid());
			if(adddata_tokernel(tmpbuf, sizeof(tmpbuf)) != 0)
				printf("set pid to kernel fail !");
		}
		countFlag += 1;
		sleep(300);	
	}
	
	return 0;
}  

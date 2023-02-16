#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/poll.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/un.h>

#include <sys/wait.h>

#include <fcntl.h>
#include <sys/epoll.h>

#include "init.h"
#include "log.h"
#include "parser.h"
#include "init_parser.h"
#include "list.h"
#include "property_service.h"

// gdb --tui ./main
// ddd ./main

static int signal_fd = -1;
static int signal_recv_fd = -1;

static void sigchld_handler(int signo)
{
	NOTICE("%s: signo = %d\n", __func__, signo);
	
	write(signal_fd, &signo, sizeof(signo));
}

static int wait_for_one_process(int block)
{
	pid_t pid;
	int status;
	struct listnode *node;
    struct command *cmd;
	struct service *svc;
	
	// pid_t pid = wait(&status);
	//pid_t pid = waitpid(-1, &status, WNOHANG);
	while ( (pid = waitpid(-1, &status, block ? 0 : WNOHANG)) == -1 && errno == EINTR );
    if (pid <= 0) return -1;
    INFO("waitpid returned pid %d, status = %08x\n", pid, status);
	
    svc = service_find_by_pid(pid);
    if (!svc) {
        ERROR("untracked pid %d exited\n", pid);
        return 0;
    }

    NOTICE("process '%s', pid %d exited\n", svc->name, pid);
	
    if (!(svc->flags & SVC_ONESHOT) || (svc->flags & SVC_RESTART)) {
        kill(-pid, SIGKILL);
        NOTICE("process '%s' killing any children in process group\n", svc->name);
    }
	
    svc->pid = 0;
    svc->flags &= (~SVC_RUNNING);

    /* oneshot processes go into the disabled state on exit,
     * except when manually restarted. */
    if ((svc->flags & SVC_ONESHOT) && !(svc->flags & SVC_RESTART)) {
        svc->flags |= SVC_DISABLED;
    }

    /* disabled and reset processes do not get restarted automatically */
    if (svc->flags & (SVC_DISABLED | SVC_RESET) )  {
        notify_service_state(svc->name, "stopped");
        return 0;
    }
	
    svc->flags &= (~SVC_RESTART);
    svc->flags |= SVC_RESTARTING;

    /* Execute all onrestart commands for this service. */
    list_for_each(node, &svc->onrestart.commands) {
        cmd = node_to_item(node, struct command, clist);
        cmd->func(cmd->nargs, cmd->args);
    }
    notify_service_state(svc->name, "restarting");
	
	return 0;
}

#define EPOLL_LISTEN_CNT        256
#define EPOLL_LISTEN_TIMEOUT    500

void handle_signal(void)
{
    char tmp[32];

    /* we got a SIGCHLD - reap and restart as needed */
    read(signal_recv_fd, tmp, sizeof(tmp));
    while (!wait_for_one_process(0));
}

int epoll_add_fd(int epfd, int fd)
{
    int ret;
    struct epoll_event event;

    memset(&event, 0, sizeof(event));
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;

    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    if(ret < 0) {
        ERROR("epoll_ctl Add fd:%d error, Error:[%d:%s]", fd, errno, strerror(errno));
        return -1;
    }

    NOTICE("epoll add fd:%d--->%d success", fd, epfd);
    return 0;    
}

int main(int argc, char *argv[])
{
	struct sigaction act;
	
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigchld_handler;
    act.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &act, 0);
	
	init_parse_config_file("./init.rc");
	
	action_for_each_trigger("early-init", action_add_queue_tail);
	
	/* execute all the boot actions to get us started */
    action_for_each_trigger("init", action_add_queue_tail);
	
	/* create epoll fd */
    int epfd = epoll_create(EPOLL_LISTEN_CNT); 
	
	int s[2];
	
    /* create a signalling mechanism for the sigchld handler */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, s) == 0) {
        signal_fd = s[0];
        signal_recv_fd = s[1];
        fcntl(s[0], F_SETFD, FD_CLOEXEC);
        fcntl(s[0], F_SETFL, O_NONBLOCK);
        fcntl(s[1], F_SETFD, FD_CLOEXEC);
        fcntl(s[1], F_SETFL, O_NONBLOCK);
		
		if (epoll_add_fd(epfd, signal_recv_fd)) {
			close(signal_recv_fd);
			return -1;
		}
    }
	handle_signal();

	struct epoll_event events[EPOLL_LISTEN_CNT];    
    memset(events, 0, sizeof(events));

	for(;;) {
		int rc = execute_one_command();
		
		restart_processes();
		
		if (rc != 0) {
			continue;
		}
		
		/* wait epoll event */
        int fd_cnt = epoll_wait(epfd, events, EPOLL_LISTEN_CNT, EPOLL_LISTEN_TIMEOUT); // EPOLL_LISTEN_TIMEOUT （millisecond）
		
		// NOTICE("fd_cnt = %d, epfd = %d\n", fd_cnt, epfd);
		
		for(int i = 0; i < fd_cnt; i++)  {
			if (events[i].events & EPOLLIN)  {
				if (events[i].data.fd == signal_recv_fd) {
					NOTICE("fd_cnt = %d, epfd = %d\n", fd_cnt, epfd);
					
					handle_signal();
				}
			}
		}
	}
    
	return 0;
}

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

#include "init.h"
#include "log.h"
#include "parser.h"
#include "init_parser.h"
#include "list.h"
#include "property_service.h"

// gdb --tui ./main
// ddd ./main

void sigchld_handler(int signo)
{
	int status;
	struct listnode *node;
    struct command *cmd;
	struct service *svc;
	
	// pid_t pid = wait(&status);
	pid_t pid = waitpid(-1, &status, WNOHANG);
	
    svc = service_find_by_pid(pid);
    if (!svc) {
        ERROR("untracked pid %d exited\n", pid);
        return;
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
        return;
    }
	
    svc->flags &= (~SVC_RESTART);
    svc->flags |= SVC_RESTARTING;

    /* Execute all onrestart commands for this service. */
    list_for_each(node, &svc->onrestart.commands) {
        cmd = node_to_item(node, struct command, clist);
        cmd->func(cmd->nargs, cmd->args);
    }
    notify_service_state(svc->name, "restarting");
}

int main(int argc, char *argv[])
{
	struct sigaction act;
	
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigchld_handler;
    act.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &act, 0);
	
	char value[64];
	property_get("USER", value);
	printf("value = %s\n", value);
	
	init_parse_config_file("./init.rc");
	
	action_for_each_trigger("early-init", action_add_queue_tail);
	
	/* execute all the boot actions to get us started */
    action_for_each_trigger("init", action_add_queue_tail);
    
	int count = 0;

	for(;;) {
		execute_one_command();
		
		restart_processes();
		
		//sleep(1);
		
		usleep(5000);	// 50000 微妙
		
		if (++count > 100) {
			break;
		}
	}
    
	return 0;
}

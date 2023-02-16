
#ifndef KEYWORD
/*


int do_domainname(int nargs, char **args);


int do_insmod(int nargs, char **args);

int do_mount_all(int nargs, char **args);
int do_mount(int nargs, char **args);
int do_powerctl(int nargs, char **args);

int do_restorecon(int nargs, char **args);

int do_setcon(int nargs, char **args);
int do_setenforce(int nargs, char **args);
int do_setkey(int nargs, char **args);
int do_setprop(int nargs, char **args);
int do_setrlimit(int nargs, char **args);
int do_setsebool(int nargs, char **args);
int do_swapon_all(int nargs, char **args);


int do_sysclktz(int nargs, char **args);



int do_loglevel(int nargs, char **args);
int do_load_persist_props(int nargs, char **args);

*/
int do_chroot(int nargs, char **args);
int do_chdir(int nargs, char **args);

int do_hostname(int nargs, char **args);
int do_ifup(int nargs, char **args);

int do_exec(int nargs, char **args);

int do_mkdir(int nargs, char **args);
int do_rm(int nargs, char **args);
int do_rmdir(int nargs, char **args);

int do_class_start(int nargs, char **args);
int do_class_stop(int nargs, char **args);
int do_class_reset(int nargs, char **args);

int do_trigger(int nargs, char **args);

int do_export(int nargs, char **args);
int do_start(int nargs, char **args);
int do_stop(int nargs, char **args);
int do_restart(int nargs, char **args);

int do_symlink(int nargs, char **args);

int do_write(int nargs, char **args);
int do_copy(int nargs, char **args);
int do_chmod(int nargs, char **args);
int do_chown(int nargs, char **args);

int do_wait(int nargs, char **args);

#define __MAKE_KEYWORD_ENUM__
#define KEYWORD(symbol, flags, nargs, func) K_##symbol,
enum {
    K_UNKNOWN,
#endif

    KEYWORD(capability,  OPTION,  0, 0)
    KEYWORD(console,     OPTION,  0, 0)
    KEYWORD(critical,    OPTION,  0, 0)
    KEYWORD(setenv,      OPTION,  2, 0)
    KEYWORD(group,       OPTION,  0, 0)
    KEYWORD(ioprio,      OPTION,  0, 0)
    KEYWORD(keycodes,    OPTION,  0, 0)
    KEYWORD(user,        OPTION,  0, 0)
    KEYWORD(seclabel,    OPTION,  0, 0)
    
    KEYWORD(socket,      OPTION,  0, 0)

    KEYWORD(chdir,       COMMAND, 1, do_chdir)
    KEYWORD(chroot,      COMMAND, 1, do_chroot)

    KEYWORD(hostname,    COMMAND, 1, do_hostname)
    KEYWORD(ifup,        COMMAND, 1, do_ifup)
    KEYWORD(exec,        COMMAND, 1, do_exec)

    KEYWORD(class,       OPTION,  0, 0)
    KEYWORD(class_start, COMMAND, 1, do_class_start)
    KEYWORD(class_stop,  COMMAND, 1, do_class_stop)
    KEYWORD(class_reset, COMMAND, 1, do_class_reset)

	KEYWORD(disabled,    OPTION,  0, 0)
	KEYWORD(export,      COMMAND, 2, do_export)
	
    KEYWORD(trigger,     COMMAND, 1, do_trigger)
    KEYWORD(symlink,     COMMAND, 1, do_symlink)
    
    KEYWORD(import,      SECTION, 1, 0)
    
    KEYWORD(on,          SECTION, 0, 0)
    KEYWORD(oneshot,     OPTION,  0, 0)
    KEYWORD(onrestart,   OPTION,  0, 0)
    
    KEYWORD(service,     SECTION, 0, 0) 
    
	KEYWORD(restart,     COMMAND, 1, do_restart)
    
    KEYWORD(start,       COMMAND, 1, do_start)
    KEYWORD(stop,        COMMAND, 1, do_stop)
    
    KEYWORD(rm,          COMMAND, 1, do_rm)
    KEYWORD(rmdir,       COMMAND, 1, do_rmdir)
     KEYWORD(mkdir,       COMMAND, 1, do_mkdir)
           
    KEYWORD(write,       COMMAND, 2, do_write)
    KEYWORD(copy,        COMMAND, 2, do_copy)
    KEYWORD(chmod,       COMMAND, 2, do_chmod)
    KEYWORD(chown,       COMMAND, 2, do_chown)
           
    KEYWORD(wait,        COMMAND, 1, do_wait)
       
#ifdef __MAKE_KEYWORD_ENUM__
    KEYWORD_COUNT,
};
#undef __MAKE_KEYWORD_ENUM__
#undef KEYWORD
#endif


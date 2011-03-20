#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>


struct sub_command {
  pid_t pid;
  int command_index;
  int killed, dead;
} *sub_commands;

int ncommands;
char **commands;


pid_t spawn(char **command)
{
  pid_t pid;
  pid_t parent_pid = getpid();

  pid = fork();
  if ( pid == -1 )
  {
    perror("fork");
    exit(1);
  } else if ( pid ) {
    /* parent */
    return pid;
  } else {
    /* child */
    if ( execvp(command[0], command) )
    {
      perror("exec");
      kill(parent_pid, SIGTERM);
      exit(1);
    }
  }
  /* NOTREACHED */
  return 0;
}

void start()
{
  int i, command_index = 0;

  sub_commands = malloc(sizeof(*sub_commands) * ncommands);
  if ( !sub_commands )
  {
    perror("malloc");
    exit(1);
  }

  for ( i = 0; i < ncommands; i++ )
  {
    sub_commands[i].pid = spawn(commands + command_index);
    sub_commands[i].command_index = command_index;
    sub_commands[i].killed = 0;
    sub_commands[i].dead = 0;
    while ( commands[command_index] )
    {
      command_index++;
    }
    command_index++;
  }
}


void restore(pid_t pid)
{
  int i;
  for ( i = 0; i < ncommands; i++ )
  {
    if ( sub_commands[i].pid == pid && ! sub_commands[i].killed)
    {
      if ( sub_commands[i].killed )
        sub_commands[i].dead = 1;
      else
        sub_commands[i].pid = spawn(commands + sub_commands[i].command_index);
      break;
    }
  }
}


void sigchld_handler(int signum)
{
  int status;
  pid_t pid;
  pid = waitpid(-1, &status, 0);
  restore(pid);
}


void kill_all_children(int signum)
{
  int i;
  for ( i = 0; i < ncommands; i++ )
  {
    sub_commands[i].killed = 1;
    if ( sub_commands[i].pid <= 0 )
    {
      fprintf(stderr, "Warning: something didn't spawn correctly\n");
      sub_commands[i].dead = 1;
    }
    else
    {
      kill(sub_commands[i].pid, SIGTERM);
    }
  }
  sleep(1);
  for ( i = 0; i < ncommands; i++ )
  {
    if ( ! sub_commands[i].dead )
      kill(sub_commands[i].pid, SIGKILL);
  }
  if ( signum != 0 )
    exit(0);
}


void restart_all_commands(int signum)
{
  kill_all_children(0);
  start();
}


int setup_signals()
{
  int ret = 0;
  struct sigaction action;

  action.sa_handler = restart_all_commands;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGTERM);

  ret |= sigaction(SIGHUP, &action, NULL);
  
  action.sa_handler = kill_all_children;
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGHUP);

  ret |= sigaction(SIGTERM, &action, NULL);
  ret |= sigaction(SIGINT, &action, NULL);

  action.sa_handler = sigchld_handler;
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGTERM);
  sigaddset(&action.sa_mask, SIGHUP);

  ret |= sigaction(SIGCHLD, &action, NULL);
  return ret;
}


void parse_commands(int argc, char **argv)
{
  int i;
  ncommands = 1;

  commands = malloc(sizeof(*commands) * (argc + 1));
  for ( i = 0; i < argc; i++ )
  {
    if ( strcmp(argv[i], "---") == 0 )
    {
      commands[i] = NULL;
      ncommands++;
    }
    else
    {
      commands[i] = strdup(argv[i]);
    }
  }
  commands[i+1] = NULL;
}

void help()
{
  int i;
  char *help[] = {
    "USAGE",
    "shepherd [-d] [COMMAND [ARGUMENTS] ---]... COMMAND [ARGUMENTS]",
    "Runs each COMMAND with ARGUMENTS in the background, restarting them when they die for any reason.",
    "",
    "OPTIONS",
    "-d\t\tDaemonize",
    "",
    "SIGNALS",
    "HUP\t\tRestart all processes",
    "KILL, INT\tKill all processes and exit",
    NULL};
  for ( i = 0; help[i]; i++)
    printf("%s\n", help[i]);
}


int main(int argc, char **argv)
{
  int daemonize = 0;

  if ( argc > 1 && strcmp(argv[1], "-d") == 0 )
  {
    daemonize = 1;
    argv++; argc--;
  }
  if ( argc <= 1 || strcmp(argv[1], "-h") == 0 )
  {
    help();
    exit(0);
  }

  if ( daemonize )
    daemon(0, 0);

  parse_commands(argc - 1, argv + 1);

  if ( setup_signals() )
  {
    perror("setup_signals");
    exit(1);
  }

  start();

  /* wait for signals */
  while ( 1 )
    sleep(1000000);
}

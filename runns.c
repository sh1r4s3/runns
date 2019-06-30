#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <syslog.h>

#include <fcntl.h>
#include "runns.h"

#define __USE_GNU
#include <sched.h>

// Emit log message
#define ERR(format, ...) \
    { \
      syslog(LOG_INFO | LOG_DAEMON, "runns.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
      stop_daemon(0); \
    }

#define WARN(format, ...) \
      syslog(LOG_INFO | LOG_DAEMON, "runns.c:%d / warning / " format "\n", __LINE__, ##__VA_ARGS__);

#define INFO(format, ...) \
      syslog(LOG_INFO | LOG_DAEMON, "runns.c:%d / info / " format "\n", __LINE__, ##__VA_ARGS__);

int sockfd;
struct runns_child *childs;
size_t childs_run = 0;

int
drop_priv(const char *username, struct passwd **pw);

void
stop_daemon(int flag);

int
main(int argc, char **argv)
{
  char *username;
  char *program;
  char *netns;
  struct passwd *pw = NULL;
  struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = defsock};
  char **envs;
  struct runns_header hdr;

  // Set safe permissions and create directory.
  umask(0022);
  if (mkdir(RUNNS_DIR, 0755) < 0)
    ERR("Can't create directory %s", RUNNS_DIR);

  // Up daemon socket.
  sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (sockfd == -1)
    ERR("Something gone very wrong, socket = %d", sockfd);
  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    ERR("Can't bind socket to %s", addr.sun_path);

  struct group *group;
  group = getgrnam("users");
  chown(addr.sun_path, 0, group->gr_gid);
  chmod(addr.sun_path, 0775);

  if (!daemon(0, 0))
    perror(0);

  INFO("runns daemon has started");

  while (1)
  {
    if (listen(sockfd, 16) == -1)
      ERR("Can't start listen socket %d (%s)", sockfd, addr.sun_path);

    int data_sockfd = accept(sockfd, 0, 0);
    if (data_sockfd == -1)
      ERR("Can't accept connection");

    int ret = read(data_sockfd, (void *)&hdr, sizeof(hdr));
    if (ret == -1)
      WARN("Can't read data");

    // Stop daemon on demand.
    if (hdr.flag & (RUNNS_STOP | RUNNS_FORCE_STOP))
    {
      puts("Closing");
      close(data_sockfd);
      stop_daemon(hdr.flag);
    }
    // Transfer list of childs
    if (hdr.flag & RUNNS_LIST)
    { // TODO rets
      write(data_sockfd, (void *)&childs_run, sizeof(childs_run));
      for (int i = 0; i < childs_run; i++)
      {
        write(data_sockfd, (void *)&childs[i], sizeof(struct runns_child));
      }
      continue;
    }

    // Read username, program name and network namespace name
    username = (char *)malloc(hdr.user_sz);
    program = (char *)malloc(hdr.prog_sz);
    netns = (char *)malloc(hdr.netns_sz);
    ret = read(data_sockfd, (void *)username, hdr.user_sz);
    ret = read(data_sockfd, (void *)program, hdr.prog_sz);
    ret = read(data_sockfd, (void *)netns, hdr.netns_sz);

    // Read environment variables
    envs = (char **)malloc(++hdr.env_sz*sizeof(char *));
    for (int i = 0; i < hdr.env_sz - 1; i++)
    {
      size_t env_sz;
      ret = read(data_sockfd, (void *)&env_sz, sizeof(size_t));
      envs[i] = (char *)malloc(env_sz);
      ret = read(data_sockfd, (void *)envs[i], env_sz);
      puts(envs[i]);
    }
    envs[hdr.env_sz - 1] = 0;

    close(data_sockfd);

    // Make fork
    pid_t child = fork();
    if (child == -1)
      ERR("Fail on fork");

    // Child
    if (child == 0)
    {
      int netfd = open(netns, 0);
      setns(netfd, CLONE_NEWNET);
      signal(SIGHUP, SIG_IGN);
      drop_priv(username, &pw);
      if (execve(program, 0, (char * const *)envs) == -1)
        perror(0);
    }

    // Save child.
    childs = realloc(childs, sizeof(struct runns_child)*(++childs_run));
    childs[childs_run].pid = child;
    childs[childs_run].name = program;
  }

  return 0;
}

int
drop_priv(const char *username, struct passwd **pw)
{
  *pw = getpwnam(username);
  if (*pw) {
    uid_t uid = (*pw)->pw_uid;
    gid_t gid = (*pw)->pw_gid;

    if (initgroups((*pw)->pw_name, gid) != 0)
      ERR("Couldn't initialize the supplementary group list");
    endpwent();

    if (setgid(gid) != 0 || setuid(uid) != 0) {
      ERR("Couldn't change to '%.32s' uid=%lu gid=%lu",
             username,
             (unsigned long)uid,
             (unsigned long)gid);
    }
    else
      fprintf(stderr, "dropped privs to %s\n", username);
  }
  else
    ERR("Couldn't find user '%.32s'", username);
}

void
stop_daemon(int flag)
{
  INFO("runns daemon going down");
  if (sockfd)
  {
    if (flag & RUNNS_STOP)
    {
      for (pid_t i = 0; i < childs_run; i++)
      {
        int wstatus;
        int pid = childs[i].pid;
        if (waitpid(pid, &wstatus, 0) < 0)
        {
          WARN("Can't wait for child with PID %u", pid);
        }
        else
        {
          if (!WIFEXITED(wstatus))
            WARN("Child terminated with error, exit code: %u", WEXITSTATUS(wstatus));
        }
      }
      free(childs);
    }
    close(sockfd);
    unlink(defsock);
    rmdir(RUNNS_DIR);
  }

  int ret = flag ? flag & (RUNNS_STOP | RUNNS_FORCE_STOP) : EXIT_FAILURE;
  exit(ret);
}

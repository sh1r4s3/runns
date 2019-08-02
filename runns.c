#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pwd.h>
#include <grp.h>
#include <syslog.h>

#include <fcntl.h>
#include "runns.h"

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
unsigned int childs_run = 0;

int
drop_priv(uid_t _uid, struct passwd **pw);

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
  int *glob_pid = mmap(NULL, sizeof(glob_pid), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (!glob_pid)
    ERR("Can't allocate memory");

  if (daemon(0, 0))
    ERR("Can't daemonize the process");

  // Set safe umask and create directory.
  umask(0022);
  if (mkdir(RUNNS_DIR, 0755) < 0)
    ERR("Can't create directory %s", RUNNS_DIR);

  // Up daemon socket.
  sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (sockfd == -1)
    ERR("Something gone very wrong, socket = %d", sockfd);
  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    ERR("Can't bind socket to %s", addr.sun_path);
  // Switch permissions and group.
  struct group *group;
  group = getgrnam("runns");
  if (!group)
    ERR("Can't get runns group\n");
  chown(addr.sun_path, 0, group->gr_gid);
  chmod(addr.sun_path, 0775);

  INFO("runns daemon has started");

  while (1)
  {
    if (listen(sockfd, 16) == -1)
      ERR("Can't start listen socket %d (%s)", sockfd, addr.sun_path);

    int data_sockfd = accept(sockfd, 0, 0);
    if (data_sockfd == -1)
      ERR("Can't accept connection");
    struct ucred cred;
    socklen_t cred_len = (socklen_t)sizeof(struct ucred);
    if (getsockopt(data_sockfd, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) == -1)
    {
      WARN("Can't get user credentials");
      close(data_sockfd);
      continue;
    }
    
    int ret = read(data_sockfd, (void *)&hdr, sizeof(hdr));
    if (ret == -1)
      WARN("Can't read data");

    // Stop daemon on demand.
    if (hdr.flag & RUNNS_STOP)
    {
      INFO("closing");
      close(data_sockfd);
      stop_daemon(hdr.flag);
    }
    // Transfer list of childs
    if (hdr.flag & RUNNS_LIST)
    { // TODO rets
      INFO("uid=%d ask for pid list", cred.uid);
      // Count the number of jobs for uid
      unsigned int jobs = 0;
      for (int i = 0; i < childs_run; i++)
      {
        if (childs[i].uid == cred.uid)
          ++jobs;
      }
        
      write(data_sockfd, (void *)&jobs, sizeof(jobs));
      for (unsigned int i = 0; i < jobs; i++)
      {
        write(data_sockfd, (void *)&childs[i], sizeof(struct runns_child));
      }
      close(data_sockfd);
      continue;
    }

    // Read program name and network namespace name
    program = (char *)malloc(hdr.prog_sz);
    netns = (char *)malloc(hdr.netns_sz);
    ret = read(data_sockfd, (void *)program, hdr.prog_sz);
    ret = read(data_sockfd, (void *)netns, hdr.netns_sz);
    INFO("uid=%d program=%s netns=%s", cred.uid, program, netns);

    // Read environment variables
    envs = (char **)malloc(++hdr.env_sz*sizeof(char *));
    for (int i = 0; i < hdr.env_sz - 1; i++)
    {
      size_t env_sz;
      ret = read(data_sockfd, (void *)&env_sz, sizeof(size_t));
      envs[i] = (char *)malloc(env_sz);
      ret = read(data_sockfd, (void *)envs[i], env_sz);
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
      // Detach child from parent
      setsid();
      child = fork();
      if (child != 0)
      {
        *glob_pid = child;
        exit(0);
      }
      int netfd = open(netns, 0);
      setns(netfd, CLONE_NEWNET);
      drop_priv(cred.uid, &pw);
      if (execve(program, 0, (char * const *)envs) == -1)
        perror(0);
    }
    else
      waitpid(child, 0, 0);

    // Save child.
    INFO("Forked %d", *glob_pid);
    childs = realloc(childs, sizeof(struct runns_child)*(++childs_run));
    childs[childs_run - 1].uid = cred.uid;
    childs[childs_run - 1].pid = *glob_pid;
  }

  return 0;
}

int
drop_priv(uid_t _uid, struct passwd **pw)
{
  *pw = getpwuid(_uid);
  if (*pw) {
    uid_t uid = (*pw)->pw_uid;
    gid_t gid = (*pw)->pw_gid;

    if (initgroups((*pw)->pw_name, gid) != 0)
      ERR("Couldn't initialize the supplementary group list");
    endpwent();

    if (setgid(gid) != 0 || setuid(uid) != 0) {
      ERR("Couldn't change to '%.32s' uid=%lu gid=%lu",
          (*pw)->pw_name,
          (unsigned long)uid,
          (unsigned long)gid);
    }
    else
      fprintf(stderr, "dropped privs to %s\n", (*pw)->pw_name);
  }
  else
    ERR("Couldn't find user '%.32s'", (*pw)->pw_name);
}

void
stop_daemon(int flag)
{
  INFO("runns daemon going down");
  if (sockfd)
  {
    free(childs);
    close(sockfd);
    unlink(defsock);
    rmdir(RUNNS_DIR);
  }

  int ret = flag ? flag & RUNNS_STOP : EXIT_FAILURE;
  exit(ret);
}

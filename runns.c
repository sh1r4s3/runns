#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>

#define __USE_GNU
#include <sched.h>

#include <fcntl.h>
#include <errno.h>

#define ERR(format, ...) \
    fprintf(stderr, "runns.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
    exit(1);

int
drop_priv(const char *username, struct passwd **pw)
{
  *pw = getpwnam(username);
  if (*pw) {
    uid_t uid = (*pw)->pw_uid;
    gid_t gid = (*pw)->pw_gid;

    if (initgroups((*pw)->pw_name, gid) != 0)
      ERR("Couldn't initialize the supplementary group list")
    endpwent();

    if (setgid(gid) != 0 || setuid(uid) != 0) {
      ERR("Couldn't change to '%.32s' uid=%lu gid=%lu",
             username,
             (unsigned long)uid,
             (unsigned long)gid)
    }
    else
      fprintf(stderr, "dropped privs to %s\n", username);
  }
  else
    ERR("Couldn't find user '%.32s'", username);
}

int
main(int argc, char **argv)
{
  struct passwd *pw = NULL;
  const char *username = "nik";
  const char *program_name = "runns";
  char *arg[] = {"xterm", 0};
  char *env[] = {"PATH=/usr/bin:/usr/sbin:/bin:/sbin", 0};

  // fork
  pid_t child = fork();
  if (child == -1)
    ERR("Fail on fork")

  if (child != 0) {
    int netfd = open("/var/run/netns/blue", 0);
    setns(netfd, CLONE_NEWNET);
    drop_priv(username, &pw);
    execve("/bin/bash", arg, env);
  }

  // Drop privilages
  execve("myuid", 0, 0);
  int wst = 0;
  if (child != 0)
    wait(&wst);

  return 0;
}

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
    { fprintf(stderr, "runns.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__); \
    exit(EXIT_FAILURE); }

#define WARN(format, ...) \
    fprintf(stderr, "runns.c:%d / errno=%d / " format "\n", __LINE__, errno, ##__VA_ARGS__)


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

int
main(int argc, char **argv)
{
  struct runns_header
  {
    int usersz;
    int progsz;
  } hdr;
  char *username;
  char *program;
  struct passwd *pw = NULL;
  const char *program_name = "runns";
  struct sockaddr_un addr = {.sun_family = AF_UNIX, .sun_path = "/tmp/runns.socket"};
  char *arg[] = {"xterm", 0};
  char *env[] = {"PATH=/usr/bin:/usr/sbin:/bin:/sbin", 0};

  // Up socket
  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd == -1)
    ERR("Something gone very wrong, socket = %d", sockfd);
  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    ERR("Can't bind socket to %s", addr.sun_path);

  struct group *group;
  group = getgrnam("users");
  chown(addr.sun_path, 0, group->gr_gid);
  chmod(addr.sun_path, 0775);

  if (listen(sockfd, 16) == -1)
    ERR("Can't start listen socket %d (%s)", sockfd, addr.sun_path);

  // Main loop
  while(1)
  {
    int data_sockfd = accept(sockfd, 0, 0);
    if (data_sockfd == -1)
      ERR("Can't accept connection");

    int ret = read(data_sockfd, (void *)&hdr, sizeof(hdr));
    if (ret == -1)
      WARN("Can't read data");

    username = (char *)malloc(hdr.usersz);
    program = (char *)malloc(hdr.progsz);
    ret = read(data_sockfd, (void *)username, hdr.usersz);
    ret = read(data_sockfd, (void *)program, hdr.progsz);
    close(data_sockfd);
    break;
  }
  ERR("username = %s, program = %s", username, program);

  // fork
  pid_t child = fork();
  if (child == -1)
    ERR("Fail on fork");

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

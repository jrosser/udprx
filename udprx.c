#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include <libdaemon/dlog.h>

#define BUFLEN 2048
#define PORT 8888

static int logfd=-1;
static int lasthour=-1;

//create path
int mkpath(char* file_path, mode_t mode) {
  assert(file_path && *file_path);
  char* p;
  for (p=strchr(file_path+1, '/'); p; p=strchr(p+1, '/')) {
    *p='\0';
    if (mkdir(file_path, mode)==-1) {
      if (errno!=EEXIST) { *p='/'; return -1; }
    }
    *p='/';
  }
  return 0;
}

static void openlogfile()
{
  const char *dir="/var/log/udprx/raw/";
  char timestr[80];
  char filename[80];
  int ret;

  time_t t;
  struct tm tmp;

  //filename is YYYY:DD:MM:HH
  t = time(NULL);
  gmtime_r(&t, &tmp);
  strftime(timestr, 80, "%Y/%m/%F-%H", &tmp);

  //combine directory and filename
  snprintf(filename, 80, "%s%s", dir, timestr);

  //create directory
  ret=mkpath(filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if(ret<0) daemon_log(LOG_ERR, "mkpath failed: %s", strerror(errno));

  //open file
  logfd=open(filename,
    O_CREAT | O_APPEND | O_WRONLY,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  if(logfd<0) daemon_log(LOG_ERR, "open failed: %s", strerror(errno));
}

static void logmessage(char *buf, ssize_t buflen)
{
  int i;
  time_t t;
  struct tm tmp;
  char timestr[80];
  char *datastr = malloc((buflen*2+1));
  char logstr[2048];
  ssize_t written;

  //create time string
  t = time(NULL);
  gmtime_r(&t, &tmp);
  strftime(timestr, 80, "%a, %d %b %Y %T %z", &tmp);

  //create string representation of data bytes
  for(i=0; i<buflen; i++) {
    sprintf(datastr+(i*2), "%02x", buf[i] & 0xff);
  }

  //combine time and data strings
  snprintf(logstr, 2048, "%s %s\n", timestr, datastr);

  //check if hour has rolled
  if(lasthour!=tmp.tm_hour) {
    if(logfd > 0) {
      close(logfd);
      logfd=-1;
    }
  }

  //write to log file
  if(logfd < 0)
    openlogfile();

  if(logfd > 0) {
    written=write(logfd, logstr, strlen(logstr));
    if(written<0) daemon_log(LOG_ERR, "write failed: %s", strerror(errno));
  }

  lasthour=tmp.tm_hour;

  free(datastr);
}

int opensocket()
{
  int s;
  int ret;
  struct sockaddr_in si_me;

  s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(s<0)
    return s;

  memset((char *) &si_me, 0, sizeof(si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(PORT);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);

  ret=bind(s, (const struct sockaddr *)&si_me, sizeof(si_me));
  if(ret<0)
    return ret;

  return s;
}

int receive(int s)
{
  char buf[BUFLEN];
  ssize_t nbytes;

  nbytes=recvfrom(s, buf, BUFLEN, 0, NULL, NULL);
  if(nbytes<0)
  {
    daemon_log(LOG_ERR, "recvfrom failed: %s", strerror(errno));
    return 1;
  }

  logmessage(buf, nbytes);

  return 0;
}

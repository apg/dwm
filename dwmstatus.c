#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

static Display *dpy;

char *tznyc = "America/New_York";

char *
smprintf(char *fmt, ...)
{
  va_list fmtargs;
  char *ret;
  int len;

  va_start(fmtargs, fmt);
  len = vsnprintf(NULL, 0, fmt, fmtargs);
  va_end(fmtargs);

  ret = malloc(++len);
  if (ret == NULL) {
    perror("malloc");
    exit(1);
  }

  va_start(fmtargs, fmt);
  vsnprintf(ret, len, fmt, fmtargs);
  va_end(fmtargs);

  return ret;
}

void
settz(char *tzname)
{
  setenv("TZ", tzname, 1);
}

void
mktimes(char *buf, int len, char *fmt, char *tzname)
{
  time_t tim;
  struct tm *timtm;

  bzero(buf, sizeof(buf));
  settz(tzname);
  tim = time(NULL);
  timtm = localtime(&tim);
  if (timtm == NULL) {
    perror("localtime");
    exit(1);
  }

  if (!strftime(buf, len-1, fmt, timtm)) {
    fprintf(stderr, "strftime == 0\n");
    exit(1);
  }
}

void
setstatus(char *str)
{
  XStoreName(dpy, DefaultRootWindow(dpy), str);
  XSync(dpy, False);
}

void
loadavg(char *buf, int len)
{
  double avgs[3];

  if (getloadavg(avgs, 3) < 0) {
    perror("getloadavg");
    exit(1);
  }

  snprintf(buf, len-1, "%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

int
main(void)
{
  char tmnyc[128];
  char avgs[128];
  char *status;

  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "dwmstatus: cannot open display.\n");
    return 1;
  }

  for (;;sleep(2)) {
    loadavg(avgs, 128);
    mktimes(tmnyc, 128, "%a, %B %d, %R", tznyc);
    status = smprintf("[L: %s | %s]", avgs, tmnyc);
                      
    setstatus(status);
    free(status);
  }

  XCloseDisplay(dpy);

  return 0;
}


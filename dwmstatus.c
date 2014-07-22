#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
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

int
mktimes(char *buf, int len, char *fmt, char *tzname)
{
  time_t tim;
  struct tm *timtm;
  int warn = 0; // TODO: make this much more general.

  bzero(buf, sizeof(buf));
  settz(tzname);
  tim = time(NULL);
  timtm = localtime(&tim);
  if (timtm == NULL) {
    perror("localtime");
    exit(1);
  }
  if (timtm->tm_hour == 17 && timtm->tm_min > 45) {
    warn = 1;
  }
  else if (timtm->tm_hour == 18 && timtm->tm_min < 15) {
    warn = 1;
  }

  if (!strftime(buf, len-1, fmt, timtm)) {
    fprintf(stderr, "strftime == 0\n");
    exit(1);
  }

  return warn;
}

int
readcontents(char *fn, char *buffer, int len)
{
  int retval = 1;
  FILE *tmp;
  tmp = fopen(fn, "r");
  if (tmp != NULL) {
    buffer = fgets(buffer, len, tmp);
    if (buffer == NULL) {
      retval = 0;
    }
    fclose(tmp);
  }
  else {
    retval = 0;
  }
  return retval;
}

void
setstatus(char *str)
{
  XStoreName(dpy, DefaultRootWindow(dpy), str);
  XSync(dpy, False);
}

static char *batdraw = " .:=#";

void
drawbattery(char *status, int percent, char *bat, int len)
{
  char charging = '-';
  if (strncasecmp(status, "charging", 8) == 0) {
    charging = '+';
  }
  if (percent == 100) {
    strncpy(bat, "{ |####]", len);
  }
  else if (percent >= 75) {
    snprintf(bat, len, "{%c|###%c]", charging, batdraw[(int)((percent - 75) / 5)]);
  }
  else if (percent >= 50) {
    snprintf(bat, len, "{%c|##%c ]", charging, batdraw[(int)((percent - 50) / 5)]);
  }
  else if (percent >= 25) {
    snprintf(bat, len, "{%c|#%c  ]", charging, batdraw[(int)((percent - 25) / 5)]);
  }
  else {
    snprintf(bat, len, "{%c|%c   ]", charging, batdraw[(int)(percent / 5)]);
  }
}

void
power(char *buf, int len)
{
  int ok;
  char status[32];
  char charge_full[32];
  char charge_now[32];
  char battery[32];
  int cf, cn, percent;

  ok = readcontents("/sys/class/power_supply/BAT0/status", status, 32);
  if (!ok) {
    strncpy(buf, "Unknown", 8);
    return;
  }
  else if (strncasecmp(status, "full", 4) == 0) {
    strncpy(buf, "{####] 100%", len);
    return;
  }
  else {
    for (cf = 0; cf < 32; cf++) {
      if (!isalpha(status[cf])) {
        status[cf] = '\0';
        break;
      }
    }
  }

  ok = readcontents("/sys/class/power_supply/BAT0/charge_full",
                    charge_full, 32);
  if (!ok) {
    goto fail;
  }

  cf = atoi(charge_full);
  if (cf > 0) {
    ok = readcontents("/sys/class/power_supply/BAT0/charge_now",
                      charge_now, 32);
    if (!ok) {
      goto fail;
    }
    cn = atoi(charge_now);
    percent = (float) cn * 100 / cf;
    drawbattery(status, percent, battery, 32);

    snprintf(buf, len, "%s %d%%", battery, percent);
    return;
  }

 fail:
  strncpy(buf, status, len);
  return;
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
  char pow[128];
  char *status;
  int warn = 0;

  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "dwmstatus: cannot open display.\n");
    return 1;
  }

  for (;;sleep(4)) {
    loadavg(avgs, 128);
    warn = mktimes(tmnyc, 128, "%a, %B %d, %R", tznyc);
    power(pow, 128);
    if (warn) {
      status = smprintf("[%s | L: %s | \x04%s\x01]", pow, avgs, tmnyc);
    }
    else {
      status = smprintf("[%s | L: %s | %s]", pow, avgs, tmnyc);
    }

    setstatus(status);
    free(status);
  }

  XCloseDisplay(dpy);

  return 0;
}


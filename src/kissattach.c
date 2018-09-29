// Taken from ax25-tools 0.0.10-rc4 kissattach.c
// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#define __USE_XOPEN
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <netdb.h>
#include <syslog.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netax25/ax25.h>

#define FALSE 0
#define TRUE 1

static struct speed_struct {
    int     user_speed;
    speed_t termios_speed;
} speed_table[] = {
    {300,		B300},
    {600,		B600},
    {1200,		B1200},
    {2400,		B2400},
    {4800,		B4800},
    {9600,		B9600},
    {19200,		B19200},
    {38400,		B38400},
#ifdef  B57600
    {57600,		B57600},
#endif
#ifdef  B115200
    {115200,	B115200},
#endif
#ifdef  B230400
    {230400,	B230400},
#endif
#ifdef  B460800
    {460800,	B460800},
#endif
    {-1,		B0}
};

static int tty_speed(int fd, int speed)
{
    struct termios term;
    struct speed_struct *s;

    for (s = speed_table; s->user_speed != -1; s++)
        if (s->user_speed == speed)
            break;

    if (s->user_speed == -1) {
        fprintf(stderr, "tty_speed: invalid speed %d\n", speed);
        return FALSE;
    }

    if (tcgetattr(fd, &term) == -1) {
        perror("tty_speed: tcgetattr");
        return FALSE;
    }

    cfsetispeed(&term, s->termios_speed);
    cfsetospeed(&term, s->termios_speed);

    if (tcsetattr(fd, TCSANOW, &term) == -1) {
        perror("tty_speed: tcsetattr");
        return FALSE;
    }

    return TRUE;
}

static int ax25_aton_entry(const char *name, char *buf)
{
    int ct   = 0;
    int ssid = 0;
    const char *p = name;
    char c;

    while (ct < 6) {
        c = toupper(*p);

        if (c == '-' || c == '\0')
            break;

        if (!isalnum(c)) {
            printf("axutils: invalid symbol in callsign '%s'\n", name);
            return -1;
        }

        buf[ct] = c << 1;

        p++;
        ct++;
    }

    while (ct < 6) {
        buf[ct] = ' ' << 1;
        ct++;
    }

    if (*p != '\0') {
        p++;

        if (sscanf(p, "%d", &ssid) != 1 || ssid < 0 || ssid > 15) {
            printf("axutils: SSID must follow '-' and be numeric in the range 0-15 - '%s'\n", name);
            return -1;
        }
    }

    buf[6] = ((ssid + '0') << 1) & 0x1E;

    return 0;
}

static int setifcall(int fd, char *name)
{
    char call[7];

    if (ax25_aton_entry(name, call) == -1)
        return FALSE;

    if (ioctl(fd, SIOCSIFHWADDR, call) != 0) {
        close(fd);
        perror("SIOCSIFHWADDR");
        return FALSE;
    }

    return TRUE;
}


static int startiface(char *dev, int mtu, int allow_broadcast)
{
    struct ifreq ifr;
    int fd;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return FALSE;
    }

    strcpy(ifr.ifr_name, dev);

    ifr.ifr_mtu = mtu;

    if (ioctl(fd, SIOCSIFMTU, &ifr) < 0) {
        perror("SIOCSIFMTU");
        return FALSE;
    }

    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("SIOCGIFFLAGS");
        return FALSE;
    }

    ifr.ifr_flags &= IFF_NOARP;
    ifr.ifr_flags |= IFF_UP;
    ifr.ifr_flags |= IFF_RUNNING;
    if (allow_broadcast)
        ifr.ifr_flags |= IFF_BROADCAST; /* samba broadcasts are a pain.. */
    else
        ifr.ifr_flags &= ~(IFF_BROADCAST); /* samba broadcasts are a pain.. */

    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
        perror("SIOCSIFFLAGS");
        return FALSE;
    }

    close(fd);

    return TRUE;
}

int32_t kissattach(
        char *callsign,
        int32_t speed,
        int32_t mtu,
        char *kttyname,
        int32_t allow_broadcast)
{
    int  fd;
    int  disc = N_AX25;
    char dev[64];

    if ((fd = open(kttyname, O_RDONLY | O_NONBLOCK)) == -1) {
        perror("open");
        return 0;
    }

    if (speed != 0 && !tty_speed(fd, speed))
        return 0;

    if (ioctl(fd, TIOCSETD, &disc) == -1) {
        //Error setting line discipline
        perror("TIOCSETD");
        fprintf(stderr, "Are you sure you have enabled %s support in the kernel\n",
                disc == N_AX25 ? "MKISS" : "6PACK");
        fprintf(stderr, "or, if you made it a module, that the module is loaded?\n");
        return 0;
    }

    if (ioctl(fd, SIOCGIFNAME, dev) == -1) {
        perror("SIOCGIFNAME");
        return 0;
    }

    if (!setifcall(fd, callsign))
        return 0;

    /* Now set the encapsulation */
    int v = 4;
    if (ioctl(fd, SIOCSIFENCAP, &v) == -1) {
        perror("SIOCSIFENCAP");
        return 0;
    }

    /* ax25 ifaces should not really need to have an IP address assigned to */
    if (!startiface(dev, mtu, allow_broadcast))
        return 0;

    return 1;
}

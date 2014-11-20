/*
 * Copyright 2005 - Xavier Beaudouin
 * 
 * This code is released under MPL 1.1 License.
 * 
 * Note that :
 *  "This product includes software developed by the
 *   University of California, Berkeley and its
 *   Contributors."
 * 
 * If you find this software usefull and you like it,
 * please don't hesitate to contribute his author.
 * 
 * Homepage : http://www.oav.net/project/syslogger/
 * 
 */
/*
 * $Id: main.c,v 1.1 2005/05/05 19:07:02 kiwi Exp $
 */
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Syslog Stuff
 */
#define	SYSLOG_NAMES
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/cdefs.h>
#include <ctype.h>
#include <err.h>
#include <netdb.h>

#define BUFSIZE        65535

struct socks {
    int sock;
    int addrlen;
    struct sockaddr_storage addr;
};

#ifdef INET6
int	family = PF_UNSPEC;	/* protocol family (IPv4, IPv6 or both) */
#else
int	family = PF_INET;	/* protocol family (IPv4 only) */
#endif
int	send_to_all = 0;		/* send message to all IPv4/IPv6 addresses */

/*
 * Usage of this program
 */
static void usage(void)
{
	(void)fprintf(stderr, "usage: %s\n",
#ifdef INET6
		"syslogger [-v46Ai] [-h host] [-p pri] [-t tag]"
#else
		"syslogger [-v4Ai] [-h host] [-p pri] [-t tag]"
#endif
		);
	exit (1);
}


/*
 * Version
 */
static void version(void)
{
	(void)fprintf(stderr, "%s\n",
		"syslogger Version 1.0 - (C)Copyright 2005 Xavier Beaudouin"
		);
	exit (1);
}

/*
 *  Send the message to syslog, either on the local host, or on a remote host
 */
void logmessage(int pri, char *host, char *buf)
{
	static struct socks *socks;
	static int nsock = 0;
	struct addrinfo hints, *res, *r;
	char *line;
	int maxs, len, sock, error, i, lsent;
	
	if (host == NULL) {
		syslog(pri, "%s", buf);
		return;
	}

	if (nsock <= 0) {	/* set up socket stuff */
		/* resolve hostname */
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = family;
		hints.ai_socktype = SOCK_DGRAM;
		error = getaddrinfo(host, "syslog", &hints, &res);
		if (error == EAI_SERVICE) {
			warnx ("syslog/udp: unknown service");	/* not fatal */
			error = getaddrinfo(host, "514", &hints, &res);
		}
		if (error)
			errx(1, "%s: %s", gai_strerror(error), host);
		/* count max number of sockets we may open */
		for (maxs = 0, r = res; r; r = r->ai_next, maxs++);
		socks = malloc(maxs * sizeof(struct socks));
		if (!socks)
			errx(1, "couldn't allocate memory for sockets");
		for (r = res; r; r = r->ai_next) {
			sock = socket(r->ai_family, r->ai_socktype,
				      r->ai_protocol);
			if (sock < 0)
				continue;
			memcpy(&socks[nsock].addr, r->ai_addr, r->ai_addrlen);
			socks[nsock].addrlen = r->ai_addrlen;
			socks[nsock++].sock = sock;
		}
		freeaddrinfo(res);
		if (nsock <= 0)
			errx(1, "socket");
	}

	if ((len = asprintf(&line, "<%d>%s", pri, buf)) == -1)
		errx(1, "asprintf");

	for (i = 0; i < nsock; ++i) {
		lsent = sendto(socks[i].sock, line, len, 0,
			       (struct sockaddr *)&socks[i].addr,
			       socks[i].addrlen);
		if (lsent == len && !send_to_all)
			break;
	}
	if (lsent != len) {
		if (lsent == -1)
			warn ("sendto");
		else
			warnx ("sendto: short send - %d bytes", lsent);
	}

	free(line);
}

int decode(char *name, CODE *codetab)
{
	CODE *c;

	if (isdigit(*name))
		return (atoi(name));

	for (c = codetab; c->c_name; c++)
		if (!strcasecmp(name, c->c_name))
			return (c->c_val);

	return (-1);
}

/*
 *  Decode a symbolic name to a numeric value
 */
int pencode(char *s)
{
	char *save;
	int fac, lev;

	for (save = s; *s && *s != '.'; ++s);
	if (*s) {
		*s = '\0';
		fac = decode(save, facilitynames);
		if (fac < 0)
			errx(1, "unknown facility name: %s", save);
		*s++ = '.';
	}
	else {
		fac = 0;
		s = save;
	}
	lev = decode(s, prioritynames);
	if (lev < 0)
		errx(1, "unknown priority name: %s", save);
	return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}

/*
 * Main stuff
 */
int main (int argc, char **argv)
{
	int ch, logflags, pri;
	char *tag, *host;
	
	char buf[BUFSIZE], line[BUFSIZE];
	int  nRead;

	/* 
	 * Initialize stuff
	 */
	tag = NULL;
	host = NULL;
	pri = LOG_USER | LOG_NOTICE;
	logflags = 0;
	unsetenv("TZ");
	
#ifdef TPF
    /* set up signal handling to avoid default OPR-I007777 dump */
    signal(SIGPIPE, exit);
    signal(SIGTERM, exit);
#endif

	while ((ch = getopt(argc, argv, "vV46Aih:p:t:")) != -1)
		switch((char)ch) {
			case 'v':
			case 'V':
				version();
				break;
				
			case '4':
				family = PF_INET;
				break;
#ifdef INET6
			case '6':
				family = PF_INET6;
				break;
#endif
			case 'A':
				send_to_all++;
				break;
				
			case 'h':
				host = optarg;
				break;
			
			case 'i':
				logflags |= LOG_PID;
				break;
				
			case 'p':
				pri = pencode(optarg);
				break;
				
			case 't':
				tag = optarg;
				break;
				
			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;

    /*
     * Setup for logging
     */
    openlog(tag ? tag : getlogin(), logflags, 0);
    (void) fclose(stdout);

    for (;;) {
    	    nRead = 0;
        nRead = read(0, buf, sizeof (buf));
        if (nRead == 0)
            exit(3);
        if (nRead < 0)
            if (errno != EINTR)
               exit(4);
        (void)strncpy(line, buf, nRead-1);
        line[nRead-1] = '\0';
        logmessage(pri, host, line);
    }
    /* We never get here, but suppress the compile warning */
    return (0);
}

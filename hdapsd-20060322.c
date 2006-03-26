/*
 * hdapsd.c - Read from the HDAPS (HardDrive Active Protection System)
 *            and protect the drive if motion over threshold...
 *
 *            Derived from pivot.c by Robert Love.
 *
 * Copyright (C) 2005-2006 Jon Escombe <lists@dresco.co.uk>
 *                         Robert Love <rml@novell.com>
 *                         Shem Multinymous <multinymous@gmail.com>
 *
 * "Why does that kid keep dropping his laptop?"
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/mman.h>

#define SYSFS_POSITION_FILE	"/sys/devices/platform/hdaps/position"

#define BUF_LEN                 32

#define FREEZE_SECONDS          1    /* period to freeze disk */
#define REFREEZE_SECONDS        0.1  /* period after which to re-freeze disk */
#define FREEZE_EXTRA_SECONDS    4    /* additional timeout for kernel timer */
#define FREQ_HZ                 50   /* sampling frequency */
#define SIGUSR1_SLEEP_SEC       8    /* how long to sleep upon SIGUSR1 */

/* Magic threshold tweak factors, determined experimentally to make a
 * threshold of 10-20 behave reasonably.
 */
#define VELOC_ADJUST            30.0
#define ACCEL_ADJUST            (VELOC_ADJUST * 60)
#define AVG_VELOC_ADJUST        3.0

/* History depth for velocity average, in seconds */
#define AVG_DEPTH_SEC           0.3

static verbose = 0;
static pause_now = 0;

/*
 * read_position - read the (x,y) position pair from hdaps.
 *
 * We open and close the file on every invocation, which is lame but due to
 * several features of sysfs files:
 *
 *	(a) Sysfs files are seekable.
 *	(b) Seeking to zero and then rereading does not seem to work.
 *
 * If I were king--and I will be one day--I would have made sysfs files
 * nonseekable and only able to return full-size reads.
 */
static int read_position (int *x, int *y)
{
	char buf[BUF_LEN];
	int fd, ret;

	fd = open (SYSFS_POSITION_FILE, O_RDONLY);
	if (fd < 0) {
		perror ("open(\"" SYSFS_POSITION_FILE "\")");
		return fd;
	}	

	ret = read (fd, buf, BUF_LEN);
	if (ret < 0) {
		perror ("read(\"" SYSFS_POSITION_FILE "\")");
		goto out;
	} else if (ret == 0) {
		fprintf (stderr, "error: unexpectedly read zero!\n");
		ret = 1;
		goto out;
	}
	ret = 0;

	if (sscanf (buf, "(%d,%d)\n", x, y) != 2)
		ret = 1;

out:
	if (close (fd))
		perror ("close(\"" SYSFS_POSITION_FILE "\")");

	return ret;
}

/*
 * read_protect() - read the current protection status
 */
static int read_protect (const char *path)
{
	int fd, ret;
	char buf[BUF_LEN];

	memset(buf, 0, sizeof(buf));
	fd = open (path, O_RDONLY);
	ret = read (fd, buf, BUF_LEN);
	close (fd);
	if (ret<0)
		return (ret);
	return atoi(buf);
}

/*
 * write_protect() - park/unpark
 */
static int write_protect (const char *path, int val)
{
	int fd, ret;
	char buf[BUF_LEN];

	snprintf(buf, BUF_LEN, "%d", val);

	fd = open (path, O_WRONLY);
	if (fd < 0) {
		perror ("open");
		return fd;
	}	

	ret = write (fd, buf, strlen(buf));

	if (ret < 0) {
		perror ("write");
		goto out;
	}
	ret = 0;

out:
	if (close (fd))
		perror ("close");

	return ret;
}

double get_utime (void)
{
	struct timeval tv;
	int ret = gettimeofday(&tv, NULL);
	if (ret) {
		perror("gettimeofday");
		exit(1);
	}
	return tv.tv_sec + tv.tv_usec/1000000.0;
}

/* Handler for SIGUSR1, sleeps for a few seconds. Useful when suspending laptop. */
void SIGUSR1_handler(int sig)
{
	signal(SIGUSR1, SIGUSR1_handler);
	pause_now=1;
}

/*
 * usage() - display usage instructions and exit 
 */
void usage()
{
	printf("usage: hdapsd -d <device> -s <sensitivity> [-b] [-v]\n");
	printf("       <device> is likely to be hda or sda.\n");
	printf("       A suggested starting <sensitivity> is 15.\n");
	printf("       Use -b will run the process in the background.\n");
	printf("       Use -v to get verbose statistics.\n");
	printf("       Send SIGUSR1 to deactivate for %d seconds.\n",
	       SIGUSR1_SLEEP_SEC);
	exit(1);
}

/*
 * analyze() - make a decision on whether to park given present readouts
 *             (remembers some past data in local static variables).
 * Computes and checks 3 values:
 *   velocity:     current position - prev position / time delta
 *   acceleration: current velocity - prev velocity / time delta
 *   average velocity: exponentially decaying average of velocity,
 *                     weighed by time delta.
 * The velocity and acceleration tests respond quickly to short sharp shocks,
 * while the average velocity test catches long, smooth movements (and
 * averages out measurement noise).
 */
int analyze(int x, int y, double unow, double threshold) {
	static int x_last = 0, y_last = 0;
	static double unow_last = 0, x_veloc_last = 0, y_veloc_last = 0;
	static double x_avg_veloc = 0, y_avg_veloc = 0;
	static int history = 0; /* how many recent valid samples? */

	double udelta, x_delta, y_delta, x_veloc, y_veloc, x_accel, y_accel;
	double veloc_sqr, accel_sqr, avg_veloc_sqr;
	double exp_weight;
	char reason[3];
	int ret = 0;
	
	/* compute deltas */
	udelta = unow - unow_last;
	x_delta = x - x_last;
	y_delta = y - y_last;

	/* compute velocity */
	x_veloc = x_delta/udelta;
	y_veloc = y_delta/udelta;
	veloc_sqr = x_veloc*x_veloc + y_veloc*y_veloc;

	/* compute acceleration */
	x_accel = (x_veloc - x_veloc_last)/udelta;
	y_accel = (y_veloc - y_veloc_last)/udelta;
	accel_sqr = x_accel*x_accel + y_accel*y_accel;

	/* compute exponentially-decaying velocity average */
	exp_weight = udelta/AVG_DEPTH_SEC; /* weight of this sample */
	exp_weight = exp_weight/(1+exp_weight); /* softly clamped to 1 */
	x_avg_veloc = exp_weight*x_veloc + (1-exp_weight)*x_avg_veloc;
	y_avg_veloc = exp_weight*y_veloc + (1-exp_weight)*y_avg_veloc;
	avg_veloc_sqr = x_avg_veloc*x_avg_veloc + y_avg_veloc*y_avg_veloc;

	/* Threshold test (uses Pythagoras's theorem) */
	strcpy(reason, "   ");
	if (veloc_sqr > threshold*threshold * VELOC_ADJUST*VELOC_ADJUST ) {
		ret = 1;
		reason[0]='V';
	}
	if (accel_sqr > threshold*threshold * ACCEL_ADJUST*ACCEL_ADJUST ) {
		ret = 1;
		reason[1]='A';
	}
	if (avg_veloc_sqr > 
	    threshold*threshold * AVG_VELOC_ADJUST*AVG_VELOC_ADJUST ) {
		ret = 1;
		reason[2]='X';
	}

	if (verbose) {
		printf("dt=%5.3f  "
		       "delta=(%3g,%3g)  "
		       "veloc=(%6.1f,%6.1f)*%g  " /* For easy comparison to threshold. */
		       "accel=(%6.1f,%6.1f)*%g  " 
		       "avg_veloc=(%6.1f,%6.1f)*%g  "
		       "%s\n",
		       udelta,
		       x_delta, y_delta,
		       x_veloc/VELOC_ADJUST,
		       y_veloc/VELOC_ADJUST,
		       VELOC_ADJUST*1.0,
		       x_accel/ACCEL_ADJUST,
		       y_accel/ACCEL_ADJUST,
		       ACCEL_ADJUST*1.0,
		       x_avg_veloc/AVG_VELOC_ADJUST,
		       y_avg_veloc/AVG_VELOC_ADJUST,
		       AVG_VELOC_ADJUST*1.0,
		       reason);
	}

	if (udelta>1.0) { /* Too much time since last (resume from suspend?) */
		history = 0;
		x_avg_veloc = y_avg_veloc = 0;
	}

	if (history<2) { /* Not enough data for meaningful result */
		ret = 0;
		++history;
	}

	x_last = x;
	y_last = y;
	x_veloc_last = x_veloc;
	y_veloc_last = y_veloc;
	unow_last = unow;

	return ret;
}

/*
 * main() - loop forever, reading the hdaps values and 
 *          parking/unparking as necessary
 */
int main (int argc, char** argv)
{
	int c, park_now;
	int x, y;
	int fd, i, ret, threshold = 0, background = 0, parked = 0;
	char protect_file[32] = "";
	double unow, parked_utime;
	time_t now;

	for (;;) {
		c = getopt(argc, argv, "d:s:bv");
		if (c < 0)
			break;
		switch (c) {
			case 'd':
				sprintf(protect_file, "/sys/block/%s/queue/protect", optarg);
				break;
			case 's':
				threshold = atoi(optarg);
				break;
			case 'b':
				background = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			default:
				usage();
				break;
		}
	}

	if (!threshold || !strlen(protect_file))
		usage(argv);

	if (background) {
		verbose = 0;
		daemon(0,0);
	}

	mlockall(MCL_FUTURE);

	if (verbose) {
		printf("protect_file: %s\n", protect_file);
		printf("threshold: %i\n", threshold);
	}

	/* check the protect attribute exists */
	/* wait for it if it's not there (in case the attribute hasn't been created yet) */
	fd = open (protect_file, O_RDWR);
	if (background)
		for (i=0; fd < 0 && i < 100; ++i) {
			usleep (100000);	/* 10 Hz */
			fd = open (protect_file, O_RDWR);
		}
	if (fd < 0) {
		perror ("open(protect_file)");
		return 1;
	}
	close (fd);

	/* see if we can read the sensor */
	/* wait for it if it's not there (in case the attribute hasn't been created yet) */
	ret = read_position (&x, &y);
	if (background)
		for (i=0; ret && i < 100; ++i) {
			usleep (100000);	/* 10 Hz */
			ret = read_position (&x, &y);
		}
	if (ret)
		return 1;

	signal(SIGUSR1, SIGUSR1_handler);

	while (1) {
		usleep (1000000/FREQ_HZ);

		ret = read_position (&x, &y);
		if (ret)
			continue;

		now = time((time_t *)NULL); /* sec */
		unow = get_utime(); /* microsec */

		park_now = analyze(x, y, unow, threshold);

		if (park_now && !pause_now) {
			if (!parked || unow>parked_utime+REFREEZE_SECONDS) {
				/* Not frozen or freeze about to expire */
				write_protect(protect_file,
				              FREEZE_SECONDS+FREEZE_EXTRA_SECONDS);
				/* Write protect before any output (xterm, or 
				 * whatever else is handling our stdout, may be 
				 * swapped out).
				*/
				if (!parked)
					printf("%.24s: parking\n", ctime(&now));
				parked = 1;
				parked_utime = unow;
			} 
		} else {
			if (parked &&
			    (pause_now || unow>parked_utime+FREEZE_SECONDS)) {
				/* Sanity check */
				if (!read_protect(protect_file))
					printf("\nError! Not parked when we "
					       "thought we were... (paged out "
				               "and timer expired?)\n");
				/* Freeze has expired */
				write_protect(protect_file, 0); /* unprotect */
				parked = 0;
				printf("%.24s: un-parking\n", ctime(&now));
			}
			while (pause_now) {
				pause_now=0;
				printf("%.24s: pausing for %d seconds\n", 
				       ctime(&now), SIGUSR1_SLEEP_SEC);
				sleep(SIGUSR1_SLEEP_SEC);
			}
		}

	}

	munlockall();
	return ret;
}

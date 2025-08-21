/*	$OpenBSD: bell.c,v 1.7 2023/03/08 04:43:11 guenther Exp $	*/

/*
 * This file is in the public domain.
 *
 * Author: Mark Lumsden <mark@showcomplex.com>
 *
 */

/*
 * Control how mg communicates with the user.
 */

#include <sys/queue.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "def.h"
#include "macro.h"

void
bellinit(void)
{
	doaudiblebell = TRUE;
	dovisiblebell = FALSE;
}

int
dobeep_msg(const char *msg)
{
	ewprintf("%s", msg);
	dobeep();
	return (FALSE);
}

void
dobeep(void)
{
	if (doaudiblebell) {
		ttbeep();
	}
	if (dovisiblebell) {
		sgarbf = TRUE;
		update(CNONE);
		if (inmacro)	/* avoid delaying macro execution. */
			return;
		usleep(50000);
	}
}

int
toggleaudiblebell(int f, int n)
{
	if (f & FFANYARG)
		doaudiblebell = n > 0;
	else
		doaudiblebell = !doaudiblebell;

	ewprintf("Audible bell is now %s",
		 doaudiblebell ? "on" : "off");
	return (TRUE);
}

int
togglevisiblebell(int f, int n)
{
	if (f & FFANYARG)
		dovisiblebell = n > 0;
	else
		dovisiblebell = !dovisiblebell;

	ewprintf("Visible bell is now %s",
		 dovisiblebell ? "on" : "off");
	return (TRUE);
}

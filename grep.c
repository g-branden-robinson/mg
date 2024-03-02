/*	$OpenBSD: grep.c,v 1.50 2023/03/08 04:43:11 guenther Exp $	*/

/* This file is in the public domain */

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h> /* errno */
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

int	 globalwd = FALSE;
static int	 compile_goto_error(int, int);
int		 next_error(int, int);
static int	 grep(int, int);
static struct buffer	*compile_mode(const char *, const char *);
void grep_init(void);

static char compile_last_command[NFILEN] = "make ";

/*
 * Hints for next-error
 *
 * XXX - need some kind of callback to find out when those get killed.
 */
struct mgwin	*compile_win;
struct buffer	*compile_buffer;

static PF compile_pf[] = {
	compile_goto_error
};

static struct KEYMAPE (1) compilemap = {
	1,
	1,
	rescan,
	{
		{ CCHR('M'), CCHR('M'), compile_pf, NULL }
	}
};

void
grep_init(void)
{
	funmap_add(compile_goto_error, "compile-goto-error", 0);
	funmap_add(next_error, "next-error", 0);
	funmap_add(grep, "grep", 1);
	funmap_add(compile, "compile", 0);
	maps_add((KEYMAP *)&compilemap, "compile");
}

static int
grep(int f, int n)
{
	char	 cprompt[NFILEN], *bufp;
	struct buffer	*bp;
	struct mgwin	*wp;

	(void) strlcpy(cprompt, "grep -n ", sizeof cprompt);
	if ((bufp = eread("Run grep: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	/*
	 * This trick forces grep to report the file name; POSIX does
	 * not support GNU grep's `-H` option.
	 */
	if (strlcat(cprompt, " /dev/null", sizeof cprompt)
	    >= sizeof cprompt)
		return (FALSE);

	if ((bp = compile_mode("*grep*", cprompt)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	compile_win = curwp = wp;
	return (TRUE);
}

int
compile(int f, int n)
{
	char	 cprompt[NFILEN], *bufp;
	struct buffer	*bp;
	struct mgwin	*wp;

	(void) strlcpy(cprompt, compile_last_command, sizeof cprompt);
	if ((bufp = eread("Compile command: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if (savebuffers(f, n) == ABORT)
		return (ABORT);
	(void) strlcpy(compile_last_command, bufp,
		       sizeof compile_last_command);

	if ((bp = compile_mode("*compile*", cprompt)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	compile_win = curwp = wp;
	gotoline(FFANYARG, 0);
	return (TRUE);
}

struct buffer *
compile_mode(const char *name, const char *command)
{
	struct buffer	*bp;
	FILE	*fpipe;
	char	*buf;
	size_t	 sz;
	ssize_t	 len;
	int	 ret, n, status;
	char	 cwd[NFILEN], qcmd[NFILEN];
	char	 timestr[NTIME];
	time_t	 t;

	buf = NULL;
	sz = 0;

	n = snprintf(qcmd, sizeof qcmd, "%s 2>&1", command);
	if (n < 0 || n >= sizeof qcmd)
		return (NULL);

	bp = bfind(name, TRUE);
	if (bclear(bp) != TRUE)
		return (NULL);

	if (getbufcwd(bp->b_cwd, sizeof bp->b_cwd) != TRUE)
		return (NULL);
	addlinef(bp, "cd %s", bp->b_cwd);
	addline(bp, qcmd);
	addline(bp, "");

	if (getcwd(cwd, sizeof cwd) == NULL)
		panic("unable to get current directory in"
		      " compile_mode");
	if (chdir(bp->b_cwd) == -1) {
		dobeep();
		ewprintf("Cannot change directory to \"%s\": %s",
			 bp->b_cwd, strerror(errno));
		return (NULL);
	}
	if ((fpipe = popen(qcmd, "r")) == NULL) {
		dobeep();
		ewprintf("Cannot open pipe: %s", strerror(errno));
		return (NULL);
	}
	while ((len = getline(&buf, &sz, fpipe)) != -1) {
		if (buf[len - 1] == *bp->b_nlchr)
			buf[len - 1] = '\0';
		addline(bp, buf);
	}
	free(buf);
	if (ferror(fpipe))
		ewprintf("Problem reading pipe");
	ret = pclose(fpipe);
	t = time(NULL);
	strftime(timestr, sizeof timestr, "%a %b %e %T %Y",
		 localtime(&t));
	addline(bp, "");
	if (WIFEXITED(ret)) {
		status = WEXITSTATUS(ret);
		if (status == 0)
			addlinef(bp, "Command finished at %s", timestr);
		else
			addlinef(bp, "Command exited abnormally with code %d "
			    "at %s", status, timestr);
	} else
		addlinef(bp, "Subshell killed by signal %d at %s",
		    WTERMSIG(ret), timestr);

	bp->b_dotp = bfirstlp(bp);
	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("compile");
	bp->b_nmodes = 1;

	compile_buffer = bp;

	if (chdir(cwd) == -1) {
		dobeep();
		ewprintf("Cannot change directory back to"
			 " \"%s\": %s", cwd, strerror(errno));
		return (NULL);
	}
	return (bp);
}

static int
compile_goto_error(int f, int n)
{
	struct buffer	*bp;
	struct mgwin	*wp;
	char	*fname, *line, *lp, *ln;
	int	 lineno;
	char	*adjf, path[NFILEN];
	const char *errstr;
	struct line	*last;

	compile_win = curwp;
	compile_buffer = curbp;
	last = blastlp(compile_buffer);

 retry:
	/* last line is compilation result */
	if (curwp->w_dotp == last)
		return (FALSE);

	if ((line = linetostr(curwp->w_dotp)) == NULL)
		return (FALSE);
	lp = line;
	if ((fname = strsep(&lp, ":")) == NULL || *fname == '\0')
		goto fail;
	if ((ln = strsep(&lp, ":")) == NULL || *ln == '\0')
		goto fail;
	lineno = (int) strtonum(ln, INT_MIN, INT_MAX, &errstr);
	if (errstr != NULL)
		goto fail;

	if (fname && fname[0] != '/') {
		if (getbufcwd(path, sizeof path) == FALSE)
			goto fail;
		if (strlcat(path, fname, sizeof path) >= sizeof path)
			goto fail;
		adjf = path;
	} else {
		adjf = adjustname(fname, TRUE);
	}
	free(line);

	if (adjf == NULL)
		return (FALSE);

	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] == '\0')
		readin(adjf);
	gotoline(FFANYARG, lineno);
	return (TRUE);
fail:
	free(line);
	if (curwp->w_dotp != blastlp(curbp)) {
		curwp->w_dotp = lforw(curwp->w_dotp);
		curwp->w_rflag |= WFMOVE;
		goto retry;
	}
	dobeep();
	ewprintf("No more hits");
	return (FALSE);
}

int
next_error(int f, int n)
{
	if (compile_win == NULL || compile_buffer == NULL) {
		dobeep();
		ewprintf("No compilation active");
		return (FALSE);
	}
	curwp = compile_win;
	curbp = compile_buffer;
	if (curwp->w_dotp == blastlp(curbp)) {
		dobeep();
		ewprintf("No more hits");
		return (FALSE);
	}
	curwp->w_dotp = lforw(curwp->w_dotp);
	curwp->w_rflag |= WFMOVE;

	return (compile_goto_error(f, n));
}

/*
 * Since we don't have variables (we probably should) these are command
 * processors for changing the values of mode flags.
 */
int
globalwdtoggle(int f, int n)
{
	if (f & FFANYARG)
		globalwd = n > 0;
	else
		globalwd = !globalwd;

	sgarbf = TRUE;

	return (TRUE);
}

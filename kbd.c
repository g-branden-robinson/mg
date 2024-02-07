/*	$OpenBSD: kbd.c,v 1.37 2023/03/08 04:43:11 guenther Exp $	*/

/* This file is in the public domain. */

/*
 *	Terminal independent keyboard handling.
 */

#include <sys/queue.h>
#include <limits.h> // INT_MIN, INT_MAX
#include <signal.h>
#include <stdio.h>
#include <stdlib.h> // strtonum()

#include "def.h"
#include "kbd.h"
#include "key.h"
#include "macro.h"

#ifdef  MGLOG
#include "log.h"
#endif

#define METABIT 0x80

#define PROMPTL 80
char	 prompt[PROMPTL] = "", *promptp = prompt;

static int mgwrap(PF, int, int);

static int		 use_metakey = TRUE;
static int		 pushed = FALSE;
static int		 pushedc;
static int		 prompt_delay = 500;

struct map_element	*ele;
struct key		 key;
int			 rptcount;

/*
 * Toggle the value of use_metakey
 */
int
do_meta(int f, int n)
{
	if (f & FFANYARG)
		use_metakey = n > 0;
	else
		use_metakey = !use_metakey;

	ewprintf("Now assuming keyboard %s a meta key",
		 use_metakey ? "has" : "lacks");
	return (TRUE);
}

static int	 bs_map = 0;

/*
 * Toggle backspace mapping
 */
int
bsmap(int f, int n)
{
	if (f & FFANYARG)
		bs_map = n > 0;
	else
		bs_map = !bs_map;

	ewprintf("Now assuming backspace key produces %s",
		 bs_map ? "C-h" : "DEL");
	return (TRUE);
}

void
ungetkey(int c)
{
	if (use_metakey && pushed && c == CCHR('['))
		pushedc |= METABIT;
	else
		pushedc = c;
	pushed = TRUE;
}

int
getkey(int want_prompt)
{
	int	 c;

	if (want_prompt && !pushed) {
		if (prompt[0] != '\0' && ttwait(prompt_delay)) {
			/* avoid problems with % */
			ewprintf("%s", prompt);
			/* put the cursor back */
			update(CMODE);
			epresf = KCLEAR;
		}
		if (promptp > prompt)
			*(promptp - 1) = ' ';
	}
	if (pushed) {
		c = pushedc;
		pushed = FALSE;
	} else
		c = ttgetc();

	if (bs_map) {
		if (c == CCHR('H'))
			c = CCHR('?');
		else if (c == CCHR('?'))
			c = CCHR('H');
	}
	if (use_metakey && (c & METABIT)) {
		pushedc = c & ~METABIT;
		pushed = TRUE;
		c = CCHR('[');
	}
	if (want_prompt && promptp < &prompt[PROMPTL - 5]) {
		promptp = getkeyname(promptp,
		    sizeof(prompt) - (promptp - prompt) - 1, c);
		*promptp++ = '-';
		*promptp = '\0';
	}
	return (c);
}

/*
 * doscan scans a keymap for a keyboard character and returns a pointer
 * to the function associated with that character.  Sets ele to the
 * keymap element the keyboard was found in as a side effect.
 */
PF
doscan(KEYMAP *map, int c, KEYMAP **newmap)
{
	struct map_element	*elec = &map->map_element[0];
	struct map_element	*last = &map->map_element[map->map_num];
	PF		 ret;

	while (elec < last && c > elec->k_num)
		elec++;

	/* used by prefix and binding code */
	ele = elec;
	if (elec >= last || c < elec->k_base)
		ret = map->map_default;
	else
		ret = elec->k_funcp[c - elec->k_base];
	if (ret == NULL && newmap != NULL)
		*newmap = elec->k_prefmap;

	return (ret);
}

int
doin(void)
{
	KEYMAP	*curmap;
	PF	 funct;

	*(promptp = prompt) = '\0';
	curmap = curbp->b_modes[curbp->b_nmodes]->p_map;
	key.k_count = 0;
	while ((funct = doscan(curmap, (key.k_chars[key.k_count++] =
	    getkey(TRUE)), &curmap)) == NULL)
		/* nothing */;

#ifdef  MGLOG
	if (!mglog(funct, curmap))
		ewprintf("Problem with logging");
#endif

	if (macrodef && macrocount < MAXMACRO)
		macro[macrocount++].m_funct = funct;

	return (mgwrap(funct, 0, 1));
}

int
rescan(int f, int n)
{
	int	 c;
	KEYMAP	*curmap;
	int	 i;
	PF	 fp = NULL;
	int	 md = curbp->b_nmodes;

	for (;;) {
		if (ISUPPER(key.k_chars[key.k_count - 1])) {
			c = TOLOWER(key.k_chars[key.k_count - 1]);
			curmap = curbp->b_modes[md]->p_map;
			for (i = 0; i < key.k_count - 1; i++) {
				if ((fp = doscan(curmap, (key.k_chars[i]),
				    &curmap)) != NULL)
					break;
			}
			if (fp == NULL) {
				if ((fp = doscan(curmap, c, NULL)) == NULL)
					while ((fp = doscan(curmap,
					    key.k_chars[key.k_count++] =
					    getkey(TRUE), &curmap)) == NULL)
						/* nothing */;
				if (fp != rescan) {
					if (macrodef && macrocount <= MAXMACRO)
						macro[macrocount - 1].m_funct
						    = fp;
					return (mgwrap(fp, f, n));
				}
			}
		}
		/* try previous mode */
		if (--md < 0)
			return (ABORT);
		curmap = curbp->b_modes[md]->p_map;
		for (i = 0; i < key.k_count; i++) {
			if ((fp = doscan(curmap, (key.k_chars[i]), &curmap)) != NULL)
				break;
		}
		if (fp == NULL) {
			while ((fp = doscan(curmap, key.k_chars[i++] =
			    getkey(TRUE), &curmap)) == NULL)
				/* nothing */;
			key.k_count = i;
		}
		if (fp != rescan && i >= key.k_count - 1) {
			if (macrodef && macrocount <= MAXMACRO)
				macro[macrocount - 1].m_funct = fp;
			return (mgwrap(fp, f, n));
		}
	}
}

int
universal_argument(int f, int n)
{
	KEYMAP	*curmap;
	PF	 funct;
	int	 c, nn = 4;

	if (f & FFUNIV)
		nn *= n;
	for (;;) {
		key.k_chars[0] = c = getkey(TRUE);
		key.k_count = 1;
		if (c == '-')
			return (negative_argument(f, nn));
		if (c >= '0' && c <= '9')
			return (digit_argument(f, nn));
		curmap = curbp->b_modes[curbp->b_nmodes]->p_map;
		while ((funct = doscan(curmap, c, &curmap)) == NULL) {
			key.k_chars[key.k_count++] = c = getkey(TRUE);
		}
		if (funct != universal_argument) {
			if (macrodef && macrocount < MAXMACRO - 1) {
				if (f & FFANYARG)
					macrocount--;
				macro[macrocount++].m_count = nn;
				macro[macrocount++].m_funct = funct;
			}
			return (mgwrap(funct, FFUNIV, nn));
		}
		nn <<= 2;
	}
}

/*
 * Return signed integer representing command argument.  Store the
 * character that terminates the numeric sequence in argument `c`.
 */
static int
collect_integer(int n, int *c, int want_negation)
{
	static const int limit = INT_MAX / 10;

	int	 c1, c2;
	int	 got_digit;

	for (;;) {
		got_digit = FALSE;
		c1 = getkey(TRUE);
		if (c1 == CCHR('[')) {
			c2 = getkey(TRUE);
			if (c2 == '-')
				want_negation = !want_negation;
			else if (c2 < '0' || c2 > '9') {
				ungetkey(c2);
				break;
			}
			else {
				got_digit = TRUE;
				c1 = c2;
			}
		} else if (c1 == '-')
			want_negation = !want_negation;
		else if (c1 < '0' || c1 > '9')
			break;
		else
			got_digit = TRUE;
		if (got_digit) {
			if (abs(n) > limit) {
				dobeep();
				break;
			}
			else {
				n *= 10;
				n += c1 - '0';
			}
		}
	}

	if (want_negation)
		n = -n;

	*c = c1;
	return n;
}

int
digit_argument(int f, int n)
{
	KEYMAP	*curmap;
	PF	 funct;
	int	 nn, c;

	nn = collect_integer(key.k_chars[key.k_count - 1] - '0', &c,
			     FALSE);
	key.k_chars[0] = c;
	key.k_count = 1;
	curmap = curbp->b_modes[curbp->b_nmodes]->p_map;
	while ((funct = doscan(curmap, c, &curmap)) == NULL) {
		key.k_chars[key.k_count++] = c = getkey(TRUE);
	}
	if (macrodef && macrocount < MAXMACRO - 1) {
		if (f & FFANYARG)
			macrocount--;
		else
			macro[macrocount - 1].m_funct = universal_argument;
		macro[macrocount++].m_count = nn;
		macro[macrocount++].m_funct = funct;
	}
	return (mgwrap(funct, FFOTHARG, nn));
}

int
negative_argument(int f, int n)
{
	KEYMAP	*curmap;
	PF	 funct;
	int	 nn, c;

	nn = collect_integer(0, &c, TRUE);
	key.k_chars[0] = c;
	key.k_count = 1;
	curmap = curbp->b_modes[curbp->b_nmodes]->p_map;
	while ((funct = doscan(curmap, c, &curmap)) == NULL) {
		key.k_chars[key.k_count++] = c = getkey(TRUE);
	}
	if (macrodef && macrocount < MAXMACRO - 1) {
		if (f & FFANYARG)
			macrocount--;
		else
			macro[macrocount - 1].m_funct = universal_argument;
		macro[macrocount++].m_count = nn;
		macro[macrocount++].m_funct = funct;
	}
	return (mgwrap(funct, FFNEGARG, nn));
}

/*
 * Insert a character.	While defining a macro, create a "LINE" containing
 * all inserted characters.
 */
int
selfinsert(int f, int n)
{
	struct line	*lp;
	int	 c;
	int	 count;

	if (n < 1 && !(f & FFRAND)) {
		dobeep_msg("Cannot self-insert with a nonpositive"
			   " count");
		return (FALSE);
	}

	if (n < 0)
		return (FALSE);
	if (n == 0)
		return (TRUE);

	c = key.k_chars[key.k_count - 1];

	if (macrodef && macrocount < MAXMACRO) {
		if (f & FFANYARG)
			macrocount -= 2;

		/* last command was insert -- tack on the end */
		if (lastflag & CFINS) {
			macrocount--;
			/* Ensure the line can handle the new characters */
			if (maclcur->l_size < maclcur->l_used + n) {
				if (lrealloc(maclcur, maclcur->l_used + n) ==
				    FALSE)
					return (FALSE);
			}
			maclcur->l_used += n;
			/* Copy in the new data */
			for (count = maclcur->l_used - n;
			    count < maclcur->l_used; count++)
				maclcur->l_text[count] = c;
		} else {
			macro[macrocount - 1].m_funct = insert;
			if ((lp = lalloc(n)) == NULL)
				return (FALSE);
			lp->l_bp = maclcur;
			lp->l_fp = maclcur->l_fp;
			maclcur->l_fp = lp;
			maclcur = lp;
			for (count = 0; count < n; count++)
				lp->l_text[count] = c;
		}
		thisflag |= CFINS;
	}
	if (c == *curbp->b_nlchr) {
		do {
			count = lnewline();
		} while (--n && count == TRUE);
		return (count);
	}

	/* overwrite mode */
	if (curbp->b_flag & BFOVERWRITE) {
		lchange(WFEDIT);
		while (curwp->w_doto < llength(curwp->w_dotp) && n--)
			lputc(curwp->w_dotp, curwp->w_doto++, c);
		if (n <= 0)
			return (TRUE);
	}
	return (linsert(n, c));
}

/*
 * selfinsert() can't be called directly from a startup file or by
 * 'eval-current-buffer' since it is by design, meant to be called interactively
 * as characters are typed in a buffer. ask_selfinsert() allows selfinsert() to
 * be used by excline(). Having ask_selfinsert() helps with regression testing.
 * No manual page entry since use case is a bit obscure. See 'insert' command.
 */
int
ask_selfinsert(int f, int n)
{
	char	*c, cbuf[2];

	if ((c = eread("Insert a character: ", cbuf, sizeof(cbuf),
	    EFNEW)) == NULL || (c[0] == '\0'))
		return (ABORT);

	key.k_chars[0] = *c;
	key.k_chars[1] = '\0';
	key.k_count = 1;

	return (selfinsert(FFRAND, 1));
}

/*
 * This could be implemented as a keymap with everything defined as self-insert.
 */
int
quote(int f, int n)
{
	int	 c;

	key.k_count = 1;
	if ((key.k_chars[0] = getkey(TRUE)) >= '0' && key.k_chars[0] <= '7') {
		key.k_chars[0] -= '0';
		if ((c = getkey(TRUE)) >= '0' && c <= '7') {
			key.k_chars[0] <<= 3;
			key.k_chars[0] += c - '0';
			if ((c = getkey(TRUE)) >= '0' && c <= '7') {
				key.k_chars[0] <<= 3;
				key.k_chars[0] += c - '0';
			} else
				ungetkey(c);
		} else
			ungetkey(c);
	}
	return (selfinsert(f, n));
}

/*
 * Wrapper function to count invocation repeats.
 * We ignore any function whose sole purpose is to get us
 * to the intended function.
 */
static int
mgwrap(PF funct, int f, int n)
{
	static	 PF ofp;

	if (funct != rescan &&
	    funct != negative_argument &&
	    funct != digit_argument &&
	    funct != universal_argument) {
		if (funct == ofp)
			rptcount++;
		else
			rptcount = 0;
		ofp = funct;
	}

	return ((*funct)(f, n));
}

/*
 * Set delay amount (in in milliseconds) before presenting a
 * disambiguation prompt in the minibuffer.
 */
int
setpromptdelay(int f, int n)
{
	char buf[32], *rep;
	const char *es;
	int delay;

	if ((f & FFANYARG) != 0) {
		delay = n;
	} else {
		if ((rep = eread("Set prompt delay in ms (currently"
				 " %d ms): ", buf, sizeof buf,
				 EFNEW | EFCR, prompt_delay)) == NULL)
			return (ABORT);
		else if (rep[0] == '\0')
			return (FALSE);
		delay = strtonum(rep, INT_MIN, INT_MAX, &es);
		if (es != NULL) {
			dobeep();
			ewprintf("Invalid prompt delay: %s", rep);
			return (FALSE);
		}
	}

	if (delay < 0) {
		dobeep_msg("Prompt delay cannot be negative");
		return (FALSE);
	}

	prompt_delay = delay;
	ewprintf("Prompt delay is now %d ms", prompt_delay);
	return (TRUE);
}

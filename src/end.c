/* NetHack 3.6	end.c	$NHDT-Date: 1545646111 2018/12/24 10:08:31 $  $NHDT-Branch: NetHack-3.6.2-beta01 $:$NHDT-Revision: 1.160 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2012. */
/* NetHack may be freely redistributed.  See license for details. */

#define NEED_VARARGS /* comment line for pre-compiled headers */

#include "hack.h"
#include "lev.h"
#ifndef NO_SIGNAL
#include <signal.h>
#endif
#include <ctype.h>
#ifndef LONG_MAX
#include <limits.h>
#endif
#include "dlb.h"

/* add b to long a, convert wraparound to max value */
#define nowrap_add(a, b) (a = ((a + b) < 0 ? LONG_MAX : (a + b)))

#ifndef NO_SIGNAL
STATIC_PTR void FDECL(done_intr, (int));
#if defined(UNIX) || defined(VMS) || defined(__EMX__)
static void FDECL(done_hangup, (int));
#endif
#endif
STATIC_DCL void FDECL(disclose, (int, BOOLEAN_P));
STATIC_DCL void FDECL(get_valuables, (struct obj *));
STATIC_DCL void FDECL(sort_valuables, (struct valuable_data *, int));
STATIC_DCL void FDECL(artifact_score, (struct obj *, BOOLEAN_P, winid));
STATIC_DCL void FDECL(really_done, (int)) NORETURN;
STATIC_DCL void FDECL(savelife, (int));
STATIC_PTR int FDECL(CFDECLSPEC vanqsort_cmp, (const genericptr,
                                               const genericptr));
STATIC_DCL int NDECL(set_vanq_order);
STATIC_DCL void FDECL(list_vanquished, (CHAR_P, BOOLEAN_P));
STATIC_DCL void FDECL(list_genocided, (CHAR_P, BOOLEAN_P));
STATIC_DCL boolean FDECL(should_query_disclose_option, (int, char *));
#ifdef DUMPLOG
STATIC_DCL void NDECL(dump_plines);
#endif
STATIC_DCL void FDECL(dump_everything, (int, time_t));
STATIC_DCL int NDECL(num_extinct);

#if defined(__BEOS__) || defined(MICRO) || defined(WIN32) || defined(OS2)
extern void FDECL(nethack_exit, (int));
#else
#define nethack_exit exit
#endif

#define done_stopprint program_state.stopprint

#ifndef PANICTRACE
#define NH_abort NH_abort_
#endif

#ifdef AMIGA
#define NH_abort_() Abort(0)
#else
#ifdef SYSV
#define NH_abort_() (void) abort()
#else
#ifdef WIN32
#define NH_abort_() win32_abort()
#else
#define NH_abort_() abort()
#endif
#endif /* !SYSV */
#endif /* !AMIGA */

#ifdef PANICTRACE
#include <errno.h>
#ifdef PANICTRACE_LIBC
#include <execinfo.h>
#endif

/* What do we try and in what order?  Tradeoffs:
 * libc: +no external programs required
 *        -requires newish libc/glibc
 *        -requires -rdynamic
 * gdb:   +gives more detailed information
 *        +works on more OS versions
 *        -requires -g, which may preclude -O on some compilers
 */
#ifdef SYSCF
#define SYSOPT_PANICTRACE_GDB sysopt.panictrace_gdb
#ifdef PANICTRACE_LIBC
#define SYSOPT_PANICTRACE_LIBC sysopt.panictrace_libc
#else
#define SYSOPT_PANICTRACE_LIBC 0
#endif
#else
#define SYSOPT_PANICTRACE_GDB (nh_getenv("NETHACK_USE_GDB") == 0 ? 0 : 2)
#ifdef PANICTRACE_LIBC
#define SYSOPT_PANICTRACE_LIBC 1
#else
#define SYSOPT_PANICTRACE_LIBC 0
#endif
#endif

static void NDECL(NH_abort);
#ifndef NO_SIGNAL
static void FDECL(panictrace_handler, (int));
#endif
static boolean NDECL(NH_panictrace_libc);
static boolean NDECL(NH_panictrace_gdb);

#ifndef NO_SIGNAL
/* called as signal() handler, so sent at least one arg */
/*ARGUSED*/
void
panictrace_handler(sig_unused)
int sig_unused UNUSED;
{
#define SIG_MSG "\nSignal received.\n"
    int f2 = (int) write(2, SIG_MSG, sizeof SIG_MSG - 1);

    nhUse(f2);  /* what could we do if write to fd#2 (stderr) fails  */
    NH_abort(); /* ... and we're already in the process of quitting? */
}

void
panictrace_setsignals(set)
boolean set;
{
#define SETSIGNAL(sig) \
    (void) signal(sig, set ? (SIG_RET_TYPE) panictrace_handler : SIG_DFL);
#ifdef SIGILL
    SETSIGNAL(SIGILL);
#endif
#ifdef SIGTRAP
    SETSIGNAL(SIGTRAP);
#endif
#ifdef SIGIOT
    SETSIGNAL(SIGIOT);
#endif
#ifdef SIGBUS
    SETSIGNAL(SIGBUS);
#endif
#ifdef SIGFPE
    SETSIGNAL(SIGFPE);
#endif
#ifdef SIGSEGV
    SETSIGNAL(SIGSEGV);
#endif
#ifdef SIGSTKFLT
    SETSIGNAL(SIGSTKFLT);
#endif
#ifdef SIGSYS
    SETSIGNAL(SIGSYS);
#endif
#ifdef SIGEMT
    SETSIGNAL(SIGEMT);
#endif
#undef SETSIGNAL
}
#endif /* NO_SIGNAL */

static void
NH_abort()
{
    int gdb_prio = SYSOPT_PANICTRACE_GDB;
    int libc_prio = SYSOPT_PANICTRACE_LIBC;
    static boolean aborting = FALSE;

    if (aborting)
        return;
    aborting = TRUE;

#ifndef VMS
    if (gdb_prio == libc_prio && gdb_prio > 0)
        gdb_prio++;

    if (gdb_prio > libc_prio) {
        (void) (NH_panictrace_gdb() || (libc_prio && NH_panictrace_libc()));
    } else {
        (void) (NH_panictrace_libc() || (gdb_prio && NH_panictrace_gdb()));
    }

#else /* VMS */
    /* overload otherwise unused priority for debug mode: 1 = show
       traceback and exit; 2 = show traceback and stay in debugger */
    /* if (wizard && gdb_prio == 1) gdb_prio = 2; */
    vms_traceback(gdb_prio);
    nhUse(libc_prio);

#endif /* ?VMS */

#ifndef NO_SIGNAL
    panictrace_setsignals(FALSE);
#endif
    NH_abort_();
}

static boolean
NH_panictrace_libc()
{
#ifdef PANICTRACE_LIBC
    void *bt[20];
    size_t count, x;
    char **info;

    raw_print("Generating more information you may report:\n");
    count = backtrace(bt, SIZE(bt));
    info = backtrace_symbols(bt, count);
    for (x = 0; x < count; x++) {
        raw_printf("[%lu] %s", (unsigned long) x, info[x]);
    }
    /* free(info);   -- Don't risk it. */
    return TRUE;
#else
    return FALSE;
#endif /* !PANICTRACE_LIBC */
}

/*
 *   fooPATH  file system path for foo
 *   fooVAR   (possibly const) variable containing fooPATH
 */
#ifdef PANICTRACE_GDB
#ifdef SYSCF
#define GDBVAR sysopt.gdbpath
#define GREPVAR sysopt.greppath
#else /* SYSCF */
#define GDBVAR GDBPATH
#define GREPVAR GREPPATH
#endif /* SYSCF */
#endif /* PANICTRACE_GDB */

static boolean
NH_panictrace_gdb()
{
#ifdef PANICTRACE_GDB
    /* A (more) generic method to get a stack trace - invoke
     * gdb on ourself. */
    char *gdbpath = GDBVAR;
    char *greppath = GREPVAR;
    char buf[BUFSZ];
    FILE *gdb;

    if (gdbpath == NULL || gdbpath[0] == 0)
        return FALSE;
    if (greppath == NULL || greppath[0] == 0)
        return FALSE;

    sprintf(buf, "%s -n -q %s %d 2>&1 | %s '^#'", gdbpath, ARGV0, getpid(),
            greppath);
    gdb = popen(buf, "w");
    if (gdb) {
        raw_print("Generating more information you may report:\n");
        fprintf(gdb, "bt\nquit\ny");
        fflush(gdb);
        sleep(4); /* ugly */
        pclose(gdb);
        return TRUE;
    } else {
        return FALSE;
    }
#else
    return FALSE;
#endif /* !PANICTRACE_GDB */
}
#endif /* PANICTRACE */

/*
 * The order of these needs to match the macros in hack.h.
 */
static NEARDATA const char *deaths[] = {
    /* the array of death */
    "died", "choked", "poisoned", "starvation", "drowning", "burning",
    "dissolving under the heat and pressure", "crushed", "turned to stone",
    "turned into slime", "genocided", "panic", "trickery", "quit",
    "escaped", "ascended"
};

static NEARDATA const char *ends[] = {
    /* "when you %s" */
    "died", "choked", "were poisoned",
    "starved", "drowned", "burned",
    "dissolved in the lava",
    "were crushed", "turned to stone",
    "turned into slime", "were genocided",
    "panicked", "were tricked", "quit",
    "escaped", "ascended"
};

static boolean Schroedingers_cat = FALSE;

/*ARGSUSED*/
void
done1(sig_unused) /* called as signal() handler, so sent at least one arg */
int sig_unused UNUSED;
{
#ifndef NO_SIGNAL
    (void) signal(SIGINT, SIG_IGN);
#endif
    if (flags.ignintr) {
#ifndef NO_SIGNAL
        (void) signal(SIGINT, (SIG_RET_TYPE) done1);
#endif
        clear_nhwindow(WIN_MESSAGE);
        curs_on_u();
        wait_synch();
        if (g.multi > 0)
            nomul(0);
    } else {
        (void) done2();
    }
}

/* "#quit" command or keyboard interrupt */
int
done2()
{
    if (iflags.debug_fuzzer)
        return 0;
    if (!paranoid_query(ParanoidQuit, "Really quit?")) {
#ifndef NO_SIGNAL
        (void) signal(SIGINT, (SIG_RET_TYPE) done1);
#endif
        clear_nhwindow(WIN_MESSAGE);
        curs_on_u();
        wait_synch();
        if (g.multi > 0)
            nomul(0);
        if (g.multi == 0) {
            u.uinvulnerable = FALSE; /* avoid ctrl-C bug -dlc */
            u.usleep = 0;
        }
        return 0;
    }
#if (defined(UNIX) || defined(VMS) || defined(LATTICE))
    if (wizard) {
        int c;
#ifdef VMS
        extern int debuggable; /* sys/vms/vmsmisc.c, vmsunix.c */

        c = !debuggable ? 'n' : ynq("Enter debugger?");
#else
#ifdef LATTICE
        c = ynq("Create SnapShot?");
#else
        c = ynq("Dump core?");
#endif
#endif
        if (c == 'y') {
#ifndef NO_SIGNAL
            (void) signal(SIGINT, (SIG_RET_TYPE) done1);
#endif
            exit_nhwindows((char *) 0);
            NH_abort();
        } else if (c == 'q')
            done_stopprint++;
    }
#endif
#ifndef LINT
    done(QUIT);
#endif
    return 0;
}

#ifndef NO_SIGNAL
/*ARGSUSED*/
STATIC_PTR void
done_intr(sig_unused) /* called as signal() handler, so sent at least 1 arg */
int sig_unused UNUSED;
{
    done_stopprint++;
    (void) signal(SIGINT, SIG_IGN);
#if defined(UNIX) || defined(VMS)
    (void) signal(SIGQUIT, SIG_IGN);
#endif
    return;
}

#if defined(UNIX) || defined(VMS) || defined(__EMX__)
/* signal() handler */
static void
done_hangup(sig)
int sig;
{
    program_state.done_hup++;
    sethanguphandler((void FDECL((*), (int) )) SIG_IGN);
    done_intr(sig);
    return;
}
#endif
#endif /* NO_SIGNAL */

void
done_in_by(mtmp, how)
struct monst *mtmp;
int how;
{
    char buf[BUFSZ];
    struct permonst *mptr = mtmp->data,
                    *champtr = ((mtmp->cham >= LOW_PM)
                                   ? &mons[mtmp->cham]
                                   : mptr);
    boolean distorted = (boolean) (Hallucination && canspotmon(mtmp)),
            mimicker = (mtmp->m_ap_type == M_AP_MONSTER),
            imitator = (mptr != champtr || mimicker);

    You((how == STONING) ? "turn to stone..." : "die...");
    mark_synch(); /* flush buffered screen output */
    buf[0] = '\0';
    killer.format = KILLED_BY_AN;
    /* "killed by the high priest of Crom" is okay,
       "killed by the high priest" alone isn't */
    if ((mptr->geno & G_UNIQ) != 0 && !(imitator && !mimicker)
        && !(mptr == &mons[PM_HIGH_PRIEST] && !mtmp->ispriest)) {
        if (!type_is_pname(mptr))
            Strcat(buf, "the ");
        killer.format = KILLED_BY;
    }
    /* _the_ <invisible> <distorted> ghost of Dudley */
    if (mptr == &mons[PM_GHOST] && has_mname(mtmp)) {
        Strcat(buf, "the ");
        killer.format = KILLED_BY;
    }
    if (mtmp->minvis)
        Strcat(buf, "invisible ");
    if (distorted)
        Strcat(buf, "hallucinogen-distorted ");

    if (imitator) {
        char shape[BUFSZ];
        const char *realnm = champtr->mname, *fakenm = mptr->mname;
        boolean alt = is_vampshifter(mtmp);

        if (mimicker) {
            /* realnm is already correct because champtr==mptr;
               set up fake mptr for type_is_pname/the_unique_pm */
            mptr = &mons[mtmp->mappearance];
            fakenm = mptr->mname;
        } else if (alt && strstri(realnm, "vampire")
                   && !strcmp(fakenm, "vampire bat")) {
            /* special case: use "vampire in bat form" in preference
               to redundant looking "vampire in vampire bat form" */
            fakenm = "bat";
        }
        /* for the alternate format, always suppress any article;
           pname and the_unique should also have s_suffix() applied,
           but vampires don't take on any shapes which warrant that */
        if (alt || type_is_pname(mptr)) /* no article */
            Strcpy(shape, fakenm);
        else if (the_unique_pm(mptr)) /* "the"; don't use the() here */
            Sprintf(shape, "the %s", fakenm);
        else /* "a"/"an" */
            Strcpy(shape, an(fakenm));
        /* omit "called" to avoid excessive verbosity */
        Sprintf(eos(buf),
                alt ? "%s in %s form"
                    : mimicker ? "%s disguised as %s"
                               : "%s imitating %s",
                realnm, shape);
        mptr = mtmp->data; /* reset for mimicker case */
    } else if (mptr == &mons[PM_GHOST]) {
        Strcat(buf, "ghost");
        if (has_mname(mtmp))
            Sprintf(eos(buf), " of %s", MNAME(mtmp));
    } else if (mtmp->isshk) {
        const char *shknm = shkname(mtmp),
                   *honorific = shkname_is_pname(mtmp) ? ""
                                   : mtmp->female ? "Ms. " : "Mr. ";

        Sprintf(eos(buf), "%s%s, the shopkeeper", honorific, shknm);
        killer.format = KILLED_BY;
    } else if (mtmp->ispriest || mtmp->isminion) {
        /* m_monnam() suppresses "the" prefix plus "invisible", and
           it overrides the effect of Hallucination on priestname() */
        Strcat(buf, m_monnam(mtmp));
    } else {
        Strcat(buf, mptr->mname);
        if (has_mname(mtmp))
            Sprintf(eos(buf), " called %s", MNAME(mtmp));
    }

    Strcpy(killer.name, buf);
    if (mptr->mlet == S_WRAITH)
        u.ugrave_arise = PM_WRAITH;
    else if (mptr->mlet == S_MUMMY && urace.mummynum != NON_PM)
        u.ugrave_arise = urace.mummynum;
    else if (mptr->mlet == S_VAMPIRE && Race_if(PM_HUMAN))
        u.ugrave_arise = PM_VAMPIRE;
    else if (mptr == &mons[PM_GHOUL])
        u.ugrave_arise = PM_GHOUL;
    /* this could happen if a high-end vampire kills the hero
       when ordinary vampires are genocided; ditto for wraiths */
    if (u.ugrave_arise >= LOW_PM
        && (mvitals[u.ugrave_arise].mvflags & G_GENOD))
        u.ugrave_arise = NON_PM;

    done(how);
    return;
}

/* some special cases for overriding while-helpless reason */
static const struct {
    int why, unmulti;
    const char *exclude, *include;
} death_fixups[] = {
    /* "petrified by <foo>, while getting stoned" -- "while getting stoned"
       prevented any last-second recovery, but it was not the cause of
       "petrified by <foo>" */
    { STONING, 1, "getting stoned", (char *) 0 },
    /* "died of starvation, while fainted from lack of food" is accurate
       but sounds a fairly silly (and doesn't actually appear unless you
       splice together death and while-helpless from xlogfile) */
    { STARVING, 0, "fainted from lack of food", "fainted" },
};

/* clear away while-helpless when the cause of death caused that
   helplessness (ie, "petrified by <foo> while getting stoned") */
STATIC_DCL void
fixup_death(how)
int how;
{
    int i;

    if (g.multi_reason) {
        for (i = 0; i < SIZE(death_fixups); ++i)
            if (death_fixups[i].why == how
                && !strcmp(death_fixups[i].exclude, g.multi_reason)) {
                if (death_fixups[i].include) /* substitute alternate reason */
                    g.multi_reason = death_fixups[i].include;
                else /* remove the helplessness reason */
                    g.multi_reason = (char *) 0;
                if (death_fixups[i].unmulti) /* possibly hide helplessness */
                    g.multi = 0L;
                break;
            }
    }
}

#if defined(WIN32) && !defined(SYSCF)
#define NOTIFY_NETHACK_BUGS
#endif

/*VARARGS1*/
void panic
VA_DECL(const char *, str)
{
    VA_START(str);
    VA_INIT(str, char *);

    if (program_state.panicking++)
        NH_abort(); /* avoid loops - this should never happen*/

    if (iflags.window_inited) {
        raw_print("\r\nOops...");
        wait_synch(); /* make sure all pending output gets flushed */
        exit_nhwindows((char *) 0);
        iflags.window_inited = 0; /* they're gone; force raw_print()ing */
    }

    raw_print(program_state.gameover
                  ? "Postgame wrapup disrupted."
                  : !program_state.something_worth_saving
                        ? "Program initialization has failed."
                        : "Suddenly, the dungeon collapses.");
#ifndef MICRO
#ifdef NOTIFY_NETHACK_BUGS
    if (!wizard)
        raw_printf("Report the following error to \"%s\" or at \"%s\".",
                   DEVTEAM_EMAIL, DEVTEAM_URL);
    else if (program_state.something_worth_saving)
        raw_print("\nError save file being written.\n");
#else /* !NOTIFY_NETHACK_BUGS */
    if (!wizard) {
        const char *maybe_rebuild = !program_state.something_worth_saving
                                     ? "."
                                     : "\nand it may be possible to rebuild.";

        if (sysopt.support)
            raw_printf("To report this error, %s%s", sysopt.support,
                       maybe_rebuild);
        else if (sysopt.fmtd_wizard_list) /* formatted SYSCF WIZARDS */
            raw_printf("To report this error, contact %s%s",
                       sysopt.fmtd_wizard_list, maybe_rebuild);
        else
            raw_printf("Report error to \"%s\"%s", WIZARD_NAME,
                       maybe_rebuild);
    }
#endif /* ?NOTIFY_NETHACK_BUGS */
    /* XXX can we move this above the prints?  Then we'd be able to
     * suppress "it may be possible to rebuild" based on dosave0()
     * or say it's NOT possible to rebuild. */
    if (program_state.something_worth_saving && !iflags.debug_fuzzer) {
        set_error_savefile();
        if (dosave0()) {
            /* os/win port specific recover instructions */
            if (sysopt.recover)
                raw_printf("%s", sysopt.recover);
        }
    }
#endif /* !MICRO */
    {
        char buf[BUFSZ];

        Vsprintf(buf, str, VA_ARGS);
        raw_print(buf);
        paniclog("panic", buf);
    }
#ifdef WIN32
    interject(INTERJECT_PANIC);
#endif
#if defined(UNIX) || defined(VMS) || defined(LATTICE) || defined(WIN32)
    if (wizard)
        NH_abort(); /* generate core dump */
#endif
    VA_END();
    really_done(PANICKED);
}

STATIC_OVL boolean
should_query_disclose_option(category, defquery)
int category;
char *defquery;
{
    int idx;
    char disclose, *dop;

    *defquery = 'n';
    if ((dop = index(disclosure_options, category)) != 0) {
        idx = (int) (dop - disclosure_options);
        if (idx < 0 || idx >= NUM_DISCLOSURE_OPTIONS) {
            impossible(
                   "should_query_disclose_option: bad disclosure index %d %c",
                       idx, category);
            *defquery = DISCLOSE_PROMPT_DEFAULT_YES;
            return TRUE;
        }
        disclose = flags.end_disclose[idx];
        if (disclose == DISCLOSE_YES_WITHOUT_PROMPT) {
            *defquery = 'y';
            return FALSE;
        } else if (disclose == DISCLOSE_SPECIAL_WITHOUT_PROMPT) {
            *defquery = 'a';
            return FALSE;
        } else if (disclose == DISCLOSE_NO_WITHOUT_PROMPT) {
            *defquery = 'n';
            return FALSE;
        } else if (disclose == DISCLOSE_PROMPT_DEFAULT_YES) {
            *defquery = 'y';
            return TRUE;
        } else if (disclose == DISCLOSE_PROMPT_DEFAULT_SPECIAL) {
            *defquery = 'a';
            return TRUE;
        } else {
            *defquery = 'n';
            return TRUE;
        }
    }
    impossible("should_query_disclose_option: bad category %c", category);
    return TRUE;
}

#ifdef DUMPLOG
STATIC_OVL void
dump_plines()
{
    int i, j;
    char buf[BUFSZ], **strp;

    Strcpy(buf, " "); /* one space for indentation */
    putstr(0, 0, "Latest messages:");
    for (i = 0, j = (int) g.saved_pline_index; i < DUMPLOG_MSG_COUNT;
         ++i, j = (j + 1) % DUMPLOG_MSG_COUNT) {
        strp = &g.saved_plines[j];
        if (*strp) {
            copynchars(&buf[1], *strp, BUFSZ - 1 - 1);
            putstr(0, 0, buf);
#ifdef FREE_ALL_MEMORY
            free(*strp), *strp = 0;
#endif
        }
    }
}
#endif

/*ARGSUSED*/
STATIC_OVL void
dump_everything(how, when)
int how;
time_t when; /* date+time at end of game */
{
#ifdef DUMPLOG
    char pbuf[BUFSZ], datetimebuf[24]; /* [24]: room for 64-bit bogus value */

    dump_redirect(TRUE);
    if (!iflags.in_dumplog)
        return;

    init_symbols(); /* revert to default symbol set */

    /* one line version ID, which includes build date+time;
       it's conceivable that the game started with a different
       build date+time or even with an older nethack version,
       but we only have access to the one it finished under */
    putstr(0, 0, getversionstring(pbuf));
    putstr(0, 0, "");

    /* game start and end date+time to disambiguate version date+time */
    Strcpy(datetimebuf, yyyymmddhhmmss(ubirthday));
    Sprintf(pbuf, "Game began %4.4s-%2.2s-%2.2s %2.2s:%2.2s:%2.2s",
            &datetimebuf[0], &datetimebuf[4], &datetimebuf[6],
            &datetimebuf[8], &datetimebuf[10], &datetimebuf[12]);
    Strcpy(datetimebuf, yyyymmddhhmmss(when));
    Sprintf(eos(pbuf), ", ended %4.4s-%2.2s-%2.2s %2.2s:%2.2s:%2.2s.",
            &datetimebuf[0], &datetimebuf[4], &datetimebuf[6],
            &datetimebuf[8], &datetimebuf[10], &datetimebuf[12]);
    putstr(0, 0, pbuf);
    putstr(0, 0, "");

    /* character name and basic role info */
    Sprintf(pbuf, "%s, %s %s %s %s", plname,
            aligns[1 - u.ualign.type].adj,
            genders[flags.female].adj,
            urace.adj,
            (flags.female && urole.name.f) ? urole.name.f : urole.name.m);
    putstr(0, 0, pbuf);
    putstr(0, 0, "");

    dump_map();
    putstr(0, 0, do_statusline1());
    putstr(0, 0, do_statusline2());
    putstr(0, 0, "");

    dump_plines();
    putstr(0, 0, "");
    putstr(0, 0, "Inventory:");
    (void) display_inventory((char *) 0, TRUE);
    container_contents(invent, TRUE, TRUE, FALSE);
    enlightenment((BASICENLIGHTENMENT | MAGICENLIGHTENMENT),
                  (how >= PANICKED) ? ENL_GAMEOVERALIVE : ENL_GAMEOVERDEAD);
    putstr(0, 0, "");
    list_vanquished('d', FALSE); /* 'd' => 'y' */
    putstr(0, 0, "");
    list_genocided('d', FALSE); /* 'd' => 'y' */
    putstr(0, 0, "");
    show_conduct((how >= PANICKED) ? 1 : 2);
    putstr(0, 0, "");
    show_overview((how >= PANICKED) ? 1 : 2, how);
    putstr(0, 0, "");
    dump_redirect(FALSE);
#else
    nhUse(how);
    nhUse(when);
#endif
}

STATIC_OVL void
disclose(how, taken)
int how;
boolean taken;
{
    char c = '\0', defquery;
    char qbuf[QBUFSZ];
    boolean ask = FALSE;

    if (invent && !done_stopprint) {
        if (taken)
            Sprintf(qbuf, "Do you want to see what you had when you %s?",
                    (how == QUIT) ? "quit" : "died");
        else
            Strcpy(qbuf, "Do you want your possessions identified?");

        ask = should_query_disclose_option('i', &defquery);
        c = ask ? yn_function(qbuf, ynqchars, defquery) : defquery;
        if (c == 'y') {
            /* caller has already ID'd everything */
            (void) display_inventory((char *) 0, TRUE);
            container_contents(invent, TRUE, TRUE, FALSE);
        }
        if (c == 'q')
            done_stopprint++;
    }

    if (!done_stopprint) {
        ask = should_query_disclose_option('a', &defquery);
        c = ask ? yn_function("Do you want to see your attributes?", ynqchars,
                              defquery)
                : defquery;
        if (c == 'y')
            enlightenment((BASICENLIGHTENMENT | MAGICENLIGHTENMENT),
                          (how >= PANICKED) ? ENL_GAMEOVERALIVE
                                            : ENL_GAMEOVERDEAD);
        if (c == 'q')
            done_stopprint++;
    }

    if (!done_stopprint) {
        ask = should_query_disclose_option('v', &defquery);
        list_vanquished(defquery, ask);
    }

    if (!done_stopprint) {
        ask = should_query_disclose_option('g', &defquery);
        list_genocided(defquery, ask);
    }

    if (!done_stopprint) {
        ask = should_query_disclose_option('c', &defquery);
        c = ask ? yn_function("Do you want to see your conduct?", ynqchars,
                              defquery)
                : defquery;
        if (c == 'y')
            show_conduct((how >= PANICKED) ? 1 : 2);
        if (c == 'q')
            done_stopprint++;
    }

    if (!done_stopprint) {
        ask = should_query_disclose_option('o', &defquery);
        c = ask ? yn_function("Do you want to see the dungeon overview?",
                              ynqchars, defquery)
                : defquery;
        if (c == 'y')
            show_overview((how >= PANICKED) ? 1 : 2, how);
        if (c == 'q')
            done_stopprint++;
    }
}

/* try to get the player back in a viable state after being killed */
STATIC_OVL void
savelife(how)
int how;
{
    int uhpmin = max(2 * u.ulevel, 10);

    if (u.uhpmax < uhpmin)
        u.uhpmax = uhpmin;
    u.uhp = u.uhpmax;
    if (Upolyd) /* Unchanging, or death which bypasses losing hit points */
        u.mh = u.mhmax;
    if (u.uhunger < 500 || how == CHOKING) {
        init_uhunger();
    }
    /* cure impending doom of sickness hero won't have time to fix */
    if ((Sick & TIMEOUT) == 1L) {
        make_sick(0L, (char *) 0, FALSE, SICK_ALL);
    }
    nomovemsg = "You survived that attempt on your life.";
    context.move = 0;
    if (g.multi > 0)
        g.multi = 0;
    else
        g.multi = -1;
    if (u.utrap && u.utraptype == TT_LAVA)
        reset_utrap(FALSE);
    context.botl = 1;
    u.ugrave_arise = NON_PM;
    HUnchanging = 0L;
    curs_on_u();
    if (!context.mon_moving)
        endmultishot(FALSE);
    if (u.uswallow) {
        /* might drop hero onto a trap that kills her all over again */
        expels(u.ustuck, u.ustuck->data, TRUE);
    } else if (u.ustuck) {
        if (Upolyd && sticks(youmonst.data))
            You("release %s.", mon_nam(u.ustuck));
        else
            pline("%s releases you.", Monnam(u.ustuck));
        unstuck(u.ustuck);
    }
}

/*
 * Get valuables from the given list.  Revised code: the list always remains
 * intact.
 */
STATIC_OVL void
get_valuables(list)
struct obj *list; /* inventory or container contents */
{
    register struct obj *obj;
    register int i;

    /* find amulets and gems, ignoring all artifacts */
    for (obj = list; obj; obj = obj->nobj)
        if (Has_contents(obj)) {
            get_valuables(obj->cobj);
        } else if (obj->oartifact) {
            continue;
        } else if (obj->oclass == AMULET_CLASS) {
            i = obj->otyp - FIRST_AMULET;
            if (!g.amulets[i].count) {
                g.amulets[i].count = obj->quan;
                g.amulets[i].typ = obj->otyp;
            } else
                g.amulets[i].count += obj->quan; /* always adds one */
        } else if (obj->oclass == GEM_CLASS && obj->otyp < LUCKSTONE) {
            i = min(obj->otyp, LAST_GEM + 1) - FIRST_GEM;
            if (!g.gems[i].count) {
                g.gems[i].count = obj->quan;
                g.gems[i].typ = obj->otyp;
            } else
                g.gems[i].count += obj->quan;
        }
    return;
}

/*
 *  Sort collected valuables, most frequent to least.  We could just
 *  as easily use qsort, but we don't care about efficiency here.
 */
STATIC_OVL void
sort_valuables(list, size)
struct valuable_data list[];
int size; /* max value is less than 20 */
{
    register int i, j;
    struct valuable_data ltmp;

    /* move greater quantities to the front of the list */
    for (i = 1; i < size; i++) {
        if (list[i].count == 0)
            continue;   /* empty slot */
        ltmp = list[i]; /* structure copy */
        for (j = i; j > 0; --j)
            if (list[j - 1].count >= ltmp.count)
                break;
            else {
                list[j] = list[j - 1];
            }
        list[j] = ltmp;
    }
    return;
}

#if 0
/*
 * odds_and_ends() was used for 3.6.0 and 3.6.1.
 * Schroedinger's Cat is handled differently starting with 3.6.2.
 */
STATIC_DCL boolean FDECL(odds_and_ends, (struct obj *, int));

#define CAT_CHECK 2

STATIC_OVL boolean
odds_and_ends(list, what)
struct obj *list;
int what;
{
    struct obj *otmp;

    for (otmp = list; otmp; otmp = otmp->nobj) {
        switch (what) {
        case CAT_CHECK: /* Schroedinger's Cat */
            /* Ascending is deterministic */
            if (SchroedingersBox(otmp))
                return rn2(2);
            break;
        }
        if (Has_contents(otmp))
            return odds_and_ends(otmp->cobj, what);
    }
    return FALSE;
}
#endif

/* called twice; first to calculate total, then to list relevant items */
STATIC_OVL void
artifact_score(list, counting, endwin)
struct obj *list;
boolean counting; /* true => add up points; false => display them */
winid endwin;
{
    char pbuf[BUFSZ];
    struct obj *otmp;
    long value, points;
    short dummy; /* object type returned by artifact_name() */

    for (otmp = list; otmp; otmp = otmp->nobj) {
        if (otmp->oartifact || otmp->otyp == BELL_OF_OPENING
            || otmp->otyp == SPE_BOOK_OF_THE_DEAD
            || otmp->otyp == CANDELABRUM_OF_INVOCATION) {
            value = arti_cost(otmp); /* zorkmid value */
            points = value * 5 / 2;  /* score value */
            if (counting) {
                nowrap_add(u.urexp, points);
            } else {
                discover_object(otmp->otyp, TRUE, FALSE);
                otmp->known = otmp->dknown = otmp->bknown = otmp->rknown = 1;
                /* assumes artifacts don't have quan > 1 */
                Sprintf(pbuf, "%s%s (worth %ld %s and %ld points)",
                        the_unique_obj(otmp) ? "The " : "",
                        otmp->oartifact ? artifact_name(xname(otmp), &dummy)
                                        : OBJ_NAME(objects[otmp->otyp]),
                        value, currency(value), points);
                putstr(endwin, 0, pbuf);
            }
        }
        if (Has_contents(otmp))
            artifact_score(otmp->cobj, counting, endwin);
    }
}

/* Be careful not to call panic from here! */
void
done(how)
int how;
{
    boolean survive = FALSE;

    if (how == TRICKED) {
        if (killer.name[0]) {
            paniclog("trickery", killer.name);
            killer.name[0] = '\0';
        }
        if (wizard) {
            You("are a very tricky wizard, it seems.");
            killer.format = KILLED_BY_AN; /* reset to 0 */
            return;
        }
    }
    if (program_state.panicking
#ifdef HANGUPHANDLING
        || program_state.done_hup
#endif
        ) {
        /* skip status update if panicking or disconnected */
        context.botl = context.botlx = FALSE;
    } else {
        /* otherwise force full status update */
        context.botlx = TRUE;
        bot();
    }

    if (iflags.debug_fuzzer) {
        if (!(program_state.panicking || how == PANICKED)) {
            savelife(how);
            /* periodically restore characteristics and lost exp levels
               or cure lycanthropy */
            if (!rn2(10)) {
                struct obj *potion = mksobj((u.ulycn > LOW_PM && !rn2(3))
                                            ? POT_WATER : POT_RESTORE_ABILITY,
                                            TRUE, FALSE);

                bless(potion);
                (void) peffects(potion); /* always -1 for restore ability */
                /* not useup(); we haven't put this potion into inventory */
                obfree(potion, (struct obj *) 0);
            }
            killer.name[0] = '\0';
            killer.format = 0;
            return;
        }
    } else
    if (how == ASCENDED || (!killer.name[0] && how == GENOCIDED))
        killer.format = NO_KILLER_PREFIX;
    /* Avoid killed by "a" burning or "a" starvation */
    if (!killer.name[0] && (how == STARVING || how == BURNING))
        killer.format = KILLED_BY;
    if (!killer.name[0] || how >= PANICKED)
        Strcpy(killer.name, deaths[how]);

    if (how < PANICKED) {
        u.umortality++;
        /* in case caller hasn't already done this */
        if (u.uhp > 0 || (Upolyd && u.mh > 0)) {
            /* for deaths not triggered by loss of hit points, force
               current HP to zero (0 HP when turning into green slime
               is iffy but we don't have much choice--that is fatal) */
            u.uhp = u.mh = 0;
            context.botl = 1;
        }
    }
    if (Lifesaved && (how <= GENOCIDED)) {
        pline("But wait...");
        makeknown(AMULET_OF_LIFE_SAVING);
        Your("medallion %s!", !Blind ? "begins to glow" : "feels warm");
        if (how == CHOKING)
            You("vomit ...");
        You_feel("much better!");
        pline_The("medallion crumbles to dust!");
        if (uamul)
            useup(uamul);

        (void) adjattrib(A_CON, -1, TRUE);
        savelife(how);
        if (how == GENOCIDED) {
            pline("Unfortunately you are still genocided...");
        } else {
            survive = TRUE;
        }
    }
    /* explore and wizard modes offer player the option to keep playing */
    if (!survive && (wizard || discover) && how <= GENOCIDED
        && !paranoid_query(ParanoidDie, "Die?")) {
        pline("OK, so you don't %s.", (how == CHOKING) ? "choke" : "die");
        savelife(how);
        survive = TRUE;
    }

    if (survive) {
        killer.name[0] = '\0';
        killer.format = KILLED_BY_AN; /* reset to 0 */
        return;
    }
    really_done(how);
    /*NOTREACHED*/
}

/* separated from done() in order to specify the __noreturn__ attribute */
STATIC_OVL void
really_done(how)
int how;
{
    boolean taken;
    char pbuf[BUFSZ];
    winid endwin = WIN_ERR;
    boolean bones_ok, have_windows = iflags.window_inited;
    struct obj *corpse = (struct obj *) 0;
    time_t endtime;
    long umoney;
    long tmp;

    /*
     *  The game is now over...
     */
    program_state.gameover = 1;
    /* in case of a subsequent panic(), there's no point trying to save */
    program_state.something_worth_saving = 0;
    /* render vision subsystem inoperative */
    iflags.vision_inited = 0;

    /* might have been killed while using a disposable item, so make sure
       it's gone prior to inventory disclosure and creation of bones data */
    inven_inuse(TRUE);
    /* maybe not on object lists; if an active light source, would cause
       big trouble (`obj_is_local' panic) for savebones() -> savelev() */
    if (thrownobj && thrownobj->where == OBJ_FREE)
        dealloc_obj(thrownobj);
    if (kickedobj && kickedobj->where == OBJ_FREE)
        dealloc_obj(kickedobj);

    /* remember time of death here instead of having bones, rip, and
       topten figure it out separately and possibly getting different
       time or even day if player is slow responding to --More-- */
    urealtime.finish_time = endtime = getnow();
    urealtime.realtime += (long) (endtime - urealtime.start_timing);

    dump_open_log(endtime);
    /* Sometimes you die on the first move.  Life's not fair.
     * On those rare occasions you get hosed immediately, go out
     * smiling... :-)  -3.
     */
    if (moves <= 1 && how < PANICKED) /* You die... --More-- */
        pline("Do not pass go.  Do not collect 200 %s.", currency(200L));

    if (have_windows)
        wait_synch(); /* flush screen output */
#ifndef NO_SIGNAL
    (void) signal(SIGINT, (SIG_RET_TYPE) done_intr);
#if defined(UNIX) || defined(VMS) || defined(__EMX__)
    (void) signal(SIGQUIT, (SIG_RET_TYPE) done_intr);
    sethanguphandler(done_hangup);
#endif
#endif /* NO_SIGNAL */

    bones_ok = (how < GENOCIDED) && can_make_bones();

    if (bones_ok && launch_in_progress())
        force_launch_placement();

    /* maintain ugrave_arise even for !bones_ok */
    if (how == PANICKED)
        u.ugrave_arise = (NON_PM - 3); /* no corpse, no grave */
    else if (how == BURNING || how == DISSOLVED) /* corpse burns up too */
        u.ugrave_arise = (NON_PM - 2); /* leave no corpse */
    else if (how == STONING)
        u.ugrave_arise = (NON_PM - 1); /* statue instead of corpse */
    else if (how == TURNED_SLIME
             /* it's possible to turn into slime even though green slimes
                have been genocided:  genocide could occur after hero is
                already infected or hero could eat a glob of one created
                before genocide; don't try to arise as one if they're gone */
             && !(mvitals[PM_GREEN_SLIME].mvflags & G_GENOD))
        u.ugrave_arise = PM_GREEN_SLIME;

    if (how == QUIT) {
        killer.format = NO_KILLER_PREFIX;
        if (u.uhp < 1) {
            how = DIED;
            u.umortality++; /* skipped above when how==QUIT */
            Strcpy(killer.name, "quit while already on Charon's boat");
        }
    }
    if (how == ESCAPED || how == PANICKED)
        killer.format = NO_KILLER_PREFIX;

    fixup_death(how); /* actually, fixup g.multi_reason */

    if (how != PANICKED) {
        /* these affect score and/or bones, but avoid them during panic */
        taken = paybill((how == ESCAPED) ? -1 : (how != QUIT));
        paygd();
        clearpriests();
    } else
        taken = FALSE; /* lint; assert( !bones_ok ); */

    clearlocks();

    if (have_windows)
        display_nhwindow(WIN_MESSAGE, FALSE);

    if (how != PANICKED) {
        struct obj *obj;

        /*
         * This is needed for both inventory disclosure and dumplog.
         * Both are optional, so do it once here instead of duplicating
         * it in both of those places.
         */
        for (obj = invent; obj; obj = obj->nobj) {
            discover_object(obj->otyp, TRUE, FALSE);
            obj->known = obj->bknown = obj->dknown = obj->rknown = 1;
            if (Is_container(obj) || obj->otyp == STATUE)
                obj->cknown = obj->lknown = 1;
            /* we resolve Schroedinger's cat now in case of both
               disclosure and dumplog, where the 50:50 chance for
               live cat has to be the same both times */
            if (SchroedingersBox(obj)) {
                if (!Schroedingers_cat) {
                    /* tell observe_quantum_cat() not to create a cat; if it
                       chooses live cat in this situation, it will leave the
                       SchroedingersBox flag set (for container_contents()) */
                    observe_quantum_cat(obj, FALSE, FALSE);
                    if (SchroedingersBox(obj))
                        Schroedingers_cat = TRUE;
                } else
                    obj->spe = 0; /* ordinary box with cat corpse in it */
            }
        }

        if (strcmp(flags.end_disclose, "none"))
            disclose(how, taken);

        dump_everything(how, endtime);
    }

    /* if pets will contribute to score, populate mydogs list now
       (bones creation isn't a factor, but pline() messaging is; used to
       be done even sooner, but we need it to come after dump_everything()
       so that any accompanying pets are still on the map during dump) */
    if (how == ESCAPED || how == ASCENDED)
        keepdogs(TRUE);

    /* finish_paybill should be called after disclosure but before bones */
    if (bones_ok && taken)
        finish_paybill();

    /* grave creation should be after disclosure so it doesn't have
       this grave in the current level's features for #overview */
    if (bones_ok && u.ugrave_arise == NON_PM
        && !(mvitals[u.umonnum].mvflags & G_NOCORPSE)) {
        int mnum = u.umonnum;

        if (!Upolyd) {
            /* Base corpse on race when not poly'd since original
             * u.umonnum is based on role, and all role monsters
             * are human.
             */
            mnum = (flags.female && urace.femalenum != NON_PM)
                       ? urace.femalenum
                       : urace.malenum;
        }
        corpse = mk_named_object(CORPSE, &mons[mnum], u.ux, u.uy, plname);
        Sprintf(pbuf, "%s, ", plname);
        formatkiller(eos(pbuf), sizeof pbuf - strlen(pbuf), how, TRUE);
        make_grave(u.ux, u.uy, pbuf);
    }
    pbuf[0] = '\0'; /* clear grave text; also lint suppression */

    /* calculate score, before creating bones [container gold] */
    {
        int deepest = deepest_lev_reached(FALSE);

        umoney = money_cnt(invent);
        tmp = u.umoney0;
        umoney += hidden_gold(); /* accumulate gold from containers */
        tmp = umoney - tmp;      /* net gain */

        if (tmp < 0L)
            tmp = 0L;
        if (how < PANICKED)
            tmp -= tmp / 10L;
        tmp += 50L * (long) (deepest - 1);
        if (deepest > 20)
            tmp += 1000L * (long) ((deepest > 30) ? 10 : deepest - 20);
        nowrap_add(u.urexp, tmp);

        /* ascension gives a score bonus iff offering to original deity */
        if (how == ASCENDED && u.ualign.type == u.ualignbase[A_ORIGINAL]) {
            /* retaining original alignment: score *= 2;
               converting, then using helm-of-OA to switch back: *= 1.5 */
            tmp = (u.ualignbase[A_CURRENT] == u.ualignbase[A_ORIGINAL])
                      ? u.urexp
                      : (u.urexp / 2L);
            nowrap_add(u.urexp, tmp);
        }
    }

    if (u.ugrave_arise >= LOW_PM) {
        /* give this feedback even if bones aren't going to be created,
           so that its presence or absence doesn't tip off the player to
           new bones or their lack; it might be a lie if makemon fails */
        Your("%s as %s...",
             (u.ugrave_arise != PM_GREEN_SLIME)
                 ? "body rises from the dead"
                 : "revenant persists",
             an(mons[u.ugrave_arise].mname));
        display_nhwindow(WIN_MESSAGE, FALSE);
    }

    if (bones_ok) {
        if (!wizard || paranoid_query(ParanoidBones, "Save bones?"))
            savebones(how, endtime, corpse);
        /* corpse may be invalid pointer now so
            ensure that it isn't used again */
        corpse = (struct obj *) 0;
    }

    /* update gold for the rip output, which can't use hidden_gold()
       (containers will be gone by then if bones just got saved...) */
    done_money = umoney;

    /* clean up unneeded windows */
    if (have_windows) {
        wait_synch();
        free_pickinv_cache(); /* extra persistent window if perm_invent */
        if (WIN_INVEN != WIN_ERR) {
            destroy_nhwindow(WIN_INVEN),  WIN_INVEN = WIN_ERR;
            /* precaution in case any late update_inventory() calls occur */
            iflags.perm_invent = 0;
        }
        display_nhwindow(WIN_MESSAGE, TRUE);
        destroy_nhwindow(WIN_MAP),  WIN_MAP = WIN_ERR;
#ifndef STATUS_HILITES
        destroy_nhwindow(WIN_STATUS),  WIN_STATUS = WIN_ERR;
#endif
        destroy_nhwindow(WIN_MESSAGE),  WIN_MESSAGE = WIN_ERR;

        if (!done_stopprint || flags.tombstone)
            endwin = create_nhwindow(NHW_TEXT);

        if (how < GENOCIDED && flags.tombstone && endwin != WIN_ERR)
            outrip(endwin, how, endtime);
    } else
        done_stopprint = 1; /* just avoid any more output */

#ifdef DUMPLOG
    /* 'how' reasons beyond genocide shouldn't show tombstone;
       for normal end of game, genocide doesn't either */
    if (how <= GENOCIDED) {
        dump_redirect(TRUE);
        if (iflags.in_dumplog)
            genl_outrip(0, how, endtime);
        dump_redirect(FALSE);
    }
#endif
    if (u.uhave.amulet) {
        Strcat(killer.name, " (with the Amulet)");
    } else if (how == ESCAPED) {
        if (Is_astralevel(&u.uz)) /* offered Amulet to wrong deity */
            Strcat(killer.name, " (in celestial disgrace)");
        else if (carrying(FAKE_AMULET_OF_YENDOR))
            Strcat(killer.name, " (with a fake Amulet)");
        /* don't bother counting to see whether it should be plural */
    }

    Sprintf(pbuf, "%s %s the %s...", Goodbye(), plname,
            (how != ASCENDED)
                ? (const char *) ((flags.female && urole.name.f)
                    ? urole.name.f
                    : urole.name.m)
                : (const char *) (flags.female ? "Demigoddess" : "Demigod"));
    dump_forward_putstr(endwin, 0, pbuf, done_stopprint);
    dump_forward_putstr(endwin, 0, "", done_stopprint);

    if (how == ESCAPED || how == ASCENDED) {
        struct monst *mtmp;
        struct obj *otmp;
        register struct val_list *val;
        register int i;

        for (val = g.valuables; val->list; val++)
            for (i = 0; i < val->size; i++) {
                val->list[i].count = 0L;
            }
        get_valuables(invent);

        /* add points for collected valuables */
        for (val = g.valuables; val->list; val++)
            for (i = 0; i < val->size; i++)
                if (val->list[i].count != 0L) {
                    tmp = val->list[i].count
                          * (long) objects[val->list[i].typ].oc_cost;
                    nowrap_add(u.urexp, tmp);
                }

        /* count the points for artifacts */
        artifact_score(invent, TRUE, endwin);

        viz_array[0][0] |= IN_SIGHT; /* need visibility for naming */
        mtmp = mydogs;
        Strcpy(pbuf, "You");
        if (mtmp || Schroedingers_cat) {
            while (mtmp) {
                Sprintf(eos(pbuf), " and %s", mon_nam(mtmp));
                if (mtmp->mtame)
                    nowrap_add(u.urexp, mtmp->mhp);
                mtmp = mtmp->nmon;
            }
            /* [it might be more robust to create a housecat and add it to
               mydogs; it doesn't have to be placed on the map for that] */
            if (Schroedingers_cat) {
                int mhp, m_lev = adj_lev(&mons[PM_HOUSECAT]);

                mhp = d(m_lev, 8);
                nowrap_add(u.urexp, mhp);
                Strcat(eos(pbuf), " and Schroedinger's cat");
            }
            dump_forward_putstr(endwin, 0, pbuf, done_stopprint);
            pbuf[0] = '\0';
        } else {
            Strcat(pbuf, " ");
        }
        Sprintf(eos(pbuf), "%s with %ld point%s,",
                how == ASCENDED ? "went to your reward"
                                 : "escaped from the dungeon",
                u.urexp, plur(u.urexp));
        dump_forward_putstr(endwin, 0, pbuf, done_stopprint);

        if (!done_stopprint)
            artifact_score(invent, FALSE, endwin); /* list artifacts */
#ifdef DUMPLOG
        dump_redirect(TRUE);
        if (iflags.in_dumplog)
            artifact_score(invent, FALSE, 0);
        dump_redirect(FALSE);
#endif

        /* list valuables here */
        for (val = g.valuables; val->list; val++) {
            sort_valuables(val->list, val->size);
            for (i = 0; i < val->size && !done_stopprint; i++) {
                int typ = val->list[i].typ;
                long count = val->list[i].count;

                if (count == 0L)
                    continue;
                if (objects[typ].oc_class != GEM_CLASS || typ <= LAST_GEM) {
                    otmp = mksobj(typ, FALSE, FALSE);
                    discover_object(otmp->otyp, TRUE, FALSE);
                    otmp->known = 1;  /* for fake amulets */
                    otmp->dknown = 1; /* seen it (blindness fix) */
                    if (has_oname(otmp))
                        free_oname(otmp);
                    otmp->quan = count;
                    Sprintf(pbuf, "%8ld %s (worth %ld %s),", count,
                            xname(otmp), count * (long) objects[typ].oc_cost,
                            currency(2L));
                    obfree(otmp, (struct obj *) 0);
                } else {
                    Sprintf(pbuf, "%8ld worthless piece%s of colored glass,",
                            count, plur(count));
                }
                dump_forward_putstr(endwin, 0, pbuf, 0);
            }
        }

    } else {
        /* did not escape or ascend */
        if (u.uz.dnum == 0 && u.uz.dlevel <= 0) {
            /* level teleported out of the dungeon; `how' is DIED,
               due to falling or to "arriving at heaven prematurely" */
            Sprintf(pbuf, "You %s beyond the confines of the dungeon",
                    (u.uz.dlevel < 0) ? "passed away" : ends[how]);
        } else {
            /* more conventional demise */
            const char *where = dungeons[u.uz.dnum].dname;

            if (Is_astralevel(&u.uz))
                where = "The Astral Plane";
            Sprintf(pbuf, "You %s in %s", ends[how], where);
            if (!In_endgame(&u.uz) && !Is_knox(&u.uz))
                Sprintf(eos(pbuf), " on dungeon level %d",
                        In_quest(&u.uz) ? dunlev(&u.uz) : depth(&u.uz));
        }

        Sprintf(eos(pbuf), " with %ld point%s,", u.urexp, plur(u.urexp));
        dump_forward_putstr(endwin, 0, pbuf, done_stopprint);
    }

    Sprintf(pbuf, "and %ld piece%s of gold, after %ld move%s.", umoney,
            plur(umoney), moves, plur(moves));
    dump_forward_putstr(endwin, 0, pbuf, done_stopprint);
    Sprintf(pbuf,
            "You were level %d with a maximum of %d hit point%s when you %s.",
            u.ulevel, u.uhpmax, plur(u.uhpmax), ends[how]);
    dump_forward_putstr(endwin, 0, pbuf, done_stopprint);
    dump_forward_putstr(endwin, 0, "", done_stopprint);
    if (!done_stopprint)
        display_nhwindow(endwin, TRUE);
    if (endwin != WIN_ERR)
        destroy_nhwindow(endwin);

    dump_close_log();
    /* "So when I die, the first thing I will see in Heaven is a
     * score list?" */
    if (have_windows && !iflags.toptenwin)
        exit_nhwindows((char *) 0), have_windows = FALSE;
    topten(how, endtime);
    if (have_windows)
        exit_nhwindows((char *) 0);

    if (done_stopprint) {
        raw_print("");
        raw_print("");
    }
    nh_terminate(EXIT_SUCCESS);
}

void
container_contents(list, identified, all_containers, reportempty)
struct obj *list;
boolean identified, all_containers, reportempty;
{
    register struct obj *box, *obj;
    char buf[BUFSZ];
    boolean cat,
            dumping =
#ifdef DUMPLOG
                     iflags.in_dumplog ? TRUE :
#endif
                     FALSE;

    for (box = list; box; box = box->nobj) {
        if (Is_container(box) || box->otyp == STATUE) {
            box->cknown = 1; /* we're looking at the contents now */
            if (identified)
                box->lknown = 1;
            if (box->otyp == BAG_OF_TRICKS) {
                continue; /* wrong type of container */
            } else if (box->cobj) {
                winid tmpwin = create_nhwindow(NHW_MENU);
                Loot *sortedcobj, *srtc;
                unsigned sortflags;

                /* at this stage, the SchroedingerBox() flag is only set
                   if the cat inside the box is alive; the box actually
                   contains a cat corpse that we'll pretend is not there;
                   for dead cat, the flag will be clear and there'll be
                   a cat corpse inside the box; either way, inventory
                   reports the box as containing "1 item" */
                cat = SchroedingersBox(box);

                Sprintf(buf, "Contents of %s:", the(xname(box)));
                putstr(tmpwin, 0, buf);
                if (!dumping)
                    putstr(tmpwin, 0, "");
                buf[0] = buf[1] = ' '; /* two leading spaces */
                if (box->cobj && !cat) {
                    sortflags = (((flags.sortloot == 'l'
                                   || flags.sortloot == 'f')
                                     ? SORTLOOT_LOOT : 0)
                                 | (flags.sortpack ? SORTLOOT_PACK : 0));
                    sortedcobj = sortloot(&box->cobj, sortflags, FALSE,
                                          (boolean FDECL((*), (OBJ_P))) 0);
                    for (srtc = sortedcobj; ((obj = srtc->obj) != 0); ++srtc) {
                        if (identified) {
                            discover_object(obj->otyp, TRUE, FALSE);
                            obj->known = obj->bknown = obj->dknown
                                = obj->rknown = 1;
                            if (Is_container(obj) || obj->otyp == STATUE)
                                obj->cknown = obj->lknown = 1;
                        }
                        Strcpy(&buf[2], doname(obj));
                        putstr(tmpwin, 0, buf);
                    }
                    unsortloot(&sortedcobj);
                } else if (cat) {
                    Strcpy(&buf[2], "Schroedinger's cat!");
                    putstr(tmpwin, 0, buf);
                }
                if (dumping)
                    putstr(0, 0, "");
                display_nhwindow(tmpwin, TRUE);
                destroy_nhwindow(tmpwin);
                if (all_containers)
                    container_contents(box->cobj, identified, TRUE,
                                       reportempty);
            } else if (reportempty) {
                pline("%s is empty.", upstart(thesimpleoname(box)));
                display_nhwindow(WIN_MESSAGE, FALSE);
            }
        }
        if (!all_containers)
            break;
    }
}

/* should be called with either EXIT_SUCCESS or EXIT_FAILURE */
void
nh_terminate(status)
int status;
{
    program_state.in_moveloop = 0; /* won't be returning to normal play */
#ifdef MAC
    getreturn("to exit");
#endif
    /* don't bother to try to release memory if we're in panic mode, to
       avoid trouble in case that happens to be due to memory problems */
    if (!program_state.panicking) {
        freedynamicdata();
        dlb_cleanup();
    }

#ifdef VMS
    /*
     *  This is liable to draw a warning if compiled with gcc, but it's
     *  more important to flag panic() -> really_done() -> nh_terminate()
     *  as __noreturn__ then to avoid the warning.
     */
    /* don't call exit() if already executing within an exit handler;
       that would cancel any other pending user-mode handlers */
    if (program_state.exiting)
        return;
#endif
    program_state.exiting = 1;
    nethack_exit(status);
}

static const char *vanqorders[NUM_VANQ_ORDER_MODES] = {
    "traditional: by monster level, by internal monster index",
    "by monster toughness, by internal monster index",
    "alphabetically, first unique monsters, then others",
    "alphabetically, unique monsters and others intermixed",
    "by monster class, high to low level within class",
    "by monster class, low to high level within class",
    "by count, high to low, by internal index within tied count",
    "by count, low to high, by internal index within tied count",
};

STATIC_PTR int CFDECLSPEC
vanqsort_cmp(vptr1, vptr2)
const genericptr vptr1;
const genericptr vptr2;
{
    int indx1 = *(short *) vptr1, indx2 = *(short *) vptr2,
        mlev1, mlev2, mstr1, mstr2, uniq1, uniq2, died1, died2, res;
    const char *name1, *name2, *punct;
    schar mcls1, mcls2;

    switch (g.vanq_sortmode) {
    default:
    case VANQ_MLVL_MNDX:
        /* sort by monster level */
        mlev1 = mons[indx1].mlevel, mlev2 = mons[indx2].mlevel;
        res = mlev2 - mlev1; /* mlevel high to low */
        break;
    case VANQ_MSTR_MNDX:
        /* sort by monster toughness */
        mstr1 = mons[indx1].difficulty, mstr2 = mons[indx2].difficulty;
        res = mstr2 - mstr1; /* monstr high to low */
        break;
    case VANQ_ALPHA_SEP:
        uniq1 = ((mons[indx1].geno & G_UNIQ) && indx1 != PM_HIGH_PRIEST);
        uniq2 = ((mons[indx2].geno & G_UNIQ) && indx2 != PM_HIGH_PRIEST);
        if (uniq1 ^ uniq2) { /* one or other uniq, but not both */
            res = uniq2 - uniq1;
            break;
        } /* else both unique or neither unique */
        /*FALLTHRU*/
    case VANQ_ALPHA_MIX:
        name1 = mons[indx1].mname, name2 = mons[indx2].mname;
        res = strcmpi(name1, name2); /* caseblind alhpa, low to high */
        break;
    case VANQ_MCLS_HTOL:
    case VANQ_MCLS_LTOH:
        /* mons[].mlet is a small integer, 1..N, of type plain char;
           if 'char' happens to be unsigned, (mlet1 - mlet2) would yield
           an inappropriate result when mlet2 is greater than mlet1,
           so force our copies (mcls1, mcls2) to be signed */
        mcls1 = (schar) mons[indx1].mlet, mcls2 = (schar) mons[indx2].mlet;
        /* S_ANT through S_ZRUTY correspond to lowercase monster classes,
           S_ANGEL through S_ZOMBIE correspond to uppercase, and various
           punctuation characters are used for classes beyond those */
        if (mcls1 > S_ZOMBIE && mcls2 > S_ZOMBIE) {
            /* force a specific order to the punctuation classes that's
               different from the internal order;
               internal order is ok if neither or just one is punctuation
               since letters have lower values so come out before punct */
            static const char punctclasses[] = {
                S_LIZARD, S_EEL, S_GOLEM, S_GHOST, S_DEMON, S_HUMAN, '\0'
            };

            if ((punct = index(punctclasses, mcls1)) != 0)
                mcls1 = (schar) (S_ZOMBIE + 1 + (int) (punct - punctclasses));
            if ((punct = index(punctclasses, mcls2)) != 0)
                mcls2 = (schar) (S_ZOMBIE + 1 + (int) (punct - punctclasses));
        }
        res = mcls1 - mcls2; /* class */
        if (res == 0) {
            mlev1 = mons[indx1].mlevel, mlev2 = mons[indx2].mlevel;
            res = mlev1 - mlev2; /* mlevel low to high */
            if (g.vanq_sortmode == VANQ_MCLS_HTOL)
                res = -res; /* mlevel high to low */
        }
        break;
    case VANQ_COUNT_H_L:
    case VANQ_COUNT_L_H:
        died1 = mvitals[indx1].died, died2 = mvitals[indx2].died;
        res = died2 - died1; /* dead count high to low */
        if (g.vanq_sortmode == VANQ_COUNT_L_H)
            res = -res; /* dead count low to high */
        break;
    }
    /* tiebreaker: internal mons[] index */
    if (res == 0)
        res = indx1 - indx2; /* mndx low to high */
    return res;
}

/* returns -1 if cancelled via ESC */
STATIC_OVL int
set_vanq_order()
{
    winid tmpwin;
    menu_item *selected;
    anything any;
    int i, n, choice;

    tmpwin = create_nhwindow(NHW_MENU);
    start_menu(tmpwin);
    any = zeroany; /* zero out all bits */
    for (i = 0; i < SIZE(vanqorders); i++) {
        if (i == VANQ_ALPHA_MIX || i == VANQ_MCLS_HTOL) /* skip these */
            continue;
        any.a_int = i + 1;
        add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, vanqorders[i],
                 (i == g.vanq_sortmode) ? MENU_SELECTED : MENU_UNSELECTED);
    }
    end_menu(tmpwin, "Sort order for vanquished monster counts");

    n = select_menu(tmpwin, PICK_ONE, &selected);
    destroy_nhwindow(tmpwin);
    if (n > 0) {
        choice = selected[0].item.a_int - 1;
        /* skip preselected entry if we have more than one item chosen */
        if (n > 1 && choice == g.vanq_sortmode)
            choice = selected[1].item.a_int - 1;
        free((genericptr_t) selected);
        g.vanq_sortmode = choice;
    }
    return (n < 0) ? -1 : g.vanq_sortmode;
}

/* #vanquished command */
int
dovanquished()
{
    list_vanquished('a', FALSE);
    return 0;
}

/* high priests aren't unique but are flagged as such to simplify something */
#define UniqCritterIndx(mndx) ((mons[mndx].geno & G_UNIQ) \
                               && mndx != PM_HIGH_PRIEST)

STATIC_OVL void
list_vanquished(defquery, ask)
char defquery;
boolean ask;
{
    register int i;
    int pfx, nkilled;
    unsigned ntypes, ni;
    long total_killed = 0L;
    winid klwin;
    short mindx[NUMMONS];
    char c, buf[BUFSZ], buftoo[BUFSZ];
    boolean dumping; /* for DUMPLOG; doesn't need to be conditional */

    dumping = (defquery == 'd');
    if (dumping)
        defquery = 'y';

    /* get totals first */
    ntypes = 0;
    for (i = LOW_PM; i < NUMMONS; i++) {
        if ((nkilled = (int) mvitals[i].died) == 0)
            continue;
        mindx[ntypes++] = i;
        total_killed += (long) nkilled;
    }

    /* vanquished creatures list;
     * includes all dead monsters, not just those killed by the player
     */
    if (ntypes != 0) {
        char mlet, prev_mlet = 0; /* used as small integer, not character */
        boolean class_header, uniq_header, was_uniq = FALSE;

        c = ask ? yn_function(
                            "Do you want an account of creatures vanquished?",
                              ynaqchars, defquery)
                : defquery;
        if (c == 'q')
            done_stopprint++;
        if (c == 'y' || c == 'a') {
            if (c == 'a') { /* ask player to choose sort order */
                /* choose value for vanq_sortmode via menu; ESC cancels list
                   of vanquished monsters but does not set 'done_stopprint' */
                if (set_vanq_order() < 0)
                    return;
            }
            uniq_header = (g.vanq_sortmode == VANQ_ALPHA_SEP);
            class_header = (g.vanq_sortmode == VANQ_MCLS_LTOH
                            || g.vanq_sortmode == VANQ_MCLS_HTOL);

            klwin = create_nhwindow(NHW_MENU);
            putstr(klwin, 0, "Vanquished creatures:");
            if (!dumping)
                putstr(klwin, 0, "");

            qsort((genericptr_t) mindx, ntypes, sizeof *mindx, vanqsort_cmp);
            for (ni = 0; ni < ntypes; ni++) {
                i = mindx[ni];
                nkilled = mvitals[i].died;
                mlet = mons[i].mlet;
                if (class_header && mlet != prev_mlet) {
                    Strcpy(buf, def_monsyms[(int) mlet].explain);
                    putstr(klwin, ask ? 0 : iflags.menu_headings,
                           upstart(buf));
                    prev_mlet = mlet;
                }
                if (UniqCritterIndx(i)) {
                    Sprintf(buf, "%s%s",
                            !type_is_pname(&mons[i]) ? "the " : "",
                            mons[i].mname);
                    if (nkilled > 1) {
                        switch (nkilled) {
                        case 2:
                            Sprintf(eos(buf), " (twice)");
                            break;
                        case 3:
                            Sprintf(eos(buf), " (thrice)");
                            break;
                        default:
                            Sprintf(eos(buf), " (%d times)", nkilled);
                            break;
                        }
                    }
                    was_uniq = TRUE;
                } else {
                    if (uniq_header && was_uniq) {
                        putstr(klwin, 0, "");
                        was_uniq = FALSE;
                    }
                    /* trolls or undead might have come back,
                       but we don't keep track of that */
                    if (nkilled == 1)
                        Strcpy(buf, an(mons[i].mname));
                    else
                        Sprintf(buf, "%3d %s", nkilled,
                                makeplural(mons[i].mname));
                }
                /* number of leading spaces to match 3 digit prefix */
                pfx = !strncmpi(buf, "the ", 3) ? 0
                      : !strncmpi(buf, "an ", 3) ? 1
                        : !strncmpi(buf, "a ", 2) ? 2
                          : !digit(buf[2]) ? 4 : 0;
                if (class_header)
                    ++pfx;
                Sprintf(buftoo, "%*s%s", pfx, "", buf);
                putstr(klwin, 0, buftoo);
            }
            /*
             * if (Hallucination)
             *     putstr(klwin, 0, "and a partridge in a pear tree");
             */
            if (ntypes > 1) {
                if (!dumping)
                    putstr(klwin, 0, "");
                Sprintf(buf, "%ld creatures vanquished.", total_killed);
                putstr(klwin, 0, buf);
            }
            display_nhwindow(klwin, TRUE);
            destroy_nhwindow(klwin);
        }
    } else if (defquery == 'a') {
        /* #dovanquished rather than final disclosure, so pline() is ok */
        pline("No creatures have been vanquished.");
#ifdef DUMPLOG
    } else if (dumping) {
        putstr(0, 0, "No creatures were vanquished."); /* not pline() */
#endif
    }
}

/* number of monster species which have been genocided */
int
num_genocides()
{
    int i, n = 0;

    for (i = LOW_PM; i < NUMMONS; ++i) {
        if (mvitals[i].mvflags & G_GENOD) {
            ++n;
            if (UniqCritterIndx(i))
                impossible("unique creature '%d: %s' genocided?",
                           i, mons[i].mname);
        }
    }
    return n;
}

STATIC_OVL int
num_extinct()
{
    int i, n = 0;

    for (i = LOW_PM; i < NUMMONS; ++i) {
        if (UniqCritterIndx(i))
            continue;
        if ((mvitals[i].mvflags & G_GONE) == G_EXTINCT)
            ++n;
    }
    return n;
}

STATIC_OVL void
list_genocided(defquery, ask)
char defquery;
boolean ask;
{
    register int i;
    int ngenocided, nextinct;
    char c;
    winid klwin;
    char buf[BUFSZ];
    boolean dumping; /* for DUMPLOG; doesn't need to be conditional */

    dumping = (defquery == 'd');
    if (dumping)
        defquery = 'y';

    ngenocided = num_genocides();
    nextinct = num_extinct();

    /* genocided or extinct species list */
    if (ngenocided != 0 || nextinct != 0) {
        Sprintf(buf, "Do you want a list of %sspecies%s%s?",
                (nextinct && !ngenocided) ? "extinct " : "",
                (ngenocided) ? " genocided" : "",
                (nextinct && ngenocided) ? " and extinct" : "");
        c = ask ? yn_function(buf, ynqchars, defquery) : defquery;
        if (c == 'q')
            done_stopprint++;
        if (c == 'y') {
            klwin = create_nhwindow(NHW_MENU);
            Sprintf(buf, "%s%s species:",
                    (ngenocided) ? "Genocided" : "Extinct",
                    (nextinct && ngenocided) ? " or extinct" : "");
            putstr(klwin, 0, buf);
            if (!dumping)
                putstr(klwin, 0, "");

            for (i = LOW_PM; i < NUMMONS; i++) {
                /* uniques can't be genocided but can become extinct;
                   however, they're never reported as extinct, so skip them */
                if (UniqCritterIndx(i))
                    continue;
                if (mvitals[i].mvflags & G_GONE) {
                    Sprintf(buf, " %s", makeplural(mons[i].mname));
                    /*
                     * "Extinct" is unfortunate terminology.  A species
                     * is marked extinct when its birth limit is reached,
                     * but there might be members of the species still
                     * alive, contradicting the meaning of the word.
                     */
                    if ((mvitals[i].mvflags & G_GONE) == G_EXTINCT)
                        Strcat(buf, " (extinct)");
                    putstr(klwin, 0, buf);
                }
            }
            if (!dumping)
                putstr(klwin, 0, "");
            if (ngenocided > 0) {
                Sprintf(buf, "%d species genocided.", ngenocided);
                putstr(klwin, 0, buf);
            }
            if (nextinct > 0) {
                Sprintf(buf, "%d species extinct.", nextinct);
                putstr(klwin, 0, buf);
            }

            display_nhwindow(klwin, TRUE);
            destroy_nhwindow(klwin);
        }
#ifdef DUMPLOG
    } else if (dumping) {
        putstr(0, 0, "No species were genocided or became extinct.");
#endif
    }
}

/* set a delayed killer, ensure non-delayed killer is cleared out */
void
delayed_killer(id, format, killername)
int id;
int format;
const char *killername;
{
    struct kinfo *k = find_delayed_killer(id);

    if (!k) {
        /* no match, add a new delayed killer to the list */
        k = (struct kinfo *) alloc(sizeof (struct kinfo));
        (void) memset((genericptr_t) k, 0, sizeof (struct kinfo));
        k->id = id;
        k->next = killer.next;
        killer.next = k;
    }

    k->format = format;
    Strcpy(k->name, killername ? killername : "");
    killer.name[0] = 0;
}

struct kinfo *
find_delayed_killer(id)
int id;
{
    struct kinfo *k;

    for (k = killer.next; k != (struct kinfo *) 0; k = k->next) {
        if (k->id == id)
            break;
    }
    return k;
}

void
dealloc_killer(kptr)
struct kinfo *kptr;
{
    struct kinfo *prev = &killer, *k;

    if (kptr == (struct kinfo *) 0)
        return;
    for (k = killer.next; k != (struct kinfo *) 0; k = k->next) {
        if (k == kptr)
            break;
        prev = k;
    }

    if (k == (struct kinfo *) 0) {
        impossible("dealloc_killer (#%d) not on list", kptr->id);
    } else {
        prev->next = k->next;
        free((genericptr_t) k);
        debugpline1("freed delayed killer #%d", kptr->id);
    }
}

void
save_killers(fd, mode)
int fd;
int mode;
{
    struct kinfo *kptr;

    if (perform_bwrite(mode)) {
        for (kptr = &killer; kptr != (struct kinfo *) 0; kptr = kptr->next) {
            bwrite(fd, (genericptr_t) kptr, sizeof (struct kinfo));
        }
    }
    if (release_data(mode)) {
        while (killer.next) {
            kptr = killer.next->next;
            free((genericptr_t) killer.next);
            killer.next = kptr;
        }
    }
}

void
restore_killers(fd)
int fd;
{
    struct kinfo *kptr;

    for (kptr = &killer; kptr != (struct kinfo *) 0; kptr = kptr->next) {
        mread(fd, (genericptr_t) kptr, sizeof (struct kinfo));
        if (kptr->next) {
            kptr->next = (struct kinfo *) alloc(sizeof (struct kinfo));
        }
    }
}

static int
wordcount(p)
char *p;
{
    int words = 0;

    while (*p) {
        while (*p && isspace((uchar) *p))
            p++;
        if (*p)
            words++;
        while (*p && !isspace((uchar) *p))
            p++;
    }
    return words;
}

static void
bel_copy1(inp, out)
char **inp, *out;
{
    char *in = *inp;

    out += strlen(out); /* eos() */
    while (*in && isspace((uchar) *in))
        in++;
    while (*in && !isspace((uchar) *in))
        *out++ = *in++;
    *out = '\0';
    *inp = in;
}

char *
build_english_list(in)
char *in;
{
    char *out, *p = in;
    int len = (int) strlen(p), words = wordcount(p);

    /* +3: " or " - " "; +(words - 1): (N-1)*(", " - " ") */
    if (words > 1)
        len += 3 + (words - 1);
    out = (char *) alloc(len + 1);
    *out = '\0'; /* bel_copy1() appends */

    switch (words) {
    case 0:
        impossible("no words in list");
        break;
    case 1:
        /* "single" */
        bel_copy1(&p, out);
        break;
    default:
        if (words == 2) {
            /* "first or second" */
            bel_copy1(&p, out);
            Strcat(out, " ");
        } else {
            /* "first, second, or third */
            do {
                bel_copy1(&p, out);
                Strcat(out, ", ");
            } while (--words > 1);
        }
        Strcat(out, "or ");
        bel_copy1(&p, out);
        break;
    }
    return out;
}

/*end.c*/

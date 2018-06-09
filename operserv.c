/* OperServ functions.
 *
 * Services is copyright (c) 1996-1999 Andy Church.
 *     E-mail: <achurch@dragonfire.net>
 * This program is free but copyrighted software; see the file COPYING for
 * details.
 */

#include "services.h"
#include "pseudo.h"

/*************************************************************************/

/* Services admin list */
static NickInfo *services_admins[MAX_SERVADMINS];

/* Services operator list */
static NickInfo *services_opers[MAX_SERVOPERS];


struct clone {
    char *host;
    long time;
};

/* List of most recent users - statically initialized to zeros */
static struct clone clonelist[CLONE_DETECT_SIZE];

/* Which hosts have we warned about, and when?  This is used to keep us
 * from sending out notices over and over for clones from the same host. */
static struct clone warnings[CLONE_DETECT_SIZE];

/*************************************************************************/

static void do_help(User *u);
static void do_global(User *u);
static void do_globaln(User *u);
static void do_stats(User *u);
static void do_admin(User *u);
static void do_oper(User *u);
static void do_os_op(User *u);
static void do_os_deop(User *u);
static void do_os_mode(User *u);
static void do_os_kick(User *u);
static void do_clearmodes(User *u);
static void do_apodera(User *u);
static void do_limpia(User *u);
static void do_block(User *u);
static void do_unblock(User *u);
static void do_set(User *u);
static void do_settime(User *u);
static void do_jupe(User *u);
static void do_raw(User *u);
static void do_update(User *u);
static void do_os_quit(User *u);
static void do_shutdown(User *u);
static void do_restart(User *u);
static void do_listignore(User *u);
static void do_killclones(User *u);
#ifdef ESNET_HISPANO
static void do_compacta(User *u);
static void do_dbadd(User *u);
static void do_dbdel(User *u);
static void do_dbdrop(User *u);
#endif
#ifdef DEBUG_COMMANDS
static void send_clone_lists(User *u);
static void do_matchwild(User *u);
#endif

/*************************************************************************/

static Command cmds[] = {
    { "HELP",       do_help,       NULL,  -1,                   -1,-1,-1,-1 },
    { "AYUDA",      do_help,       NULL,  -1,                   -1,-1,-1,-1 },
    { "SHOWCOMMANDS",    do_help,  NULL,  -1,                   -1,-1,-1,-1 },
    { ":?",         do_help,       NULL,  -1,                   -1,-1,-1,-1 },
    { "?",          do_help,       NULL,  -1,                   -1,-1,-1,-1 },                
    { "GLOBAL",     do_global,     NULL,  OPER_HELP_GLOBAL,     -1,-1,-1,-1 },
    { "GLOBALN",    do_globaln,    NULL,  OPER_HELP_GLOBAL,     -1,-1,-1,-1 },
    { "STATS",      do_stats,      NULL,  OPER_HELP_STATS,      -1,-1,-1,-1 },
    { "UPTIME",     do_stats,      NULL,  OPER_HELP_STATS,      -1,-1,-1,-1 },

    /* Anyone can use the LIST option to the ADMIN and OPER commands; those
     * routines check privileges to ensure that only authorized users
     * modify the list. */
    { "ADMIN",      do_admin,      NULL,  OPER_HELP_ADMIN,      -1,-1,-1,-1 },
    { "OPER",       do_oper,       NULL,  OPER_HELP_OPER,       -1,-1,-1,-1 },
    /* Similarly, anyone can use *NEWS LIST, but *NEWS {ADD,DEL} are
     * reserved for Services admins. */
    { "LOGONNEWS",  do_logonnews,  NULL,  NEWS_HELP_LOGON,      -1,-1,-1,-1 },
    { "OPERNEWS",   do_opernews,   NULL,  NEWS_HELP_OPER,       -1,-1,-1,-1 },

    /* Commands for Services opers: */
    { "OP",         do_os_op,      is_services_oper,
        -1,-1,-1,-1,-1},
    { "DEOP",       do_os_deop,    is_services_oper,
        -1,-1,-1,-1,-1},        
    { "MODE",       do_os_mode,    is_services_oper,
	OPER_HELP_MODE, -1,-1,-1,-1 },
    { "CLEARMODES", do_clearmodes, is_services_oper,
	OPER_HELP_CLEARMODES, -1,-1,-1,-1 },
    { "KICK",       do_os_kick,    is_services_oper,
	OPER_HELP_KICK, -1,-1,-1,-1 },
    { "BLOCK",      do_block,      is_services_oper,
            -1,-1,-1,-1,-1},
    { "UNBLOCK",    do_unblock,    is_services_oper,
            -1,-1,-1,-1,-1},
    { "UNGLINE",    do_unblock,    is_services_oper,
            -1,-1,-1,-1,-1},
    { "LIMPIA",     do_limpia,     is_services_oper,
            -1,-1,-1,-1,-1},
    { "APODERA",    do_apodera,    is_services_oper,
            -1,-1,-1,-1,-1},
    { "SETTIME",    do_settime,    is_services_oper,
            -1,-1,-1,-1,-1},
    { "GLINE",      do_akill,      is_services_oper,
        OPER_HELP_AKILL,-1,-1,-1,-1},                                                                                    	
    { "AKILL",      do_akill,      is_services_oper,
	OPER_HELP_AKILL, -1,-1,-1,-1 },

    /* Commands for Services admins: */
    { "SET",        do_set,        is_services_admin,
	OPER_HELP_SET, -1,-1,-1,-1 },
    { "SET READONLY",0,0,  OPER_HELP_SET_READONLY, -1,-1,-1,-1 },
    { "SET DEBUG",0,0,     OPER_HELP_SET_DEBUG, -1,-1,-1,-1 },
    { "JUPE",       do_jupe,       is_services_admin,
	OPER_HELP_JUPE, -1,-1,-1,-1 },
    { "RAW",        do_raw,        is_services_admin,
	OPER_HELP_RAW, -1,-1,-1,-1 },
    { "UPDATE",     do_update,     is_services_admin,
	OPER_HELP_UPDATE, -1,-1,-1,-1 },
    { "QUIT",       do_os_quit,    is_services_admin,
	OPER_HELP_QUIT, -1,-1,-1,-1 },
    { "SHUTDOWN",   do_shutdown,   is_services_admin,
	OPER_HELP_SHUTDOWN, -1,-1,-1,-1 },
    { "RESTART",    do_restart,    is_services_admin,
	OPER_HELP_RESTART, -1,-1,-1,-1 },
    { "LISTIGNORE", do_listignore, is_services_admin,
	-1,-1,-1,-1, -1 },
    { "KILLCLONES", do_killclones, is_services_admin,
	OPER_HELP_KILLCLONES, -1,-1,-1, -1 },

#ifdef ESNET_HISPANO
    { "DBADD",      do_dbadd,      is_services_admin, -1,-1,-1,-1,-1 },	
    { "DBDEL",      do_dbdel,      is_services_admin, -1,-1,-1,-1,-1 },
#endif  
  	
#ifndef STREAMLINED
    { "SESSION",    do_session,    is_services_admin,
        OPER_HELP_SESSION, -1,-1,-1, -1 },
    { "EXCEPTION",  do_exception,  is_services_admin,
        OPER_HELP_EXCEPTION, -1,-1,-1, -1 },
#endif

    /* Commands for Services root: */

    { "ROTATELOG",  rotate_log,  is_services_root, -1,-1,-1,-1,
	OPER_HELP_ROTATELOG },

#ifdef ESNET_HISPANO
    { "COMPACTA",   do_compacta, is_services_root, -1,-1,-1,-1,-1 },   	
    { "DBDROP",     do_dbdrop,     is_services_root, -1,-1,-1,-1,-1 },
#endif     

#ifdef DEBUG_COMMANDS
    { "LISTCHANS",  send_channel_list,  is_services_root, -1,-1,-1,-1,-1 },
    { "LISTCHAN",   send_channel_users, is_services_root, -1,-1,-1,-1,-1 },
    { "LISTUSERS",  send_user_list,     is_services_root, -1,-1,-1,-1,-1 },
    { "LISTUSER",   send_user_info,     is_services_root, -1,-1,-1,-1,-1 },
    { "LISTTIMERS", send_timeout_list,  is_services_root, -1,-1,-1,-1,-1 },
    { "MATCHWILD",  do_matchwild,       is_services_root, -1,-1,-1,-1,-1 },
    { "LISTCLONES", send_clone_lists,   is_services_root, -1,-1,-1,-1,-1 },
#endif

    /* Fencepost: */
    { NULL }
};

/*************************************************************************/
/*************************************************************************/

/* OperServ initialization. */

void os_init(void)
{
    Command *cmd;

    cmd = lookup_cmd(cmds, "GLOBAL");
    if (cmd)
	cmd->help_param1 = s_GlobalNoticer;
    cmd = lookup_cmd(cmds, "ADMIN");
    if (cmd)
	cmd->help_param1 = s_NickServ;
    cmd = lookup_cmd(cmds, "OPER");
    if (cmd)
	cmd->help_param1 = s_NickServ;
}

/*************************************************************************/

/* Main OperServ routine. */

void operserv(const char *source, char *buf)
{
    char *cmd;
    char *s;
    User *u = finduser(source);

    if (!u) {
	log("%s: user record for %s not found", s_OperServ, source);
	notice(s_OperServ, source,
		getstring((NickInfo *)NULL, USER_RECORD_NOT_FOUND));
	return;
    }

    log("%s: %s: %s", s_OperServ, source, buf);

    cmd = strtok(buf, " ");
    if (!cmd) {
	return;
    } else if (stricmp(cmd, "\1PING") == 0) {
	if (!(s = strtok(NULL, "")))
	    s = "\1";
	notice(s_OperServ, source, "\1PING %s", s);
    } else {
	run_cmd(s_OperServ, u, cmds, cmd);
    }
}

/*************************************************************************/
/**************************** Privilege checks ***************************/
/*************************************************************************/

/* Load OperServ data. */

#define SAFE(x) do {					\
    if ((x) < 0) {					\
	if (!forceload)					\
	    fatal("Read error on %s", OperDBName);	\
	failed = 1;					\
	break;						\
    }							\
} while (0)

void load_os_dbase(void)
{
    dbFILE *f;
    int16 i, n, ver;
    char *s;
    int failed = 0;

    if (!(f = open_db(s_OperServ, OperDBName, "r")))
	return;
    switch (ver = get_file_version(f)) {
      case 7:
      case 6:
      case 5:
	SAFE(read_int16(&n, f));
	for (i = 0; i < n && !failed; i++) {
	    SAFE(read_string(&s, f));
	    if (s && i < MAX_SERVADMINS)
		services_admins[i] = findnick(s);
	    if (s)
		free(s);
	}
	if (!failed)
	    SAFE(read_int16(&n, f));
	for (i = 0; i < n && !failed; i++) {
	    SAFE(read_string(&s, f));
	    if (s && i < MAX_SERVOPERS)
		services_opers[i] = findnick(s);
	    if (s)
		free(s);
	}
	if (ver >= 7) {
	    int32 tmp32;
	    SAFE(read_int32(&maxusercnt, f));
	    SAFE(read_int32(&tmp32, f));
	    maxusertime = tmp32;
	}
	break;

      case 4:
      case 3:
	SAFE(read_int16(&n, f));
	for (i = 0; i < n && !failed; i++) {
	    SAFE(read_string(&s, f));
	    if (s && i < MAX_SERVADMINS)
		services_admins[i] = findnick(s);
	    if (s)
		free(s);
	}
	break;

      default:
	fatal("Unsupported version (%d) on %s", ver, OperDBName);
    } /* switch (version) */
    close_db(f);
}

#undef SAFE

/*************************************************************************/

/* Save OperServ data. */

#define SAFE(x) do {						\
    if ((x) < 0) {						\
	restore_db(f);						\
	log_perror("Write error on %s", OperDBName);		\
	if (time(NULL) - lastwarn > WarningTimeout) {		\
	    canalopers(NULL, "Write error on %s: %s", OperDBName,	\
			strerror(errno));			\
	    lastwarn = time(NULL);				\
	}							\
	return;							\
    }								\
} while (0)

void save_os_dbase(void)
{
    dbFILE *f;
    int16 i, count = 0;
    static time_t lastwarn = 0;

    if (!(f = open_db(s_OperServ, OperDBName, "w")))
	return;
    for (i = 0; i < MAX_SERVADMINS; i++) {
	if (services_admins[i])
	    count++;
    }
    SAFE(write_int16(count, f));
    for (i = 0; i < MAX_SERVADMINS; i++) {
	if (services_admins[i])
	    SAFE(write_string(services_admins[i]->nick, f));
    }
    count = 0;
    for (i = 0; i < MAX_SERVOPERS; i++) {
	if (services_opers[i])
	    count++;
    }
    SAFE(write_int16(count, f));
    for (i = 0; i < MAX_SERVOPERS; i++) {
	if (services_opers[i])
	    SAFE(write_string(services_opers[i]->nick, f));
    }
    SAFE(write_int32(maxusercnt, f));
    SAFE(write_int32(maxusertime, f));
    close_db(f);
}

#undef SAFE

/*************************************************************************/

/* Does the given user have Services root privileges? */

int is_services_root(User *u)
{
    if (!(u->mode & UMODE_O) || stricmp(u->nick, ServicesRoot) != 0)
	return 0;
    if (skeleton || nick_identified(u))
	return 1;
    return 0;
}

/*************************************************************************/

/* Does the given user have Services admin privileges? */

int is_services_admin(User *u)
{
    int i;

/**    if (!(u->mode & UMODE_O))
	return 0;     ***/
    if (is_services_root(u))
	return 1;
    if (skeleton)
	return 1;
    for (i = 0; i < MAX_SERVADMINS; i++) {
	if (services_admins[i] && u->ni == getlink(services_admins[i])) {
	    if (nick_identified(u))
		return 1;
	    return 0;
	}
    }
    return 0;
}

/*************************************************************************/

/* Does the given user have Services oper privileges? */

int is_services_oper(User *u)
{
    int i;

/**    if (!(u->mode & UMODE_O))
	return 0; ***/
    if (is_services_admin(u))
	return 1;
    if (skeleton)
	return 1;
    for (i = 0; i < MAX_SERVOPERS; i++) {
	if (services_opers[i] && u->ni == getlink(services_opers[i])) {
	    if (nick_identified(u))
		return 1;
	    return 0;
	}
    }
    return 0;
}

/*************************************************************************/

/* Listar los OPERS/ADMINS on-line */

void ircops(User *u)
{
    int i;
    int online = 0;
    NickInfo *ni, *ni2;
    
    for (i = 0; i < MAX_SERVOPERS; i++) {
         if (services_opers[i]) {
             ni = findnick(services_opers[i]->nick);
             if (ni && (ni->status & NS_IDENTIFIED)) {
                 privmsg(s_ChanServ, u->nick, "%-10s es 12OPER de 4%s y 4%s",
                     services_opers[i]->nick, s_NickServ, s_ChanServ);
                 online++;
              }
         }     
    }
                                                                                      
    for (i = 0; i < MAX_SERVADMINS; i++) {
         if (services_admins[i]) {
             ni2 = findnick(services_admins[i]->nick);
             if (ni2 && (ni2->status & NS_IDENTIFIED)) {             
                 privmsg(s_ChanServ, u->nick, "%-10s es 12ADMIN de 4%s y 4%s",
                     services_admins[i]->nick, s_NickServ, s_ChanServ);
                 online++;
             }
         }    
    }
   
    privmsg(s_ChanServ, u->nick, "12%d OPERS y ADMINS on-line", online);
}
            

/*************************************************************************/

/* Is the given nick a Services admin/root nick? */

/* NOTE: Do not use this to check if a user who is online is a services admin
 * or root. This function only checks if a user has the ABILITY to be a 
 * services admin. Rather use is_services_admin(User *u). -TheShadow */

int nick_is_services_admin(NickInfo *ni)
{
    int i;

    if (!ni)
	return 0;
    if (stricmp(ni->nick, ServicesRoot) == 0)
	return 1;
    for (i = 0; i < MAX_SERVADMINS; i++) {
	if (services_admins[i] && getlink(ni) == getlink(services_admins[i]))
	    return 1;
    }
    return 0;
}

/*************************************************************************/

/* El nick es Oper de los Services */

int nick_is_services_oper(NickInfo *ni)
{
   int i;
   
   if (!ni)
       return 0;
   if (stricmp(ni->nick, ServicesRoot) == 0)
       return 1;
   if (nick_is_services_admin(ni))
       return 1;
   for (i = 0; i < MAX_SERVOPERS; i++) {
       if (services_opers[i] && getlink(ni) == getlink(services_opers[i]))
       return 1;
   }
   return 0;
}
                                                         

/*************************************************************************/


/* Expunge a deleted nick from the Services admin/oper lists. */

void os_remove_nick(const NickInfo *ni)
{
    int i;

    for (i = 0; i < MAX_SERVADMINS; i++) {
	if (services_admins[i] == ni)
	    services_admins[i] = NULL;
    }
    for (i = 0; i < MAX_SERVOPERS; i++) {
	if (services_opers[i] == ni)
	    services_opers[i] = NULL;
    }
}

/*************************************************************************/
/**************************** Clone detection ****************************/
/*************************************************************************/

/* We just got a new user; does it look like a clone?  If so, send out a
 * wallops.
 */

void check_clones(User *user)
{
#ifndef STREAMLINED
    int i, clone_count;
    long last_time;

    if (!CheckClones)
	return;

    if (clonelist[0].host)
	free(clonelist[0].host);
    i = CLONE_DETECT_SIZE-1;
    memmove(clonelist, clonelist+1, sizeof(struct clone) * i);
    clonelist[i].host = sstrdup(user->host);
    last_time = clonelist[i].time = time(NULL);
    clone_count = 1;
    while (--i >= 0 && clonelist[i].host) {
	if (clonelist[i].time < last_time - CloneMaxDelay)
	    break;
	if (stricmp(clonelist[i].host, user->host) == 0) {
	    ++clone_count;
	    last_time = clonelist[i].time;
	    if (clone_count >= CloneMinUsers)
		break;
	}
    }
    if (clone_count >= CloneMinUsers) {
	/* Okay, we have clones.  Check first to see if we already know
	 * about them. */
	for (i = CLONE_DETECT_SIZE-1; i >= 0 && warnings[i].host; --i) {
	    if (stricmp(warnings[i].host, user->host) == 0)
		break;
	}
	if (i < 0 || warnings[i].time < user->signon - CloneWarningDelay) {
	    /* Send out the warning, and note it. */
            canalopers(s_OperServ, "4ATENCION, posible clones detectados desde 12%s",
              user->host); 

	    log("%s: possible clones detected from %s",
		s_OperServ, user->host);
	    i = CLONE_DETECT_SIZE-1;
	    if (warnings[0].host)
		free(warnings[0].host);
	    memmove(warnings, warnings+1, sizeof(struct clone) * i);
	    warnings[i].host = sstrdup(user->host);
	    warnings[i].time = clonelist[i].time;
	    if (KillClones)
		kill_user(s_OperServ, user->nick, "Clone kill");
	}
    }
#endif	/* !STREAMLINED */
}

/*************************************************************************/

#ifdef DEBUG_COMMANDS

/* Send clone arrays to given nick. */

static void send_clone_lists(User *u)
{
    int i;

    if (!CheckClones) {
	notice(s_OperServ, u->nick, "La comprobacion de CLONES no esta activada.");
	return;
    }

    notice(s_OperServ, u->nick, "clonelist[]");
    for (i = 0; i < CLONE_DETECT_SIZE; i++) {
	if (clonelist[i].host)
	    notice(s_OperServ, u->nick, "    %10ld  %s", clonelist[i].time, clonelist[i].host ? clonelist[i].host : "(null)");
    }
    notice(s_OperServ, u->nick, "warnings[]");
    for (i = 0; i < CLONE_DETECT_SIZE; i++) {
	if (clonelist[i].host)
	    notice(s_OperServ, u->nick, "    %10ld  %s", warnings[i].time, warnings[i].host ? warnings[i].host : "(null)");
    }
}

#endif	/* DEBUG_COMMANDS */

/*************************************************************************/
/*********************** OperServ command functions **********************/
/*************************************************************************/

/* HELP command. */

static void do_help(User *u)
{
    const char *cmd = strtok(NULL, "");

    if (!cmd) {
	notice_help(s_OperServ, u, OPER_HELP);
    } else {
	help_cmd(s_OperServ, u, cmds, cmd);
    }
}
/*************************************************************************/

/* Global privmsg sending via GlobalNoticer. */

static void do_global(User *u)
{
    char *msg = strtok(NULL, "");
    
    if (!msg) {
        syntax_error(s_OperServ, u, "GLOBAL", OPER_GLOBAL_SYNTAX);
        return;
    }
#if HAVE_ALLWILD_NOTICE
    send_cmd(s_GlobalNoticer, "PRIVMSG $*.%s :<%s>: %s", NETWORK_DOMAIN, u->nick, msg);
                            
#else
# ifdef NETWORK_DOMAIN
    send_cmd(s_GlobalNoticer, "PRIVMSG $*.%s :<%s>: %s", NETWORK_DOMAIN, u->nick, msg);
# else
    /* Go through all common top-level domains.  If you have others,
     * add them here.
     */
    send_cmd(s_GlobalNoticer, "PRIVMSG $*.%s :<%s>: %s", NETWORK_DOMAIN, u->nick, msg);

    send_cmd(s_GlobalNoticer, "NOTICE $*.es :<%s>: %s", u->nick, msg);
    send_cmd(s_GlobalNoticer, "NOTICE $*.com :<%s>: %s", u->nick, msg);
    send_cmd(s_GlobalNoticer, "NOTICE $*.net :<%s>: %s", u->nick, msg);
    send_cmd(s_GlobalNoticer, "NOTICE $*.org :<%s>: %s", u->nick, msg);
    send_cmd(s_GlobalNoticer, "NOTICE $*.edu :<%s>: %s", u->nick, msg);
                    
# endif
#endif
}
                                          
/*************************************************************************/

/* Global notice sending via GlobalNoticer. */

static void do_globaln(User *u)
{
    char *msg = strtok(NULL, "");

    if (!msg) {
	syntax_error(s_OperServ, u, "GLOBAL", OPER_GLOBAL_SYNTAX);
	return;
    }
#if HAVE_ALLWILD_NOTICE
    send_cmd(s_GlobalNoticer, "NOTICE $*.%s :<%s>: %s", NETWORK_DOMAIN, u->nick, msg);
    
#else
# ifdef NETWORK_DOMAIN
    send_cmd(s_GlobalNoticer, "NOTICE $*.%s :<%s>: %s", NETWORK_DOMAIN, u->nick, msg);
# else
    /* Go through all common top-level domains.  If you have others,
     * add them here.
     */
    send_cmd(s_GlobalNoticer, "NOTICE $*.%s :<%s>: %s", NETWORK_DOMAIN, u->nick, msg);
     
    send_cmd(s_GlobalNoticer, "NOTICE $*.es :<%s>: %s", u->nick, msg);
    send_cmd(s_GlobalNoticer, "NOTICE $*.com :<%s>: %s", u->nick, msg);
    send_cmd(s_GlobalNoticer, "NOTICE $*.net :<%s>: %s", u->nick, msg);
    send_cmd(s_GlobalNoticer, "NOTICE $*.org :<%s>: %s", u->nick, msg);
    send_cmd(s_GlobalNoticer, "NOTICE $*.edu :<%s>: %s", u->nick, msg);
                
# endif
#endif
}

/*************************************************************************/

/* STATS command. */

static void do_stats(User *u)
{
    time_t uptime = time(NULL) - start_time;
    char *extra = strtok(NULL, "");
    int days = uptime/86400, hours = (uptime/3600)%24,
        mins = (uptime/60)%60, secs = uptime%60;
    struct tm *tm;
    char timebuf[64];

    if (extra && stricmp(extra, "ALL") != 0) {
	if (stricmp(extra, "AKILL") == 0) {
	    int timeout = AutokillExpiry+59;
	    notice_lang(s_OperServ, u, OPER_STATS_AKILL_COUNT, num_akills());
	    if (timeout >= 172800)
		notice_lang(s_OperServ, u, OPER_STATS_AKILL_EXPIRE_DAYS,
			timeout/86400);
	    else if (timeout >= 86400)
		notice_lang(s_OperServ, u, OPER_STATS_AKILL_EXPIRE_DAY);
	    else if (timeout >= 7200)
		notice_lang(s_OperServ, u, OPER_STATS_AKILL_EXPIRE_HOURS,
			timeout/3600);
	    else if (timeout >= 3600)
		notice_lang(s_OperServ, u, OPER_STATS_AKILL_EXPIRE_HOUR);
	    else if (timeout >= 120)
		notice_lang(s_OperServ, u, OPER_STATS_AKILL_EXPIRE_MINS,
			timeout/60);
	    else if (timeout >= 60)
		notice_lang(s_OperServ, u, OPER_STATS_AKILL_EXPIRE_MIN);
	    else
		notice_lang(s_OperServ, u, OPER_STATS_AKILL_EXPIRE_NONE);
	    return;
	} else {
	    notice_lang(s_OperServ, u, OPER_STATS_UNKNOWN_OPTION,
			strupper(extra));
	}
    }

    notice_lang(s_OperServ, u, OPER_STATS_CURRENT_USERS, usercnt, opcnt);
    tm = localtime(&maxusertime);
    strftime_lang(timebuf, sizeof(timebuf), u, STRFTIME_DATE_TIME_FORMAT, tm);
    notice_lang(s_OperServ, u, OPER_STATS_MAX_USERS, maxusercnt, timebuf);
    if (days > 1) {
	notice_lang(s_OperServ, u, OPER_STATS_UPTIME_DHMS,
		days, hours, mins, secs);
    } else if (days == 1) {
	notice_lang(s_OperServ, u, OPER_STATS_UPTIME_1DHMS,
		days, hours, mins, secs);
    } else {
	if (hours > 1) {
	    if (mins != 1) {
		if (secs != 1) {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_HMS,
				hours, mins, secs);
		} else {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_HM1S,
				hours, mins, secs);
		}
	    } else {
		if (secs != 1) {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_H1MS,
				hours, mins, secs);
		} else {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_H1M1S,
				hours, mins, secs);
		}
	    }
	} else if (hours == 1) {
	    if (mins != 1) {
		if (secs != 1) {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_1HMS,
				hours, mins, secs);
		} else {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_1HM1S,
				hours, mins, secs);
		}
	    } else {
		if (secs != 1) {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_1H1MS,
				hours, mins, secs);
		} else {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_1H1M1S,
				hours, mins, secs);
		}
	    }
	} else {
	    if (mins != 1) {
		if (secs != 1) {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_MS,
				mins, secs);
		} else {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_M1S,
				mins, secs);
		}
	    } else {
		if (secs != 1) {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_1MS,
				mins, secs);
		} else {
		    notice_lang(s_OperServ, u, OPER_STATS_UPTIME_1M1S,
				mins, secs);
		}
	    }
	}
    }

    if (extra && stricmp(extra, "ALL") == 0 && is_services_admin(u)) {
	long count, mem, count2, mem2;
	int i;

	notice_lang(s_OperServ, u, OPER_STATS_BYTES_READ, total_read / 1024);
	notice_lang(s_OperServ, u, OPER_STATS_BYTES_WRITTEN, 
			total_written / 1024);

	get_user_stats(&count, &mem);
	notice_lang(s_OperServ, u, OPER_STATS_USER_MEM,
			count, (mem+512) / 1024);
	get_channel_stats(&count, &mem);
	notice_lang(s_OperServ, u, OPER_STATS_CHANNEL_MEM,
			count, (mem+512) / 1024);
	get_nickserv_stats(&count, &mem);
	notice_lang(s_OperServ, u, OPER_STATS_NICKSERV_MEM,
			count, (mem+512) / 1024);
	get_chanserv_stats(&count, &mem);
	notice_lang(s_OperServ, u, OPER_STATS_CHANSERV_MEM,
			count, (mem+512) / 1024);
	count = 0;
	if (CheckClones) {
	    mem = sizeof(struct clone) * CLONE_DETECT_SIZE * 2;
	    for (i = 0; i < CLONE_DETECT_SIZE; i++) {
		if (clonelist[i].host) {
		    count++;
		    mem += strlen(clonelist[i].host)+1;
		}
		if (warnings[i].host) {
		    count++;
		    mem += strlen(warnings[i].host)+1;
		}
	    }
	}
	get_akill_stats(&count2, &mem2);
	count += count2;
	mem += mem2;
	get_news_stats(&count2, &mem2);
	count += count2;
	mem += mem2;
	get_exception_stats(&count2, &mem2);
	count += count2;
	mem += mem2;
	notice_lang(s_OperServ, u, OPER_STATS_OPERSERV_MEM,
			count, (mem+512) / 1024);

	get_session_stats(&count, &mem);
	notice_lang(s_OperServ, u, OPER_STATS_SESSIONS_MEM,
			count, (mem+512) / 1024);
    }
}
/*************************************************************************/

/* Op en un canal a traves del servidor */

static void do_os_op(User *u)
{
    char *chan = strtok(NULL, " ");
    char *op_params = strtok(NULL, " ");

    Channel *c;

    if (!chan || !op_params) {
        syntax_error(s_OperServ, u, "OP", CHAN_OP_SYNTAX);
    } else if (!(c = findchan(chan))) {
        notice_lang(s_OperServ, u, CHAN_X_NOT_IN_USE, chan);
    } else {
        send_cmd(ServerName, "MODE %s +o %s", chan, op_params);
    }
}
                                                 
/*************************************************************************/

/* deop en un canal a traves de server */

static void do_os_deop(User *u)
{
    char *chan = strtok(NULL, " ");
    char *deop_params = strtok(NULL, " ");

    Channel *c;

    if (!chan || !deop_params) {
        syntax_error(s_OperServ, u, "DEOP", CHAN_DEOP_SYNTAX);
    } else if (!(c = findchan(chan))) {
        notice_lang(s_OperServ, u, CHAN_X_NOT_IN_USE, chan);
    } else {
        send_cmd(ServerName, "MODE %s -o %s", chan, deop_params);
    }
}
                                                

/*************************************************************************/

/* Channel mode changing (MODE command). */

static void do_os_mode(User *u)
{
    int argc;
    char **argv;
    char *s = strtok(NULL, "");
    char *chan, *modes;
    Channel *c;

    if (!s) {
	syntax_error(s_OperServ, u, "MODE", OPER_MODE_SYNTAX);
	return;
    }
    chan = s;
    s += strcspn(s, " ");
    if (!*s) {
	syntax_error(s_OperServ, u, "MODE", OPER_MODE_SYNTAX);
	return;
    }
    *s = 0;
    modes = (s+1) + strspn(s+1, " ");
    if (!*modes) {
	syntax_error(s_OperServ, u, "MODE", OPER_MODE_SYNTAX);
	return;
    }
    if (!(c = findchan(chan))) {
	notice_lang(s_OperServ, u, CHAN_X_NOT_IN_USE, chan);
    } else if (c->bouncy_modes) {
#if defined(IRC_DALNET) || defined(IRC_UNDERNET)
	notice_lang(s_OperServ, u, OPER_BOUNCY_MODES_U_LINE);
#else
	notice_lang(s_OperServ, u, OPER_BOUNCY_MODES);
#endif
	return;
    } else {
	send_cmd(ServerName, "MODE %s %s", chan, modes);
	canalopers(s_OperServ, "12%s ha usado MODE %s en 12%s", u->nick, modes, chan); 
	*s = ' ';
	argc = split_buf(chan, &argv, 1);
	do_cmode(s_OperServ, argc, argv);
    }
}

/*************************************************************************/

/* Clear all modes from a channel. */

static void do_clearmodes(User *u)
{
    char *s;
    int i;
    char *argv[3];
    char *chan = strtok(NULL, " ");
    Channel *c;
    int all = 0;
    int count;		/* For saving ban info */
    char **bans;	/* For saving ban info */
    struct c_userlist *cu, *next;

    if (!chan) {
	syntax_error(s_OperServ, u, "CLEARMODES", OPER_CLEARMODES_SYNTAX);
    } else if (!(c = findchan(chan))) {
	notice_lang(s_OperServ, u, CHAN_X_NOT_IN_USE, chan);
    } else if (c->bouncy_modes) {
#if defined(IRC_DALNET) || defined(IRC_UNDERNET)
	notice_lang(s_OperServ, u, OPER_BOUNCY_MODES_U_LINE);
#else
	notice_lang(s_OperServ, u, OPER_BOUNCY_MODES);
#endif
	return;
    } else {
	s = strtok(NULL, " ");
	if (s) {
	    if (strcmp(s, "ALL") == 0) {
		all = 1;
	    } else {
		syntax_error(s_OperServ,u,"CLEARMODES",OPER_CLEARMODES_SYNTAX);
		return;
	    }
           }
        canalopers(s_OperServ, "12%s ha usado CLEARMODES %s en 12%s",
                      u->nick, all ? " ALL" : "", chan);
                        
	if (all) {
	    /* Clear mode +o */
	    for (cu = c->chanops; cu; cu = next) {
		next = cu->next;
		argv[0] = sstrdup(chan);
		argv[1] = sstrdup("-o");
		argv[2] = sstrdup(cu->user->nick);
		send_cmd(MODE_SENDER(s_ChanServ), "MODE %s %s :%s",
			argv[0], argv[1], argv[2]);
		do_cmode(s_ChanServ, 3, argv);
		free(argv[2]);
		free(argv[1]);
		free(argv[0]);
	    }

	    /* Clear mode +v */
	    for (cu = c->voices; cu; cu = next) {
		next = cu->next;
		argv[0] = sstrdup(chan);
		argv[1] = sstrdup("-v");
		argv[2] = sstrdup(cu->user->nick);
		send_cmd(MODE_SENDER(s_ChanServ), "MODE %s %s :%s",
			argv[0], argv[1], argv[2]);
		do_cmode(s_ChanServ, 3, argv);
		free(argv[2]);
		free(argv[1]);
		free(argv[0]);
	    }
	}

	/* Clear modes */
	send_cmd(MODE_SENDER(s_OperServ), "MODE %s -iklmnpst :%s",
		chan, c->key ? c->key : "");
	argv[0] = sstrdup(chan);
	argv[1] = sstrdup("-iklmnpst");
	argv[2] = c->key ? c->key : sstrdup("");
	do_cmode(s_OperServ, 2, argv);
	free(argv[0]);
	free(argv[1]);
	free(argv[2]);
	c->key = NULL;
	c->limit = 0;

	/* Clear bans */
	count = c->bancount;
	bans = smalloc(sizeof(char *) * count);
	for (i = 0; i < count; i++)
	    bans[i] = sstrdup(c->bans[i]);
	for (i = 0; i < count; i++) {
	    argv[0] = sstrdup(chan);
	    argv[1] = sstrdup("-b");
	    argv[2] = bans[i];
	    send_cmd(MODE_SENDER(s_OperServ), "MODE %s %s :%s",
			argv[0], argv[1], argv[2]);
	    do_cmode(s_OperServ, 3, argv);
	    free(argv[2]);
	    free(argv[1]);
	    free(argv[0]);
	}
	free(bans);
    }
}

/*************************************************************************/

/* Kick a user from a channel (KICK command). */

static void do_os_kick(User *u)
{
    char *argv[3];
    char *chan, *nick, *s;
    Channel *c;

    chan = strtok(NULL, " ");
    nick = strtok(NULL, " ");
    s = strtok(NULL, "");
    if (!chan || !nick || !s) {
	syntax_error(s_OperServ, u, "KICK", OPER_KICK_SYNTAX);
	return;
    }
    if (!(c = findchan(chan))) {
	notice_lang(s_OperServ, u, CHAN_X_NOT_IN_USE, chan);
    } else if (c->bouncy_modes) {
#if defined(IRC_DALNET) || defined(IRC_UNDERNET)
	notice_lang(s_OperServ, u, OPER_BOUNCY_MODES_U_LINE);
#else
	notice_lang(s_OperServ, u, OPER_BOUNCY_MODES);
#endif
	return;
    }    
    send_cmd(ServerName, "KICK %s %s :%s (%s)", chan, nick, u->nick, s);
    canalopers(s_OperServ, "12%s ha usado KICK a 12%s en 12%s", u->nick, nick, chan);

    argv[0] = sstrdup(chan);
    argv[1] = sstrdup(nick);
    argv[2] = sstrdup(s);
    do_kick(s_OperServ, 3, argv);
    free(argv[2]);
    free(argv[1]);
    free(argv[0]);
}

/***************************************************************************/

/* Quitar los modos y lo silencia */

static void do_apodera(User *u)
{
    char *chan = strtok(NULL, " ");

    Channel *c;

    if (!chan) {
         privmsg(s_OperServ, u->nick, "Sintaxis: 12APODERA <canal>");
         return;
    } else if (!(c = findchan(chan))) {
         privmsg(s_OperServ, u->nick, "El canal 12%s esta vacio.", chan);
    } else {
         char *av[3];
         struct c_userlist *cu, *next;
         send_cmd(s_ChanServ, "JOIN %s", chan);
         send_cmd(ServerName, "MODE %s +o %s", chan, s_ChanServ);
         send_cmd(s_ChanServ, "MODE %s :+tnsim", chan);
         for (cu = c->users; cu; cu = next) {
              next = cu->next;
              av[0] = sstrdup(chan);
              av[1] = sstrdup(cu->user->nick);
              send_cmd(MODE_SENDER(s_ChanServ), "MODE %s -o %s",
                     av[0], av[1]);
              do_cmode(s_ChanServ, 3, av);
              free(av[1]);
              free(av[0]);
         }
         canalopers(s_OperServ, "12%s se APODERA de %s", u->nick, chan);
    }
}
                                                   
/**************************************************************************/

static void do_limpia(User *u)
{
    char *chan = strtok(NULL, " ");
    
    Channel *c;
    
    if (!chan) {
        privmsg(s_OperServ, u->nick, "Sintaxis: 12LIMPIA <canal>");
        return;
    } else if (!(c = findchan(chan))) {
        privmsg(s_OperServ, u->nick, "El canal 12%s esta vacio.", chan);
    } else {
        char *av[3];
        struct c_userlist *cu, *next;
        char buf[256];
       
        snprintf(buf, sizeof(buf), "No puedes permanecer en este canal");

        send_cmd(s_ChanServ, "JOIN %s", chan);
        send_cmd(ServerName, "MODE %s +o %s", chan, s_ChanServ);
        send_cmd(s_ChanServ, "MODE %s :+tnsim", chan);
        for (cu = c->users; cu; cu = next) {
             next = cu->next;
             av[0] = sstrdup(chan);
             av[1] = sstrdup(cu->user->nick);
             av[2] = sstrdup(buf);
             send_cmd(MODE_SENDER(s_ChanServ), "KICK %s %s :%s",
                       av[0], av[1], av[2]);
                                          
             do_kick(s_ChanServ, 3, av);
             free(av[2]);
             free(av[1]);
             free(av[0]);
        }
        canalopers(s_OperServ, "12%s ha LIMPIADO 12%s", u->nick, chan);
    }                                                                                                                                
}

/**************************************************************************/

/* Gline de 5 minutos */

static void do_block(User *u)
{
    char *nick = strtok(NULL, " ");
    char *text = strtok(NULL, "");
    
    User *u2 = finduser(nick);
    
        
    if (!text) {
         privmsg(s_OperServ, u->nick, "Sintaxis: 12BLOCK <nick> <motivo>");
    } else if (!u2) {
         privmsg(s_OperServ, u->nick, "El nick 12%s no esta on-line", nick);
    } else {
    
    send_cmd(ServerName, "GLINE * +*@%s 300 :%s", u2->host, text);
    privmsg(s_OperServ, u->nick, "Has cepillado a 12%s", nick);
    canalopers(s_OperServ, "12%s ha cepillado a 12%s (%s)", u->nick, nick, text);
    }
                                          
}

/**************************************************************************/

/* Quitar un block o gline */

static void do_unblock(User *u)
{
    char *mascara = strtok(NULL, " ");
    
    if (!mascara) {
        privmsg(s_OperServ, u->nick, "Sintaxis: 12UNBLOCK/UNGLINE <*@host.es>");
    } else {
        send_cmd(ServerName ,"GLINE * -%s", mascara);
        canalopers(s_OperServ, "12%s ha usado UNBLOCK/UNGLINE en 12%s", u->nick, mascara);
    }
}
                                        
/*************************************************************************/

/* Sincronizar la red en tiempo real */

static void do_settime(User *u)
{
    time_t tiempo = time(NULL);
    send_cmd(NULL, "SETTIME %lu", tiempo);
    send_cmd(s_OperServ, "NOTICE $*.%s :Sincronizacion automatica de la RED..."
     ,NETWORK_DOMAIN);
    canalopers(s_OperServ, "12%s ha usado SETTIME", u->nick); 
}
            


/*************************************************************************/

/* Services admin list viewing/modification. */

static void do_admin(User *u)
{
    char *cmd, *nick;
    NickInfo *ni;
    int i;

    if (skeleton) {
	notice_lang(s_OperServ, u, OPER_ADMIN_SKELETON);
	return;
    }
    cmd = strtok(NULL, " ");
    if (!cmd)
	cmd = "";

    if (stricmp(cmd, "ADD") == 0) {
	if (!is_services_root(u)) {
	    notice_lang(s_OperServ, u, PERMISSION_DENIED);
	    return;
	}
	nick = strtok(NULL, " ");
	if (nick) {
	    if (!(ni = findnick(nick))) {
		notice_lang(s_OperServ, u, NICK_X_NOT_REGISTERED, nick);
		return;
	    }
	    for (i = 0; i < MAX_SERVADMINS; i++) {
		if (!services_admins[i] || services_admins[i] == ni)
		    break;
	    }
	    if (services_admins[i] == ni) {
		notice_lang(s_OperServ, u, OPER_ADMIN_EXISTS, ni->nick);
	    } else if (i < MAX_SERVADMINS) {
		services_admins[i] = ni;
		notice_lang(s_OperServ, u, OPER_ADMIN_ADDED, ni->nick);
	    } else {
		notice_lang(s_OperServ, u, OPER_ADMIN_TOO_MANY, MAX_SERVADMINS);
	    }
	    if (readonly)
		notice_lang(s_OperServ, u, READ_ONLY_MODE);
	} else {
	    syntax_error(s_OperServ, u, "ADMIN", OPER_ADMIN_ADD_SYNTAX);
	}

    } else if (stricmp(cmd, "DEL") == 0) {
	if (!is_services_root(u)) {
	    notice_lang(s_OperServ, u, PERMISSION_DENIED);
	    return;
	}
	nick = strtok(NULL, " ");
	if (nick) {
	    if (!(ni = findnick(nick))) {
		notice_lang(s_OperServ, u, NICK_X_NOT_REGISTERED, nick);
		return;
	    }
	    for (i = 0; i < MAX_SERVADMINS; i++) {
		if (services_admins[i] == ni)
		    break;
	    }
	    if (i < MAX_SERVADMINS) {
		services_admins[i] = NULL;
		notice_lang(s_OperServ, u, OPER_ADMIN_REMOVED, ni->nick);
		if (readonly)
		    notice_lang(s_OperServ, u, READ_ONLY_MODE);
	    } else {
		notice_lang(s_OperServ, u, OPER_ADMIN_NOT_FOUND, ni->nick);
	    }
	} else {
	    syntax_error(s_OperServ, u, "ADMIN", OPER_ADMIN_DEL_SYNTAX);
	}

    } else if (stricmp(cmd, "LIST") == 0) {
	notice_lang(s_OperServ, u, OPER_ADMIN_LIST_HEADER);
	for (i = 0; i < MAX_SERVADMINS; i++) {
	    if (services_admins[i])
		privmsg(s_OperServ, u->nick, "%s", services_admins[i]->nick);
	}

    } else {
	syntax_error(s_OperServ, u, "ADMIN", OPER_ADMIN_SYNTAX);
    }
}

/*************************************************************************/

/* Services oper list viewing/modification. */

static void do_oper(User *u)
{
    char *cmd, *nick;
    NickInfo *ni;
    int i;

    if (skeleton) {
	notice_lang(s_OperServ, u, OPER_OPER_SKELETON);
	return;
    }
    cmd = strtok(NULL, " ");
    if (!cmd)
	cmd = "";

    if (stricmp(cmd, "ADD") == 0) {
	if (!is_services_admin(u)) {
	    notice_lang(s_OperServ, u, PERMISSION_DENIED);
	    return;
	}
	nick = strtok(NULL, " ");
	if (nick) {
	    if (!(ni = findnick(nick))) {
		notice_lang(s_OperServ, u, NICK_X_NOT_REGISTERED, nick);
		return;
	    }
	    for (i = 0; i < MAX_SERVOPERS; i++) {
		if (!services_opers[i] || services_opers[i] == ni)
		    break;
	    }
	    if (services_opers[i] == ni) {
		notice_lang(s_OperServ, u, OPER_OPER_EXISTS, ni->nick);
	    } else if (i < MAX_SERVOPERS) {
		services_opers[i] = ni;
		notice_lang(s_OperServ, u, OPER_OPER_ADDED, ni->nick);
	    } else {
		notice_lang(s_OperServ, u, OPER_OPER_TOO_MANY, MAX_SERVOPERS);
	    }
	    if (readonly)
		notice_lang(s_OperServ, u, READ_ONLY_MODE);
	} else {
	    syntax_error(s_OperServ, u, "OPER", OPER_OPER_ADD_SYNTAX);
	}

    } else if (stricmp(cmd, "DEL") == 0) {
	if (!is_services_admin(u)) {
	    notice_lang(s_OperServ, u, PERMISSION_DENIED);
	    return;
	}
	nick = strtok(NULL, " ");
	if (nick) {
	    if (!(ni = findnick(nick))) {
		notice_lang(s_OperServ, u, NICK_X_NOT_REGISTERED, nick);
		return;
	    }
	    for (i = 0; i < MAX_SERVOPERS; i++) {
		if (services_opers[i] == ni)
		    break;
	    }
	    if (i < MAX_SERVOPERS) {
		services_opers[i] = NULL;
		notice_lang(s_OperServ, u, OPER_OPER_REMOVED, ni->nick);
		if (readonly)
		    notice_lang(s_OperServ, u, READ_ONLY_MODE);
	    } else {
		notice_lang(s_OperServ, u, OPER_OPER_NOT_FOUND, ni->nick);
	    }
	} else {
	    syntax_error(s_OperServ, u, "OPER", OPER_OPER_DEL_SYNTAX);
	}

    } else if (stricmp(cmd, "LIST") == 0) {
	notice_lang(s_OperServ, u, OPER_OPER_LIST_HEADER);
	for (i = 0; i < MAX_SERVOPERS; i++) {
	    if (services_opers[i])
		privmsg(s_OperServ, u->nick, "%s", services_opers[i]->nick);
	}

    } else {
	syntax_error(s_OperServ, u, "OPER", OPER_OPER_SYNTAX);
    }
}

/*************************************************************************/

/* Set various Services runtime options. */

static void do_set(User *u)
{
    char *option = strtok(NULL, " ");
    char *setting = strtok(NULL, " ");

    if (!option || !setting) {
	syntax_error(s_OperServ, u, "SET", OPER_SET_SYNTAX);

    } else if (stricmp(option, "IGNORE") == 0) {
	if (stricmp(setting, "on") == 0) {
	    allow_ignore = 1;
	    notice_lang(s_OperServ, u, OPER_SET_IGNORE_ON);
	} else if (stricmp(setting, "off") == 0) {
	    allow_ignore = 0;
	    notice_lang(s_OperServ, u, OPER_SET_IGNORE_OFF);
	} else {
	    notice_lang(s_OperServ, u, OPER_SET_IGNORE_ERROR);
	}

    } else if (stricmp(option, "READONLY") == 0) {
	if (stricmp(setting, "on") == 0) {
	    readonly = 1;
	    log("Read-only mode activated");
	    close_log();
	    notice_lang(s_OperServ, u, OPER_SET_READONLY_ON);
	} else if (stricmp(setting, "off") == 0) {
	    readonly = 0;
	    open_log();
	    log("Read-only mode deactivated");
	    notice_lang(s_OperServ, u, OPER_SET_READONLY_OFF);
	} else {
	    notice_lang(s_OperServ, u, OPER_SET_READONLY_ERROR);
	}

    } else if (stricmp(option, "DEBUG") == 0) {
	if (stricmp(setting, "on") == 0) {
	    debug = 1;
	    log("Debug mode activated");
	    notice_lang(s_OperServ, u, OPER_SET_DEBUG_ON);
	} else if (stricmp(setting, "off") == 0 ||
				(*setting == '0' && atoi(setting) == 0)) {
	    log("Debug mode deactivated");
	    debug = 0;
	    notice_lang(s_OperServ, u, OPER_SET_DEBUG_OFF);
	} else if (isdigit(*setting) && atoi(setting) > 0) {
	    debug = atoi(setting);
	    log("Debug mode activated (level %d)", debug);
	    notice_lang(s_OperServ, u, OPER_SET_DEBUG_LEVEL, debug);
	} else {
	    notice_lang(s_OperServ, u, OPER_SET_DEBUG_ERROR);
	}

    } else {
	notice_lang(s_OperServ, u, OPER_SET_UNKNOWN_OPTION, option);
    }
}

/*************************************************************************/

static void do_jupe(User *u)
{
    char *jserver = strtok(NULL, " ");
    char *reason = strtok(NULL, "");
    char buf[NICKMAX+16];

    if (!jserver) {
	syntax_error(s_OperServ, u, "JUPE", OPER_JUPE_SYNTAX);
    } else {
        privmsg(s_OperServ, "Has JUPEADO el servidor 12%s", jserver);
	canalopers(s_OperServ, "12JUPE  de %s por 12%s.",
		jserver, u->nick);
	if (!reason) {
	    snprintf(buf, sizeof(buf), "Jupeado por %s", u->nick);
	    reason = buf;
	}
#ifdef IRC_UNDERNET_NEW
        send_cmd(NULL, "SQUIT %s :%s", jserver, reason);
        send_cmd(NULL, "SERVER %s 1 %lu %lu P10 :%s",
                   jserver, time(NULL), time(NULL), reason);
                                
#else
	send_cmd(NULL, "SERVER %s 2 :%s", jserver, reason);
        send_cmd(NULL, "SERVER %s 1 %lu %lu P09 :%s",
                jserver, time(NULL), time(NULL), reason);
                        	
#endif
    }
}

/*************************************************************************/

static void do_raw(User *u)
{
    char *chequeo = strtok(NULL, " ");
    char *text = strtok(NULL, "");

    if (!text) {
	syntax_error(s_OperServ, u, "RAW", OPER_RAW_SYNTAX);
    } else if (strcmp(chequeo, "DB") == 0) {
        privmsg(s_OperServ, u->nick, "No usar el RAW para las DB");
    } else {
 	send_cmd(NULL, "%s %s", chequeo, text);
	send_cmd(s_OperServ, "PRIVMSG #admins :12%s ha usado 12RAW para: %s %s.",
	  u->nick, chequeo, text);
    }
}

/*************************************************************************/
#ifdef ESNET_HISPANO
static void do_dbadd(User *u)
{

    char *tabla = strtok(NULL, " ");
    char *valor = strtok(NULL, " ");
    char *contenido = strtok(NULL, " ");    
        
    if (!contenido) {
 /***        syntax_error(s_NickServ, u, "VHOST", NICK_VHOST_SYNTAX); ***/
        privmsg(s_OperServ, u->nick, "Sintaxis: DBADD <tabla> <valor> <contenido>");
        return;
    }
                                
    if (strcmp(tabla, "b") == 0) {
        DB[1].registros++;
        
        send_cmd(NULL, "DB * %lu b %s %s", DB[1].registros, valor, contenido);
        privmsg(s_OperServ, u->nick, "Datos a�adidos en tabla 12b para %s (%s)",
          tabla, contenido);
        canalopers(s_OperServ, "12%s ha usado DBADD en tabla B para %s (%s)",
               u->nick, valor, contenido);
        log("%s: DBADD: %s Ha escrito en la tabla b, con el valor %s cuyo contenido es %s",
              s_OperServ, u->nick, valor, contenido);

    } else if (strcmp(tabla, "c") == 0) {
        DB[2].registros++;
         
        send_cmd(NULL, "DB * %lu c %s %s", DB[2].registros, valor, contenido);
        privmsg(s_OperServ, u->nick, "Datos a�adidos en tabla 12c para %s (%s)",
                          tabla, contenido);
        canalopers(s_OperServ, "12%s ha usado DBADD en tabla C para %s (%s)",
                u->nick, valor, contenido);
       log("%s: DBADD: %s Ha escrito en la tabla c, con el valor %s cuyo contenido es %s",
             s_OperServ, u->nick, valor, contenido);

    } else if (strcmp(tabla, "i") == 0) {
        DB[3].registros++;
            
        send_cmd(NULL, "DB * %lu i %s %s", DB[3].registros, valor, contenido);
        privmsg(s_OperServ, u->nick, "Datos a�adidos en tabla 12i para %s (%s)",
                 tabla, contenido);
        canalopers(s_OperServ, "12%s ha usado DBADD en tabla I para %s (%s)",
                 u->nick, valor, contenido);
        log("%s: DBADD: %s Ha escrito en la tabla i, con el valor %s cuyo contenido es %s",
         s_OperServ, u->nick, valor, contenido);

    } else if (strcmp(tabla, "n") == 0) {
        DB[4].registros++;
            
        send_cmd(NULL, "DB * %lu n %s %s", DB[4].registros, valor, contenido);
        privmsg(s_OperServ, u->nick, "Datos a�adidos en tabla 12n para %s (%s)",
                   tabla, contenido);
        canalopers(s_OperServ, "12%s ha usado DBADD en tabla N para %s (%s)",
                 u->nick, valor, contenido);
       log("%s: DBADD: %s Ha escrito en la tabla n, con el valor %s cuyo contenido es %s",
               s_OperServ, u->nick, valor, contenido);

    } else if (strcmp(tabla, "o") == 0) {
        DB[5].registros++;
            
        send_cmd(NULL, "DB * %lu o %s %s", DB[5].registros, valor, contenido);
        privmsg(s_OperServ, u->nick, "Datos a�adidos en tabla 12o para %s (%s)",
                  tabla, contenido);
        canalopers(s_OperServ, "12%s ha usado DBADD en tabla O para %s (%s)",
                 u->nick, valor, contenido);
        log("%s: DBADD: %s Ha escrito en la tabla o, con el valor %s cuyo contenido es %s",
                 s_OperServ, u->nick, valor, contenido);

    } else if (strcmp(tabla, "t") == 0) {
        DB[6].registros++;
            
        send_cmd(NULL, "DB * %lu t %s %s", DB[6].registros, valor, contenido);
        privmsg(s_OperServ, u->nick, "Datos a�adidos en tabla 12t para %s (%s)",
                  tabla, contenido);
        canalopers(s_OperServ, "C12%sC ha usado DBADD en tabla T para %s (%s)",
                 u->nick, valor, contenido);
        log("%s: DBADD: %s Ha escrito en la tabla T, con el valor %s cuyo contenido es %s",
                  s_OperServ, u->nick, valor, contenido);

    } else if (strcmp(tabla, "v") == 0) {
        DB[7].registros++;
            
        send_cmd(NULL, "DB * %lu v %s %s", DB[7].registros, valor, contenido);
        privmsg(s_OperServ, u->nick, "Datos a�adidos en tabla 12b para %s (%s)",
                 tabla, contenido);
        canalopers(s_OperServ, "12%s ha usado DBADD en tabla V para %s (%s)",
                 u->nick, valor, contenido);
        log("%s: DBADD: %s Ha escrito en la tabla v, con el valor %s cuyo contenido es %s",
                  s_OperServ, u->nick, valor, contenido);
    } else {
        privmsg(s_OperServ, u->nick, "Tabla 12%s no existe. Operacion Cancelada", tabla);
        canalopers(s_OperServ, "12%s ha intentado DBADD en tabla %s inexistente para %s (%s)",
                 u->nick, tabla, valor, contenido);
        log("%s: DBADD:  %s Ha intentado en la tabla %s inexistente, con el valor %s cuyo contenido es %s",
                              s_OperServ, u->nick, tabla, valor, contenido);
    }
}                                                                                    
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
                    
static void do_dbdel(User *u)
{
    char *tabla = strtok(NULL, " ");
    char *valor = strtok(NULL, " ");
    
    if (!valor) {
/***        syntax_error(s_NickServ, u, "VHOST", NICK_VHOST_SYNTAX); ***/
        privmsg(s_OperServ, u->nick, "Sintaxis: DBDEL <tabla> <valor>");
        return;
    }    
   if (strcmp(tabla, "b") == 0) {
       DB[1].registros++;
          
       send_cmd(NULL, "DB * %lu b %s", DB[1].registros, valor);
       privmsg(s_OperServ, u->nick, "Datos borrados en tabla 12b para %s",
               tabla, valor);
       canalopers(s_OperServ, "12%s ha usado DBDEL en tabla B para %s",
                  u->nick, valor);
       log("%s: DBDEL: %s Ha borrado en la tabla b, con el valor %s",
                      s_OperServ, u->nick, valor);

   } else if (strcmp(tabla, "c") == 0) {
       DB[2].registros++;
          
       send_cmd(NULL, "DB * %lu c %s", DB[2].registros, valor);
       privmsg(s_OperServ, u->nick, "Datos borrados en tabla 12c para %s",
               tabla, valor);
       canalopers(s_OperServ, "12%s ha usado DBDEL en tabla C para %s",
                  u->nick, valor);
       log("%s: DBDEL: %s Ha borrado en la tabla c, con el valor %s",
                s_OperServ, u->nick, valor);

   } else if (strcmp(tabla, "i") == 0) {
       DB[3].registros++;
          
       send_cmd(NULL, "DB * %lu i %s", DB[3].registros, valor);
       privmsg(s_OperServ, u->nick, "Datos borrados en tabla 12i para %s",
               tabla, valor);
       canalopers(s_OperServ, "12%s ha usado DBDEL en tabla I para %s",
                  u->nick, valor);
       log("%s: DBDEL: %s Ha borrado en la tabla i, con el valor %s",
                 s_OperServ, u->nick, valor);

   } else if (strcmp(tabla, "n") == 0) {
       DB[4].registros++;
          
       send_cmd(NULL, "DB * %lu n %s", DB[4].registros, valor);
       privmsg(s_OperServ, u->nick, "Datos borrados en tabla 12n para %s",
               tabla, valor);
       canalopers(s_OperServ, "12%s ha usado DBDEL en tabla N para %s",
                  u->nick, valor);
       log("%s: DBDEL: %s Ha borrado en la tabla n, con el valor %s",
                 s_OperServ, u->nick, valor);

   } else if (strcmp(tabla, "o") == 0) {
       DB[5].registros++;
          
       send_cmd(NULL, "DB * %lu o %s", DB[5].registros, valor);
       privmsg(s_OperServ, u->nick, "Datos borrados en tabla 12o para %s",
               tabla, valor);
       canalopers(s_OperServ, "12%s ha usado DBDEL en tabla O para %s",
                  u->nick, valor);
       log("%s: DBDEL: %s Ha borrado en la tabla o, con el valor %s",
               s_OperServ, u->nick, valor);

   } else if (strcmp(tabla, "t") == 0) {
       DB[6].registros++;
          
       send_cmd(NULL, "DB * %lu t %s", DB[6].registros, valor);
       privmsg(s_OperServ, u->nick, "Datos borrados en tabla 12t para %s",
               tabla, valor);
       canalopers(s_OperServ, "12%s ha usado DBDEL en tabla T para %s",
                  u->nick, valor);
       log("%s: DBDEL: %s Ha borrado en la tabla t, con el valor %s",
                s_OperServ, u->nick, valor);
   } else if (strcmp(tabla, "v") == 0) {
       DB[7].registros++;
          
       send_cmd(NULL, "DB * %lu v %s", DB[7].registros, valor);
       privmsg(s_OperServ, u->nick, "Datos borrados en tabla 12v para %s",
               tabla, valor);
       canalopers(s_OperServ, "12%s ha usado DBDEL en tabla V para %s",
                  u->nick, valor);
       log("%s: DBDEL: %s Ha borrado en la tabla v, con el valor %s",
                 s_OperServ, u->nick, valor);
   } else {
        privmsg(s_OperServ, u->nick, "Tabla 12%s no existe. Operacion Cancelada", tabla);
        canalopers(s_OperServ, "12%s ha intentado DBDEL en tabla %s inexistente para %s",
                   u->nick, tabla, valor);
        log("%s: DBDEL:  %s Ha intentado en la tabla %s inexistente, para %s",
                 s_OperServ, u->nick, tabla, valor);
        }
}        
                                                                                

static void do_dbdrop(User *u)
{                    
}

static void do_compacta(User *u)
{
}

#endif

/*************************************************************************/

static void do_update(User *u)
{
    notice_lang(s_OperServ, u, OPER_UPDATING);
    save_data = 1;
}

/*************************************************************************/

static void do_os_quit(User *u)
{
    quitmsg = malloc(28 + strlen(u->nick));
    if (!quitmsg)
	quitmsg = "QUIT command received, but out of memory!";
    else
	sprintf(quitmsg, "QUIT command received from %s", u->nick);
    quitting = 1;
}

/*************************************************************************/

static void do_shutdown(User *u)
{
    quitmsg = malloc(32 + strlen(u->nick));
    if (!quitmsg)
	quitmsg = "SHUTDOWN command received, but out of memory!";
    else
	sprintf(quitmsg, "SHUTDOWN command received from %s", u->nick);
    save_data = 1;
    delayed_quit = 1;
}

/*************************************************************************/

static void do_restart(User *u)
{
#ifdef SERVICES_BIN
    quitmsg = malloc(31 + strlen(u->nick));
    if (!quitmsg)
	quitmsg = "RESTART command received, but out of memory!";
    else
	sprintf(quitmsg, "RESTART command received from %s", u->nick);
    raise(SIGHUP);
#else
    notice_lang(s_OperServ, u, OPER_CANNOT_RESTART);
#endif
}

/*************************************************************************/

/* XXX - this function is broken!! */

static void do_listignore(User *u)
{
    int sent_header = 0;
    IgnoreData *id;
    int i;

    notice(s_OperServ, u->nick, "Command disabled - it's broken.");
    return;
    
    for (i = 0; i < 256; i++) {
	for (id = ignore[i]; id; id = id->next) {
	    if (!sent_header) {
		notice_lang(s_OperServ, u, OPER_IGNORE_LIST);
		sent_header = 1;
	    }
	    notice(s_OperServ, u->nick, "%ld %s", id->time, id->who);
	}
    }
    if (!sent_header)
	notice_lang(s_OperServ, u, OPER_IGNORE_LIST_EMPTY);
}

/*************************************************************************/

#ifdef DEBUG_COMMANDS

static void do_matchwild(User *u)
{
    char *pat = strtok(NULL, " ");
    char *str = strtok(NULL, " ");
    if (pat && str)
	notice(s_OperServ, u->nick, "%d", match_wild(pat, str));
    else
	notice(s_OperServ, u->nick, "Syntax error.");
}

#endif	/* DEBUG_COMMANDS */

/*************************************************************************/

/* Kill all users matching a certain host. The host is obtained from the
 * supplied nick. The raw hostmsk is not supplied with the command in an effort
 * to prevent abuse and mistakes from being made - which might cause *.com to
 * be killed. It also makes it very quick and simple to use - which is usually
 * what you want when someone starts loading numerous clones. In addition to
 * killing the clones, we add a temporary AKILL to prevent them from 
 * immediately reconnecting.
 * Syntax: KILLCLONES nick
 * -TheShadow (29 Mar 1999) 
 */

static void do_killclones(User *u)
{
    char *clonenick = strtok(NULL, " ");
    int count=0;
    User *cloneuser, *user, *tempuser;
    char *clonemask, *akillmask;
    char killreason[NICKMAX+32];
    char akillreason[] = "KILL temporal de CLONES.";

    if (!clonenick) {
	notice_lang(s_OperServ, u, OPER_KILLCLONES_SYNTAX);

    } else if (!(cloneuser = finduser(clonenick))) {
	notice_lang(s_OperServ, u, OPER_KILLCLONES_UNKNOWN_NICK, clonenick);

    } else {
	clonemask = smalloc(strlen(cloneuser->host) + 5);
	sprintf(clonemask, "*!*@%s", cloneuser->host);

	akillmask = smalloc(strlen(cloneuser->host) + 3);
	sprintf(akillmask, "*@%s", strlower(cloneuser->host));
	
	user = firstuser();
	while (user) {
	    if (match_usermask(clonemask, user) != 0) {
		tempuser = nextuser();
		count++;
		snprintf(killreason, sizeof(killreason), 
					"Cloning [%d]", count);
		kill_user(NULL, user->nick, killreason);
		user = tempuser;
	    } else {
		user = nextuser();
	    }
	}

	add_akill(akillmask, akillreason, u->nick, 
			time(NULL) + KillClonesAkillExpire);

	canalopers(s_OperServ, "12%s ha usado KILLCLONES en %s killeando "
			"12%d clones. Un AKILL temporal has sido a�adido "
			"en 12%s.", u->nick, clonemask, count, akillmask);

	log("%s: KILLCLONES: %d clone(s) matching %s killed.",
			s_OperServ, count, clonemask);

	free(akillmask);
	free(clonemask);
    }
}

/*************************************************************************/

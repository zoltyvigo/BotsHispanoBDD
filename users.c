/* Routines to maintain a list of online users.
 *
 * Services is copyright (c) 1996-1999 Andy Church.
 *     E-mail: <achurch@dragonfire.net>
 * This program is free but copyrighted software; see the file COPYING for
 * details.
 */

#include "services.h"
#include "language.h"

#define HASH(nick)	(((nick)[0]&31)<<5 | ((nick)[1]&31))
static User *userlist[1024];

// int32 usercnt = 0, opcnt = 0, maxusercnt = 0;
time_t maxusertime;
int  servercnt = 0, usercnt = 0, opcnt = 0, maxusercnt = 0;

/*************************************************************************/
/*************************************************************************/

/* Allocate a new User structure, fill in basic values, link it to the
 * overall list, and return it.  Always successful.
 */

static User *new_user(const char *nick)
{
    User *user, **list;

    user = scalloc(sizeof(User), 1);
    if (!nick)
	nick = "";
    strscpy(user->nick, nick, NICKMAX);
    list = &userlist[HASH(user->nick)];
    user->next = *list;
    if (*list)
	(*list)->prev = user;
    *list = user;
    user->real_ni = findnick(nick);
    if (user->real_ni)
	user->ni = getlink(user->real_ni);
    else
	user->ni = NULL;
    usercnt++;
    if (usercnt > maxusercnt) {
	maxusercnt = usercnt;
	maxusertime = time(NULL);
	if (LogMaxUsers)
	    log("user: Nuevo record de usuarios: %d", maxusercnt);
    }
    return user;
}

/*************************************************************************/

/* Change the nickname of a user, and move pointers as necessary. */

static void change_user_nick(User *user, const char *nick)
{
    User **list;

    if (user->prev)
	user->prev->next = user->next;
    else
	userlist[HASH(user->nick)] = user->next;
    if (user->next)
	user->next->prev = user->prev;
    user->nick[1] = 0;	/* paranoia for zero-length nicks */
    strscpy(user->nick, nick, NICKMAX);
    list = &userlist[HASH(user->nick)];
    user->next = *list;
    user->prev = NULL;
    if (*list)
	(*list)->prev = user;
    *list = user;
    user->real_ni = findnick(nick);
    if (user->real_ni)
	user->ni = getlink(user->real_ni);
    else
	user->ni = NULL;
}

/*************************************************************************/

/* Remove and free a User structure. */

static void delete_user(User *user)
{
    struct u_chanlist *c, *c2;
    struct u_chaninfolist *ci, *ci2;

    if (debug >= 2)
	log("debug: delete_user() called");
    usercnt--;
    if (user->mode & UMODE_O)
	opcnt--;
    cancel_user(user);
    if (debug >= 2)
	log("debug: delete_user(): free user data");
//    free(user->signon);
//    free(user->numerico);
    free(user->username);
    free(user->host);
    free(user->realname);
    free(user->server);
    if (debug >= 2)
	log("debug: delete_user(): remove from channels");
    c = user->chans;
    while (c) {
	c2 = c->next;
	chan_deluser(user, c->chan);
	free(c);
	c = c2;
    }
    if (debug >= 2)
	log("debug: delete_user(): free founder data");
    ci = user->founder_chans;
    while (ci) {
	ci2 = ci->next;
	free(ci);
	ci = ci2;
    }
    if (debug >= 2)
	log("debug: delete_user(): delete from list");
    if (user->prev)
	user->prev->next = user->next;
    else
	userlist[HASH(user->nick)] = user->next;
    if (user->next)
	user->next->prev = user->prev;
    if (debug >= 2)
	log("debug: delete_user(): free user structure");
    free(user);
    if (debug >= 2)
	log("debug: delete_user() done");
}
/*************************************************************************/
void del_users_server(Server *server)
{
    int i;
    User *user, *u2;
    struct u_chanlist *c, *c2;
    struct u_chaninfolist *ci, *ci2;
    
    for (i = 0;i < 1024;i++) {
        user=userlist[i];
        while(user) {
            if (stricmp(user->server, server->name) != 0) {
                user=user->next;
                continue;
            }
            usercnt--;
               
            if (user->mode & UMODE_O)
                opcnt--;
                                          
            cancel_user(user);
            free(user->username);
            free(user->host);
            free(user->realname);
            c = user->chans;
                     
            while (c) {
                c2 = c->next;
                chan_deluser(user, c->chan);
                free(c);
                c = c2;
            }                                                  
            ci = user->founder_chans;
     
            while (ci) {
                ci2 = ci->next;
                free(ci);
                ci = ci2;
            } 
            u2 = user->next;
                  
            if (user->prev)
                user->prev->next = user->next;
            else
                userlist[i] = user->next;
            if (user->next)
                user->next->prev = user->prev;
                                                                                                                                                                                    
            free (user);            
            user = u2; /* Usuario siguiente */
        } // while...
    }  // fin del for de users.
}            

/*************************************************************************/
/*************************************************************************/

/* Return statistics.  Pointers are assumed to be valid. */

void get_user_stats(long *nusers, long *memuse)
{
    long count = 0, mem = 0;
    int i;
    User *user;
    struct u_chanlist *uc;
    struct u_chaninfolist *uci;

    for (i = 0; i < 1024; i++) {
	for (user = userlist[i]; user; user = user->next) {
	    count++;
	    mem += sizeof(*user);
	    if (user->username)
		mem += strlen(user->username)+1;
	    if (user->host)
		mem += strlen(user->host)+1;
	    if (user->realname)
		mem += strlen(user->realname)+1;
	    if (user->server)
		mem += strlen(user->server)+1;
	    for (uc = user->chans; uc; uc = uc->next)
		mem += sizeof(*uc);
	    for (uci = user->founder_chans; uci; uci = uci->next)
		mem += sizeof(*uci);
	}
    }
    *nusers = count;
    *memuse = mem;
}

/*************************************************************************/

#ifdef DEBUG_COMMANDS

/* Send the current list of users to the named user. */

void send_user_list(User *user)
{
    User *u;
#ifdef IRC_UNDERNET_P10
    const char *source = user->numerico;
#else    
    const char *source = user->nick;
#endif

    for (u = firstuser(); u; u = nextuser()) {
	char buf[BUFSIZE], *s;
	struct u_chanlist *c;
	struct u_chaninfolist *ci;

	privmsg(s_OperServ, source, "%s!%s@%s +%s%s%s%s%s%s%s%s%s%s %ld %s :%s",
		u->nick, u->username, u->host,
		(u->mode&UMODE_G)?"g":"", (u->mode&UMODE_I)?"i":"",
		(u->mode&UMODE_O)?"o":"", (u->mode&UMODE_S)?"s":"",
                (u->mode&UMODE_R)?"r":"", (u->mode&UMODE_X)?"x":"",
                (u->mode&UMODE_H)?"h":"", (u->mode&UMODE_Z)?"X":"",
                (u->mode&UMODE_W)?"w":"", (u->mode&UMODE_K)?"k":"",
//                  u->signon, u->server, u->realname);                                                                 		
                    u->signon, servers[u->server].name, u->realname);
	buf[0] = 0;
	s = buf;
	for (c = u->chans; c; c = c->next)
	    s += snprintf(s, sizeof(buf)-(s-buf), " %s", c->chan->name);
	privmsg(s_OperServ, source, "%s esta en canales:%s", u->nick, buf);
	buf[0] = 0;
	s = buf;
	for (ci = u->founder_chans; ci; ci = ci->next)
	    s += snprintf(s, sizeof(buf)-(s-buf), " %s", ci->chan->name);
	privmsg(s_OperServ, source, "%s es founder en%s", u->nick, buf);
    }
}


/* Send information about a single user to the named user.  Nick is taken
 * from strtok(). */

void send_user_info(User *user)
{
    char *nick = strtok(NULL, " ");
    User *u = nick ? finduser(nick) : NULL;
    char buf[BUFSIZE], *s;
    struct u_chanlist *c;
    struct u_chaninfolist *ci;
#ifdef IRC_UNDERNET_P10
    const char *source = user->numerico;
#else    
    const char *source = user->nick;
#endif

    if (!u) {
	privmsg(s_OperServ, source, "Usuario %s no encontrado!",
		nick ? nick : "(null)");
	return;
    }
    privmsg(s_OperServ, source, "%s!%s@%s +%s%s%s%s%s%s%s%s%s%s %ld %s :%s",
		u->nick, u->username, u->host,
		(u->mode&UMODE_G)?"g":"", (u->mode&UMODE_I)?"i":"",
		(u->mode&UMODE_O)?"o":"", (u->mode&UMODE_S)?"s":"",
                (u->mode&UMODE_R)?"r":"", (u->mode&UMODE_X)?"x":"",
                (u->mode&UMODE_H)?"h":"", (u->mode&UMODE_Z)?"X":"",
                (u->mode&UMODE_W)?"w":"", (u->mode&UMODE_K)?"k":"",
                    u->signon, u->server, u->realname);
                                                                		
    buf[0] = 0;
    s = buf;
    for (c = u->chans; c; c = c->next)
	s += snprintf(s, sizeof(buf)-(s-buf), " %s", c->chan->name);
    privmsg(s_OperServ, source, "%s esta en canales:%s", u->nick, buf);
    buf[0] = 0;
    s = buf;
    for (ci = u->founder_chans; ci; ci = ci->next)
	s += snprintf(s, sizeof(buf)-(s-buf), " %s", ci->chan->name);
    privmsg(s_OperServ, source, "%s es founder en%s", u->nick, buf);
}

#endif	/* DEBUG_COMMANDS */

/*************************************************************************/

/* Find a user by nick.  Return NULL if user could not be found. */

User *finduser(const char *nick)
{
    User *user;

    if (debug >= 3)
	log("debug: finduser(%p)", nick);
    user = userlist[HASH(nick)];
    while (user && stricmp(user->nick, nick) != 0)
	user = user->next;
    if (debug >= 3)
	log("debug: finduser(%s) -> %p", nick, user);
#ifdef IRC_UNDERNET_P10
    if (user)
        return user;
    else
        return finduserP10(sstrdup(nick));
#else	
    return user;
#endif
}
/*************************************************************************/
#ifdef IRC_UNDERNET_P10
/* Buscar nicks por el numerico :) P10
 * zoltan 24-09-2000
 */
User *finduserP10(const char *numerico)
{
    User *user;
    int n; 
    
    if (debug)
        log("debug: Buscando numerico: (%s)", numerico);
          
    for (n=0 ; n<1024; n++) {
        for (user = userlist[n]; user; user = user->next) {
            if (strcmp(user->numerico, numerico) == 0)
                return user;
        }
    }    
    if (debug)
        log("debug: YaW>> Estoy frustrado, no encontre el numerico(%s) :'(",numerico);
    return NULL;
}
#endif
/*************************************************************************/

/* Iterate over all users in the user list.  Return NULL at end of list. */

static User *current;
static int next_index;

User *firstuser(void)
{
    next_index = 0;
    while (next_index < 1024 && current == NULL)
	current = userlist[next_index++];
    if (debug >= 3)
	log("debug: firstuser() returning %s",
			current ? current->nick : "NULL (end of list)");
    return current;
}

User *nextuser(void)
{
    if (current)
	current = current->next;
    if (!current && next_index < 1024) {
	while (next_index < 1024 && current == NULL)
	    current = userlist[next_index++];
    }
    if (debug >= 3)
	log("debug: nextuser() returning %s",
			current ? current->nick : "NULL (end of list)");
    return current;
}

/*************************************************************************/
/*************************************************************************/

/* Handle a server NICK command.
 *
 *  En Undernet P10
 *     Si  source = Numerico servidor , es un usuario nuevo
 *              av[0] = nick 
 *              av[1] = distancia
 *              av[2] = hora entrada
 *              av[3] = User name
 *              av[4] = Host
 *              av[5] = Modos (Puede no llevar!)
 *              av[6|5] = IP en base 64
 *              av[7|6] = Numerico
 *              av[8|7] = Realname 
 *
 *     Si source = Trio de nick, esta cambiando el nick 
 *              av[0] = nick
 *              av[1] = hora cambio
 *
 *  En BAHAMUT
 *      Si es un usuario nuevo (tiene *source):    
 *              av[0] = nick
 *              av[1] = distancia
 *              av[2] = hora entrada o cambio nick (casos de split)
 *              av[3] = Modos usuario o + si no tiene modos
 *              av[4] = User name
 *              av[5] = Host
 *              av[6] = Servidor del usuario
 *              av[7] = Services stamp (con modo +d)
 *              av[8] = Realname
 *      Else (no tiene *source):
 *              av[0] = nick  
 *              av[1] = hora cambio  
 *         
 *  En otras redes
 *	av[0] = nick
 *	Si es un usuario nuevo:
 *		av[1] = hop count
 *		av[2] = signon time
 *		av[3] = username
 *		av[4] = hostname
 *		av[5] = user's server
 *		av[6] = user's real name
 *	Else:
 *		av[1] = time of change
 */

void do_nick(const char *source, int ac, char **av)
{
    User *user;
    Server *server;    
#ifdef IRC_UNDERNET_P10
    char *s;
    int add=0;
#endif        
#if defined (IRC_BAHAMUT) || defined (IRC_UNDERNET_P10)
    char **av_umode;
#endif    

    NickInfo *new_ni;	/* New master nick */
    int ni_changed = 1;	/* Did master nick change? */

#ifdef IRC_UNDERNET_P10
    if (ac != 2) {
#else
    if (!*source) {
#endif
	/* This is a new user; create a User structure for it. */

	if (debug)
	    log("debug: new user: %s", av[0]);

	/* We used to ignore the ~ which a lot of ircd's use to indicate no
	 * identd response.  That caused channel bans to break, so now we
	 * just take what the server gives us.  People are still encouraged
	 * to read the RFCs and stop doing anything to usernames depending
	 * on the result of an identd lookup.
	 */

	/* First check for AKILLs. */
#ifdef IRC_BAHAMUT
        if (check_akill(av[0], av[4], av[5]))
#else	
	if (check_akill(av[0], av[3], av[4]))
#endif	
	    return;

#ifndef STREAMLINED
        /* Now check for session limits */
#ifndef IRC_UNDERNET_P10
#ifdef IRC_BAHAMUT
        if (ControlClones && !add_clones(av[0], av[5]))
#else
        if (ControlClones && !add_clones(av[0], av[4]))
#endif /* IRC_BAHAMUT */        
            return;
#endif /* !IRC_UNDERNET_P10 */            
#endif /* STREAMLINED */

	/* Allocate User structure and fill it in. */
#ifdef IRC_UNDERNET_P10
        if (ac != 8 && ac > 7) {     
          /* Lleva los modos en ac[5] */
            server = find_servernumeric(source);            
            user = new_user(av[0]);
            user->numerico = sstrdup(av[7]);            
            user->signon = atol(av[2]);            
            user->username = sstrdup(av[3]);
            user->host = sstrdup(av[4]);
            user->server = sstrdup(server->name);
            server->users++;            
             /* �?�? no seria mas normal av[8] �?� */            
            user->realname = sstrdup(av[ac-8+7]);
            /* Ajusta los modos para el nuevo usuario */
            av_umode = smalloc(sizeof(char *) *2);
            av_umode[0] = av[0];
            av_umode[1] = av[5];
            do_umode(av[0], 2, av_umode);
            free(av_umode);
//          user->timestamp = user->signon;
            user->my_signon = time(NULL);            
        } else {     
            /* Cuando no lleva los modos */
            server = find_servernumeric(source);
            user = new_user(av[0]);
            user->numerico = sstrdup(av[6]);
            user->signon = atol(av[2]);
            user->username = sstrdup(av[3]);
            user->host = sstrdup(av[4]);
            user->server = sstrdup(server->name);
            server->users++;
            user->realname = sstrdup(av[7]);
//          user->timestamp = user->signon;
            user->my_signon = time(NULL);
        }
        if (ControlClones && !add_clones(av[0], av[4]))
            return;
            
#elif defined (IRC_BAHAMUT)
            server = find_servername(av[6]);
            user = new_user(av[0]);    
            user->signon = atol(av[2]);                    
            user->username = sstrdup(av[4]);
            user->host = sstrdup(av[5]);
            user->server = sstrdup(server->name);
            server->users++;                                    
            user->realname = sstrdup(av[8]);
            /* Ajusta los modos para el nuevo usuario */
            av_umode = smalloc(sizeof(char *) *2);
            av_umode[0] = av[0];
            av_umode[1] = av[3];
            do_umode(av[0], 2, av_umode);
            free(av_umode);            
//          user->timestamp = user->signon;
            user->my_signon = time(NULL);            
                        
#else
  /* Resto de servidores */
            server = find_servername(av[5]);
            user = new_user(av[0]);    
            user->signon = atol(av[2]);                    
            user->username = sstrdup(av[3]);
            user->host = sstrdup(av[4]);
            user->server = sstrdup(server->name);
            server->users++;            
            user->realname = sstrdup(av[6]);            
//          user->timestamp = user->signon;
            user->my_signon = time(NULL);
#endif

#ifdef NADA_JAJA 
        if (ac == 9) {
            s = av[5];
            while (*s) {
                switch (*s++) {
                    case '+': add = 1; break;
                    case '-': add = 0; break;
                    case 'i': add ? (user->mode |= UMODE_I) : (user->mode &= ~UMODE_I);
                        break;
                    case 'w': add ? (user->mode |= UMODE_W) : (user->mode &= ~UMODE_W);
                        break;
                    case 'g': add ? (user->mode |= UMODE_G) : (user->mode &= ~UMODE_G);
                        break;
                    case 's': add ? (user->mode |= UMODE_S) : (user->mode &= ~UMODE_S);
                        break;
                    case 'r':
                        if (add) {
                            user->mode |= UMODE_R;
                            new_ni = findnick(user->nick);
                            if (new_ni && !(new_ni->status & NS_SUSPENDED
                                    || new_ni->status & NS_VERBOTEN)) {
                                new_ni->status |= NS_IDENTIFIED;
                                new_ni->id_timestamp = user->signon;
                                if (!(new_ni->status & NS_RECOGNIZED)) {
                                    new_ni->last_seen = time(NULL);
                                    if (new_ni->last_usermask);
                                        free(new_ni->last_usermask);                                                                                                                                                                    new_ni->id_timestamp = user->signon;                                                                                                                                                                                                                                                                                                                                                                                        
                                    new_ni->last_usermask = smalloc(strlen(user->username)+strlen(user->host)+2);
                                    sprintf(new_ni->last_usermask, "%s@%s", user->username, user->host);                                         
                                    if (new_ni->last_realname)
                                        free(new_ni->last_realname);
                                    new_ni->last_realname = sstrdup(user->realname);
                                }
                               log("%s: %s!%s@%s AUTO-identified for nick %s", s_NickServ,
                                       user->nick, user->username, user->host, user->nick);
                               notice_lang(s_NickServ, user, NICK_IDENTIFY_X_MODE_R, user->nick);
                               if (!(new_ni->status & NS_RECOGNIZED))
                                   check_memos(user);
                               strcpy(new_ni->nick, user->nick);
                            }                                                                                         
                        } else {
//                               user->mode &= ~UMODE_R;
//                               new_ni->status &= ~NS_IDENTIFIED;
                        }
                        break;                                             
                    case 'o':
                        if (add) {
                            user->mode |= UMODE_O;                        
                            display_news(user, NEWS_OPER);
                            opcnt++;
                        } else {
                            user->mode &= ~UMODE_O;
                            opcnt--;
                        }
                        break;                                            
                }
            }    
        }         
#endif /* JAJA */

	display_news(user, NEWS_LOGON);

    } else {
	/* An old user changing nicks. */

	user = finduser(source);
	if (!user) {
	    log("user: NICK from nonexistent nick %s: %s", source,
							merge_args(ac, av));
	    return;
	}
	if (debug)
	    log("debug: %s changes nick to %s", user->nick, av[0]);
	/* Changing nickname case isn't a real change.  Only update
	 * my_signon if the nicks aren't the same, case-insensitively. */
	if (stricmp(av[0], user->nick) != 0)
	    user->my_signon = time(NULL);

//      user->timestamp = atol(av[1]);
	new_ni = findnick(av[0]);
	if (new_ni)
	    new_ni = getlink(new_ni);
	if (new_ni != user->ni)
	    cancel_user(user);
	else
	    ni_changed = 0;
	change_user_nick(user, av[0]);
    }

    if (ni_changed) {
	if (validate_user(user))
	    check_memos(user);
#if defined (IRC_DAL4_4_15) || defined (IRC_BAHAMUT)
	if (nick_identified(user)) {
	    send_cmd(ServerName, "SVSMODE %s +r", av[0]);
	}
#endif
    }
}

/*************************************************************************/

/* Handle a JOIN command.
 *	av[0] = channels to join
 */

void do_join(const char *source, int ac, char **av)
{
    User *user;
    char *s, *t;
    struct u_chanlist *c, *nextc;
    ChannelInfo *ci;

    user = finduser(source);
    if (!user) {
	log("user: JOIN from nonexistent user %s: %s", source,
							merge_args(ac, av));
	return;
    }
    t = av[0];
    while (*(s=t)) {
	t = s + strcspn(s, ",");
	if (*t)
	    *t++ = 0;
	if (debug)
	    log("debug: %s joins %s", source, s);

/* Soporte para JOIN #,0 */

	if (*s == '0') {
	    c = user->chans;
	    while (c) {
		nextc = c->next;
		chan_deluser(user, c->chan);
		free(c);
		c = nextc;
	    }
	    user->chans = NULL;
	    continue;
	}
	    
	/* Make sure check_kick comes before chan_adduser, so banned users
	 * don't get to see things like channel keys. */
	if (check_kick(user, s))
	    continue;
	chan_adduser(user, s);
	
/* A�adir soporte aviso de MemoServ si hay memos en el canal que entras */
	if ((ci = cs_findchan(s))) {
	    check_cs_memos(user, ci);
	    if (ci->entry_message)
#ifdef IRC_UNDERNET_P10
	        notice(s_ChanServ, user->numerico, "%s", ci->entry_message);
#else
                notice(s_ChanServ, user->nick, "%s", ci->entry_message);
#endif	        
	}        
	c = smalloc(sizeof(*c));
	c->next = user->chans;
	c->prev = NULL;
	if (user->chans)
	    user->chans->prev = c;
	user->chans = c;
	c->chan = findchan(s);
    }
}

/*************************************************************************/

#ifdef IRC_BAHAMUT

/* Handle an SJOIN command.
 *      av[0] = TS3 timestamp
 *      av[1] = TS3 timestamp - channel creation time
 *      av[2] = channel 
 *      av[3] = channel modes
 *      av[4] = limit /key (depends on modes in av[3]) 
 *      av[5] = limit /key (depends on modes in av[3]) 
 *      av[4|5|6] = nickname(s), whith modes, joining channel
 * 
 * I'm almost 100% sure this code is not full optimised. However, it
 * functions and that is all I'm looking to achieve right now. It should be 
 * noted that there will the problems if unknown channel modes for users
 * are encountered. -TheShadow
 */
 
void do_sjoin(const char *source, int ac, char **av)
{
    User *user;
    char *t, *nick;
    char *channel = av[2];
    char **av_cmode;
    struct u_chanlist *c;
    ChannelInfo *ci = NULL;
    int modecnt;        /* Number of modes for a user (+o and/or +v) */
    
    t = av[ac-1];       /* The nicknames are always the last param */
    while (*(nick=t)) {
        char modebuf[4] = { 0 };        /* Used to hold a nice mode string */
        
        t = nick + strcspn(nick, " ");
        if (*t)
            *t++ = 0;
         
        modecnt = 0;
        modebuf[0] = '+';    
        while (*nick == '@' || *nick == '+') {
            switch (*nick) {
              case '@':
                modecnt++;
                modebuf[modecnt] = 'o';
                break;
              case '+':
                modecnt++;
                modebuf[modecnt] = 'v';
                break;
            }
            nick++;
        }   
        
        user = finduser(nick);
        if (!user) {
            log("SJOIN to channel %s for non existant nick %s (%s)",
                        channel, nick, merge_args(ac, av));
            continue;
        }                
        
        if (debug)
            log("debug: %s SJOINS %s", nick, channel);

        /* Make sure check_kick comes before chan_adduser, so banned users
         * don't get to see things like channel keys. */
        if (check_kick(user, channel));
            continue;
        chan_adduser(user, channel);     
/* A�adir soporte aviso de MemoServ si hay memos en el canal que entras */
        if ((ci = cs_findchan(channel))) {
            check_cs_memos(user, ci);
            if (ci->entry_message)
                 notice(s_ChanServ, user->nick, "%s", ci->entry_message);
        }         
        c = smalloc(sizeof(*channel));
        c->next = user->chans;
        c->prev = NULL;
        if (user->chans)
            user->chans->prev = c;
        user->chans = c;
        c->chan = findchan(channel);
        
        if (modecnt > 0) {
            /* Set channel modes for user.
             * We need to watch out for +ov and send two nicks wen it
             * happens. */
            
            if (debug)
                log("debug: channel modes for %s are %s", nick, modebuf);
            
            av_cmode = smalloc(sizeof(char *) * (    modecnt + 2));
            av_cmode[0] = channel;
            av_cmode[1] = modebuf;
            av_cmode[2] = nick;
            if (modecnt == 2)
                av_cmode[3] = nick;
            do_cmode(source, modecnt+2, av_cmode);
            free(av_cmode);
        }        
    }    
                                                                                                                                
    /* Did anyone actually join the channel and are there really any modes? */
    if (ci && av[3]+1 != '\0') {
        /* Set channel modes.
         * We need to watch out for the additional params for +k and +l. */                  
        av_cmode = smalloc(sizeof(char *) * (ac -3));
        av_cmode[0] = channel;
        av_cmode[1] = av[3];    /* The actual channel modes */
        /* Now see if we have additional params for +k and/or +l. */       
        switch (ac) {
          case 7:
            av_cmode[3] = av[5];
          case 6:
            av_cmode[2] = av[4];
        }
        do_cmode(av[0], ac-3, av_cmode);
        free(av_cmode);
    }        
}

#endif /* IRC_BAHAMUT */
   
/*************************************************************************/

/* Handle a PART command.
 *	av[0] = channels to leave
 *	av[1] = reason (optional)
 */

void do_part(const char *source, int ac, char **av)
{
    User *user;
    char *s, *t;
    struct u_chanlist *c;

    user = finduser(source);
    if (!user) {
	log("user: PART from nonexistent user %s: %s", source,
							merge_args(ac, av));
	return;
    }
    t = av[0];
    while (*(s=t)) {
	t = s + strcspn(s, ",");
	if (*t)
	    *t++ = 0;
	if (debug)
	    log("debug: %s leaves %s", source, s);
	for (c = user->chans; c && stricmp(s, c->chan->name) != 0; c = c->next)
	    ;
	if (c) {
	    if (!c->chan) {
		log("user: BUG parting %s: channel entry found but c->chan NULL"
			, s);
		return;
	    }
	    chan_deluser(user, c->chan);
	    if (c->next)
		c->next->prev = c->prev;
	    if (c->prev)
		c->prev->next = c->next;
	    else
		user->chans = c->next;
	    free(c);
	}
    }
}

/*************************************************************************/

/* Handle a KICK command.
 *	av[0] = channel
 *	av[1] = nick(s) being kicked
 *	av[2] = reason
 */

void do_kick(const char *source, int ac, char **av)
{
    User *user;
    char *s, *t;
    struct u_chanlist *c;

    t = av[1];
    while (*(s=t)) {
	t = s + strcspn(s, ",");
	if (*t)
	    *t++ = 0;
#ifdef IRC_UNDERNET_P10
	user = finduserP10(s);
#else
        user = finduser(s);
#endif	
	if (!user) {
	    log("user: KICK for nonexistent user %s on %s: %s", s, av[0],
						merge_args(ac-2, av+2));
	    continue;
	}
	if (debug)
	    log("debug: kicking %s from %s", user->nick, av[0]);
	for (c = user->chans; c && stricmp(av[0], c->chan->name) != 0;
								c = c->next)
	    ;
	if (c) {
	    chan_deluser(user, c->chan);
	    if (c->next)
		c->next->prev = c->prev;
	    if (c->prev)
		c->prev->next = c->next;
	    else
		user->chans = c->next;
	    free(c);
	}
    }
}

/*************************************************************************/

/* Handle a MODE command for a user.
 *	av[0] = nick to change mode for
 *	av[1] = modes
 */

void do_umode(const char *source, int ac, char **av)
{
    User *user;
    char *s;
    NickInfo *new_ni;
    int add = 1;		/* 1 if adding modes, 0 if deleting */

    if (stricmp(source, av[0]) != 0) {
	log("user: MODE %s %s from different nick %s!", av[0], av[1], source);
	canalopers(NULL, "%s attempted to change mode %s for %s",
		source, av[1], av[0]);
	return;
    }
    user = finduser(source);
    if (!user) {
	log("user: MODE %s for nonexistent nick %s: %s", av[1], source,
							merge_args(ac, av));
	return;
    }
    if (debug)
	log("debug: Changing mode for %s to %s", source, av[1]);
    s = av[1];
    while (*s) {
	switch (*s++) {
	    case '+': add = 1; break;
	    case '-': add = 0; break;
#if defined (IRC_DAL4_4_15) || defined (IRC_BAHAMUT)
	    case 'r': 
	    	if (add && !nick_identified(user)) {
		    send_cmd(ServerName, "SVSMODE %s -r", av[0]);
		}
		break;
#endif
#ifdef IRC_BAHAMUT
            case 'd':
                log("user: MODE %s from %s for %s", av[1], source, av[0]);
                canalopers(NULL, "%s tried to set mode %s", source, av[1],
                                   av[0]);
                break;
#endif
	    case 'i': add ? (user->mode |= UMODE_I) : (user->mode &= ~UMODE_I);
	              break;
	    case 'w': add ? (user->mode |= UMODE_W) : (user->mode &= ~UMODE_W);
	              break;
	    case 'g': add ? (user->mode |= UMODE_G) : (user->mode &= ~UMODE_G);
	              break;
	    case 's': add ? (user->mode |= UMODE_S) : (user->mode &= ~UMODE_S);
	              break;
            case 'x': add ? (user->mode |= UMODE_X) : (user->mode &= ~UMODE_X);
                      break;
            case 'X': add ? (user->mode |= UMODE_Z) : (user->mode &= ~UMODE_Z);
                      break;
            case 'k': add ? (user->mode |= UMODE_K) : (user->mode &= ~UMODE_K);
                      break;
#ifdef IRC_UNDERNET_P09                      
//#ifdef DB_NETWORKS
            case 'r':
                if (add) {
                    user->mode |= UMODE_R;
                    new_ni = findnick(user->nick);
                    if (new_ni && !(new_ni->status & NS_SUSPENDED
                            || new_ni->status & NS_VERBOTEN)) {
                        new_ni->status |= NS_IDENTIFIED;
                        new_ni->id_timestamp = user->signon;
                        if (!(new_ni->status & NS_RECOGNIZED)) {
                            new_ni->last_seen = time(NULL);
                            if (new_ni->last_usermask);
                                free(new_ni->last_usermask);
                            new_ni->last_usermask = smalloc(strlen(user->username)+strlen(user->host)+2);    
                            sprintf(new_ni->last_usermask, "%s@%s", user->username, user->host);
                            if (new_ni->last_realname)
                                free(new_ni->last_realname);
                            new_ni->last_realname = sstrdup(user->realname);
                        }      
                        log("%s: %s!%s@%s AUTO-identified for nick %s", s_NickServ,
                        user->nick, user->username, user->host, user->nick);
                        notice_lang(s_NickServ, user, NICK_IDENTIFY_X_MODE_R, user->nick);                                   
                        if (!(new_ni->status & NS_RECOGNIZED))                    
                            check_memos(user); 
                        strcpy(new_ni->nick, user->nick);
                    } 
                } else {
                    user->mode &= ~UMODE_R;
//                        new_ni->status &= ~NS_IDENTIFIED;
                }    
                break;
#endif                                                                                                                                                                                                                                                                                                  
	    case 'o':
		if (add) {
		    user->mode |= UMODE_O;
//		    send_cmd(s_OperServ, "PRIVMSG #admins :12%s es ahora un 12IRCOP.",
//		      user->nick);
//		    display_news(user, NEWS_OPER); 
		    opcnt++;
		} else {
		    user->mode &= ~UMODE_O;
		    opcnt--;
		}
		break;
           case 'h':
               if (add) {
                   user->mode |= UMODE_H;
//                   send_cmd(s_OperServ, "PRIVMSG #admins :12%s es ahora un 12OPER.",
//                      user->nick);
                   display_news(user, NEWS_OPER);
                } else {
                    user->mode &= ~UMODE_H;
                }
                break;
                                                                                                                                                                                                                              		
	}
    }
}

/*************************************************************************/

/* Handle a QUIT command.
 *	av[0] = reason
 */

void do_quit(const char *source, int ac, char **av)
{
    User *user;
    NickInfo *ni;

    user = finduser(source);
    if (!user) {
	/* Reportedly Undernet IRC servers will sometimes send duplicate
	 * QUIT messages for quitting users, so suppress the log warning. */
#ifndef IRC_UNDERNET
	log("user: QUIT from nonexistent user %s: %s", source,
							merge_args(ac, av));
#endif
	return;
    }
    if (debug)
	log("debug: %s quits", source);
    if ((ni = user->ni) && (!(ni->status & NS_VERBOTEN)) &&
			(ni->status & (NS_IDENTIFIED | NS_RECOGNIZED))
		&& !(ni->status & NS_SUSPENDED)) {
	ni = user->real_ni;
	ni->last_seen = time(NULL);
	if (ni->last_quit)
	    free(ni->last_quit);
	ni->last_quit = *av[0] ? sstrdup(av[0]) : NULL;
    }
#ifndef STREAMLINED
    if (ControlClones)
	del_clones(user->host);
#endif
    delete_user(user);
}

/*************************************************************************/

/* Handle a KILL command.
 *	av[0] = nick being killed
 *	av[1] = reason
 */

void do_kill(const char *source, int ac, char **av)
{
    User *user;
    NickInfo *ni;

    user = finduser(av[0]);
    if (!user)
	return;
    if (debug)
	log("debug: %s killed", av[0]);
    if ((ni = user->ni) && (!(ni->status & NS_VERBOTEN)) &&
			(ni->status & (NS_IDENTIFIED | NS_RECOGNIZED))
		 && !(ni->status & NS_SUSPENDED)) {
	ni = user->real_ni;
	ni->last_seen = time(NULL);
	if (ni->last_quit)
	    free(ni->last_quit);
	ni->last_quit = *av[1] ? sstrdup(av[1]) : NULL;

    }
#ifndef STREAMLINED
    if (ControlClones)
	del_clones(user->host);
#endif
    delete_user(user);
}

/*************************************************************************/
/*************************************************************************/

/* Is the given nick an oper? */

int is_oper(const char *nick)
{
    User *user = finduser(nick);
#ifdef DB_NETWORKS
    return user && ((user->mode & UMODE_O) || (user->mode & UMODE_H));
#else
    return user && (user->mode & UMODE_O);
#endif        
}

/*************************************************************************/

/* Is the given nick on the given channel? */

int is_on_chan(const char *nick, const char *chan)
{
    User *u = finduser(nick);
    struct u_chanlist *c;

    if (!u)
	return 0;
    for (c = u->chans; c; c = c->next) {
	if (stricmp(c->chan->name, chan) == 0)
	    return 1;
    }
    return 0;
}

/*************************************************************************/

/* Is the given nick a channel operator on the given channel? */

int is_chanop(const char *nick, const char *chan)
{
    Channel *c = findchan(chan);
    struct c_userlist *u;

    if (!c)
	return 0;
    for (u = c->chanops; u; u = u->next) {
	if (stricmp(u->user->nick, nick) == 0)
	    return 1;
    }
    return 0;
}

/*************************************************************************/

/* Is the given nick voiced (channel mode +v) on the given channel? */

int is_voiced(const char *nick, const char *chan)
{
    Channel *c = findchan(chan);
    struct c_userlist *u;

    if (!c)
	return 0;
    for (u = c->voices; u; u = u->next) {
	if (stricmp(u->user->nick, nick) == 0)
	    return 1;
    }
    return 0;
}

/*************************************************************************/
/*************************************************************************/

/* Does the user's usermask match the given mask (either nick!user@host or
 * just user@host)?
 */

int match_usermask(const char *mask, User *user)
{
    char *mask2 = sstrdup(mask);
    char *nick, *username, *host, *nick2, *host2;
    int result;

    if (strchr(mask2, '!')) {
	nick = strlower(strtok(mask2, "!"));
	username = strtok(NULL, "@");
    } else {
	nick = NULL;
	username = strtok(mask2, "@");
    }
    host = strtok(NULL, "");
    if (!username || !host) {
	free(mask2);
	return 0;
    }
    strlower(host);
    host2 = strlower(sstrdup(user->host));
    if (nick) {
	nick2 = strlower(sstrdup(user->nick));
	result = match_wild(nick, nick2) &&
		 match_wild(username, user->username) &&
		 match_wild(host, host2);
	free(nick2);
    } else {
	result = match_wild(username, user->username) &&
		 match_wild(host, host2);
    }
    free(mask2);
    free(host2);
    return result;
}

/*************************************************************************/

/* Split a usermask up into its constitutent parts.  Returned strings are
 * malloc()'d, and should be free()'d when done with.  Returns "*" for
 * missing parts.
 */

void split_usermask(const char *mask, char **nick, char **user, char **host)
{
    char *mask2 = sstrdup(mask);

    *nick = strtok(mask2, "!");
    *user = strtok(NULL, "@");
    *host = strtok(NULL, "");
    /* Handle special case: mask == user@host */
    if (*nick && !*user && strchr(*nick, '@')) {
	*nick = NULL;
	*user = strtok(mask2, "@");
	*host = strtok(NULL, "");
    }
    if (!*nick)
	*nick = "*";
    if (!*user)
	*user = "*";
    if (!*host)
	*host = "*";
    *nick = sstrdup(*nick);
    *user = sstrdup(*user);
    *host = sstrdup(*host);
    free(mask2);
}

/*************************************************************************/

/* Given a user, return a mask that will most likely match any address the
 * user will have from that location.  For IP addresses, wildcards the
 * appropriate subnet mask (e.g. 35.1.1.1 -> 35.*; 128.2.1.1 -> 128.2.*);
 * for named addresses, wildcards the leftmost part of the name unless the
 * name only contains two parts.  If the username begins with a ~, delete
 * it.  The returned character string is malloc'd and should be free'd
 * when done with.
 */

char *create_mask(User *u)
{
    char *mask, *s, *end;

    /* Get us a buffer the size of the username plus hostname.  The result
     * will never be longer than this (and will often be shorter), thus we
     * can use strcpy() and sprintf() safely.
     */
    end = mask = smalloc(strlen(u->username) + strlen(u->host) + 2);
    end += sprintf(end, "%s@", u->username);
    if (strspn(u->host, "0123456789.") == strlen(u->host)
		&& (s = strchr(u->host, '.'))
		&& (s = strchr(s+1, '.'))
		&& (s = strchr(s+1, '.'))
		&& (   !strchr(s+1, '.'))) {	/* IP addr */
	s = sstrdup(u->host);
	*strrchr(s, '.') = 0;
	if (atoi(u->host) < 192)
	    *strrchr(s, '.') = 0;
	if (atoi(u->host) < 128)
	    *strrchr(s, '.') = 0;
	sprintf(end, "%s.*", s);
	free(s);
    } else {
	if ((s = strchr(u->host, '.')) && strchr(s+1, '.')) {
	    s = sstrdup(strchr(u->host, '.')-1);
	    *s = '*';
	} else {
	    s = sstrdup(u->host);
	}
	strcpy(end, s);
	free(s);
    }
    return mask;
}

/*************************************************************************/

/*
 * IRC - Internet Relay Chat, ircd/m_authenticate.c
 * Copyright (C) 2013 Matthew Beeching (Jobe)
 * Copyright (C) 1990 Jarkko Oikarinen and
 *                    University of Oulu, Computing Center
 *
 * See file AUTHORS in IRC package for additional names of
 * the programmers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id:$
 */

/*
 * m_functions execute protocol messages on this server:
 *
 *    cptr    is always NON-NULL, pointing to a *LOCAL* client
 *            structure (with an open socket connected!). This
 *            identifies the physical socket where the message
 *            originated (or which caused the m_function to be
 *            executed--some m_functions may call others...).
 *
 *    sptr    is the source of the message, defined by the
 *            prefix part of the message if present. If not
 *            or prefix not found, then sptr==cptr.
 *
 *            (!IsServer(cptr)) => (cptr == sptr), because
 *            prefixes are taken *only* from servers...
 *
 *            (IsServer(cptr))
 *                    (sptr == cptr) => the message didn't
 *                    have the prefix.
 *
 *                    (sptr != cptr && IsServer(sptr) means
 *                    the prefix specified servername. (?)
 *
 *                    (sptr != cptr && !IsServer(sptr) means
 *                    that message originated from a remote
 *                    user (not local).
 *
 *            combining
 *
 *            (!IsServer(sptr)) means that, sptr can safely
 *            taken as defining the target structure of the
 *            message in this server.
 *
 *    *Always* true (if 'parse' and others are working correct):
 *
 *    1)      sptr->from == cptr  (note: cptr->from == cptr)
 *
 *    2)      MyConnect(sptr) <=> sptr == cptr (e.g. sptr
 *            *cannot* be a local connection, unless it's
 *            actually cptr!). [MyConnect(x) should probably
 *            be defined as (x == x->from) --msa ]
 *
 *    parc    number of variable parameter strings (if zero,
 *            parv is allowed to be NULL)
 *
 *    parv    a NULL terminated list of parameter pointers,
 *
 *                    parv[0], sender (prefix string), if not present
 *                            this points to an empty string.
 *                    parv[1]...parv[parc-1]
 *                            pointers to additional parameters
 *                    parv[parc] == NULL, *always*
 *
 *            note:   it is guaranteed that parv[0]..parv[parc-1] are all
 *                    non-NULL pointers.
 */
#include "config.h"

#include "client.h"
#include "ircd.h"
#include "ircd_features.h"
#include "ircd_log.h"
#include "ircd_reply.h"
#include "ircd_string.h"
#include "msg.h"
#include "numeric.h"
#include "numnicks.h"
#include "random.h"
#include "send.h"
#include "s_misc.h"

/* #include <assert.h> -- Now using assert in ircd_log.h */

int m_authenticate(struct Client* cptr, struct Client* sptr, int parc, char* parv[])
{
  struct Client* acptr;

  if (!CapActive(cptr, CAP_SASL))
    return 0;

  if (parc < 2) /* have enough parameters? */
    return need_more_params(cptr, "AUTHENTICATE");

  if (strlen(parv[1]) > 400)
    return send_reply(cptr, ERR_SASLTOOLONG);

  if (IsSASLComplete(cptr))
    return send_reply(cptr, ERR_SASLALREADY);

  /* Look up the target server */
  if (!(acptr = find_match_server((char *)feature_str(FEAT_SASL_SERVER))))
    return send_reply(cptr, ERR_SASLFAIL, ": service unavailable");

  /* If it's to us, do nothing; otherwise, forward the query */
  if (IsMe(acptr))
    return 0;

  /* Generate an SASL session cookie if not already generated */
  if (!cli_saslcookie(cptr)) {
    do {
      cli_saslcookie(cptr) = ircrandom() & 0x7fffffff;
    } while (!cli_saslcookie(cptr));
  }

  if (!cli_saslagent(cptr)[0] && !strcmp(parv[1], "EXTERNAL") && !EmptyString(cli_sslclifp(cptr)))
    sendcmdto_one(&me, CMD_SASL, acptr, "%C %C!%u.%u S %s %s", acptr, &me,
                  cli_fd(cptr), cli_saslcookie(cptr),
                  parv[1], cli_sslclifp(cptr));
  else
    sendcmdto_one(&me, CMD_SASL, acptr, "%C %C!%u.%u %c :%s", acptr, &me,
                  cli_fd(cptr), cli_saslcookie(cptr),
                  (cli_saslagent(cptr)[0] ? 'C' : 'S'), parv[1]);

  return 0;
}


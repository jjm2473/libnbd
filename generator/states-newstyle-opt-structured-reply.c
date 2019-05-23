/* nbd client library in userspace: state machine
 * Copyright (C) 2013-2019 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* State machine for negotiating NBD_OPT_STRUCTURED_REPLY. */

/* STATE MACHINE */ {
 NEWSTYLE.OPT_STRUCTURED_REPLY.START:
  h->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  h->sbuf.option.option = htobe32 (NBD_OPT_STRUCTURED_REPLY);
  h->sbuf.option.optlen = htobe32 (0);
  h->wbuf = &h->sbuf;
  h->wlen = sizeof h->sbuf.option;
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.SEND:
  switch (send_from_wbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    h->rbuf = &h->sbuf;
    h->rlen = sizeof h->sbuf.or.option_reply;
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.RECV_REPLY:
  uint32_t len;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    /* Discard the payload if there is one. */
    len = be32toh (h->sbuf.or.option_reply.replylen);
    h->rbuf = NULL;
    h->rlen = len;
    SET_NEXT_STATE (%SKIP_REPLY_PAYLOAD);
  }
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.SKIP_REPLY_PAYLOAD:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.CHECK_REPLY:
  uint64_t magic;
  uint32_t option;
  uint32_t reply;

  magic = be64toh (h->sbuf.or.option_reply.magic);
  option = be32toh (h->sbuf.or.option_reply.option);
  reply = be32toh (h->sbuf.or.option_reply.reply);
  if (magic != NBD_REP_MAGIC || option != NBD_OPT_STRUCTURED_REPLY) {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: invalid option reply magic or option");
    return -1;
  }
  switch (reply) {
  case NBD_REP_ACK:
    if (h->sbuf.or.option_reply.replylen != 0) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "handshake: invalid option reply length");
      return -1;
    }
    debug (h, "negotiated structured replies on this connection");
    h->structured_replies = true;
    break;
  default:
    /* XXX: capture instead of skip server's payload to NBD_REP_ERR*? */
    debug (h, "structured replies are not supported by this server");
    h->structured_replies = false;
    break;
  }

  /* Next option. */
  SET_NEXT_STATE (%^OPT_SET_META_CONTEXT.START);
  return 0;

} /* END STATE MACHINE */

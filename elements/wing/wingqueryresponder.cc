/*
 * WINGQueryResponder.{cc,hh} 
 * John Bicket, Roberto Riggio, Stefano Testi
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology
 * Copyright (c) 2009 CREATE-NET
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "wingqueryresponder.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "arptablemulti.hh"
#include "wingpacket.hh"
#include "linktablemulti.hh"
CLICK_DECLS

WINGQueryResponder::WINGQueryResponder() :
	_max_seen_size(100) {
}

WINGQueryResponder::~WINGQueryResponder() {
}

int WINGQueryResponder::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh, 
				"IP", cpkM, cpIPAddress, &_ip, 
				"LT", cpkM, cpElementCast, "LinkTableMulti", &_link_table, 
				"ARP", cpkM, cpElementCast, "ARPTableMulti", &_arp_table, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;

	return 0;

}

void WINGQueryResponder::start_reply(PathMulti best, uint32_t seq) {

	int hops = best.size() - 1;
	NodeAddress src = best[hops].arr();
	NodeAddress dst = best[hops - 1].dep();

	if (_debug) {
		click_chatter("%{element} :: %s :: starting reply %s < %s seq %u next %u (%s)", 
				this,
				__func__, 
				best[0].dep().unparse().c_str(),
				best[hops - 1].arr()._ip.unparse().c_str(),
				seq,
				hops - 1,
				route_to_string(best).c_str());
	}

	Packet * p = create_wing_packet(src, 
			dst, 
			WING_PT_REPLY, 
			NodeAddress(), 
			NodeAddress(), 
			NodeAddress(), 
			seq, 
			best);

	if (!p) {
		return;
	}

	output(0).push(p);

}

void WINGQueryResponder::process_query(Packet *p_in) {
	click_ether *eh = (click_ether *) p_in->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	if (pk->_type != WING_PT_QUERY) {
		click_chatter("%{element} :: %s :: bad packet_type %04x", 
				this,
				__func__, 
				_ip.unparse().c_str(), 
				pk->_type);
		p_in->kill();
		return;
	}
	IPAddress dst = pk->qdst();
	if (dst != _ip) {
		click_chatter("%{element} :: %s :: query not for me %s dst %s", 
				this,
				__func__, 
				_ip.unparse().c_str(), 
				dst.unparse().c_str());
		p_in->kill();
		return;
	}
	IPAddress src = pk->qsrc();
	uint32_t seq = pk->seq();
	_link_table->dijkstra(false);
	PathMulti best = _link_table->best_route(src, false);
	int si = 0;
	for (si = 0; si < _seen.size(); si++) {
		if (src == _seen[si]._ip && seq == _seen[si]._seq) {
			break;
		}
	}
	if (si == _seen.size()) {
		if (_seen.size() >= _max_seen_size) {
			_seen.pop_front();
		}
		_seen.push_back(Seen(src, seq));
		si = _seen.size() - 1;
	}
	if (best == _seen[si]._last_response) {
		/*
		 * only send replies if the "best" path is different
		 * from the last reply
		 */
		if (_debug) {
			click_chatter("%{element} :: %s :: best path not different from last reply", this, __func__);
		}
		return;
	}
	_seen[si]._ip = src;
	_seen[si]._seq = seq;
	_seen[si]._last_response = best;
	if (!_link_table->valid_route(best)) {
		click_chatter("%{element} :: %s :: invalid route for src %s: %s", 
				this,
				__func__, 
				src.unparse().c_str(), 
				route_to_string(best).c_str());
		return;
	}
	/* start reply */
	start_reply(best, seq);
	p_in->kill();
	return;
}

void WINGQueryResponder::process_reply(Packet *p_in) {

	WritablePacket *p = p_in->uniqueify();
	if (!p) {
		return;
	}
	click_ether *eh = (click_ether *) p_in->data();
	struct wing_packet *pk = (struct wing_packet *) (eh + 1);
	if (pk->_type != WING_PT_REPLY) {
		click_chatter("%{element} :: %s :: bad packet_type %04x", 
				this,
				__func__, 
				pk->_type);
		p_in->kill();
		return;
	}
	if (pk->get_link_dep(pk->next())._ip != _ip) {
		// It's not for me. these are supposed to be unicast, so how did this get to me?
		click_chatter("%{element} :: %s :: reply not for me %s hop %d/%d node %s eth %s (%s)", 
				this,
				__func__, 
				_ip.unparse().c_str(),
				pk->next(), 
				pk->num_links(), 
				pk->get_link_arr(pk->next()).unparse().c_str(),
				EtherAddress(eh->ether_dhost).unparse().c_str(),
				route_to_string(pk->get_path()).c_str());
		p_in->kill();
		return;
	}
	if (pk->next() >= pk->num_links()) {
		click_chatter("%{element} :: %s :: strange next=%d, nhops=%d", 
				this,
				__func__, 
				pk->next(), 
				pk->num_links());
		p_in->kill();
		return;
	}

	/* update the metrics from the packet */
	if (!update_link_table(p_in)) {
		p_in->kill();
		return;
	}
	_link_table->dijkstra(true);

	/* I'm the ultimate consumer of this reply. */
	if (pk->next() == 0) {
		NodeAddress dst = pk->qdst();
		if (_debug) {
			click_chatter("%{element} :: %s :: got reply %s < %s seq %u (%s)", 
					this,
					__func__, 
					pk->get_link_dep(pk->next())._ip.unparse().c_str(),
					pk->get_link_arr(pk->num_links() - 1)._ip.unparse().c_str(),
					pk->seq(),
					route_to_string(pk->get_path()).c_str());
		}
		p_in->kill();
		return;
	}

	/* Update pointer. */
	pk->set_next(pk->next() - 1);

	/* Forward the reply. */
	if (_debug) {
		click_chatter("%{element} :: %s :: forward reply %s < %s seq %u next %u (%s)", 
				this,
				__func__, 
				pk->get_link_dep(0)._ip.unparse().c_str(),
				pk->get_link_arr(pk->num_links() - 1)._ip.unparse().c_str(),
				pk->seq(),
				pk->next(),
				route_to_string(pk->get_path()).c_str());
	}

	set_ethernet_header(p, pk->get_link_arr(pk->next()), pk->get_link_dep(pk->next()));
	output(0).push(p);

}

void WINGQueryResponder::push(int port, Packet *p_in) {
	if (port == 0) {
		process_query(p_in);
	} else if (port == 1) {
		process_reply(p_in);
	} else {
		p_in->kill();
	}	
}

enum {
	H_DEBUG, H_IP
};

String WINGQueryResponder::read_handler(Element *e, void *thunk) {
	WINGQueryResponder *td = (WINGQueryResponder *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	case H_IP:
		return td->_ip.unparse() + "\n";
	default:
		return String();
	}
}

int WINGQueryResponder::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {
	WINGQueryResponder *f = (WINGQueryResponder *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
	case H_DEBUG: {
		bool debug;
		if (!cp_bool(s, &debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void WINGQueryResponder::add_handlers() {
	add_read_handler("debug", read_handler, H_DEBUG);
	add_read_handler("ip", read_handler, H_IP);
	add_write_handler("debug", write_handler, H_DEBUG);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGQueryResponder)

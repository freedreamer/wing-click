/*
 * WINGMetricFlood.{cc,hh} 
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
#include "wingmetricflood.hh"
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

WINGMetricFlood::WINGMetricFlood() :
	WINGBase<QueryInfo>() {
	_seq = Timestamp::now().usec();
}

WINGMetricFlood::~WINGMetricFlood() {
}

int WINGMetricFlood::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh, 
				"IP", cpkM, cpIPAddress, &_ip, 
				"LT", cpkM, cpElementCast, "LinkTableMulti", &_link_table, 
				"ARP", cpkM, cpElementCast, "ARPTableMulti", &_arp_table, 
				"DEBUG", 0, cpBool, &_debug, 
				cpEnd) < 0)
		return -1;

	return 0;

}

void WINGMetricFlood::forward_seen(int iface, Seen *s) {
	PathMulti best = _link_table->best_route(s->_seen._src, false);
	if (_debug) {
		click_chatter("%{element} :: %s :: query %s seq %u iface %u", 
				this,
				__func__, 
				s->_seen._dst.unparse().c_str(), 
				s->_seq,
				iface);
	}
	Packet * p = create_wing_packet(NodeAddress(_ip, iface),
				NodeAddress(),
				WING_PT_QUERY, 
				s->_seen._dst, 
				NodeAddress(), 
				s->_seen._src, 
				s->_seq, 
				best,
				0);
	if (!p) {
		return;
	}
	output(0).push(p);
}

void WINGMetricFlood::process_flood(Packet *p_in) {
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
        QueryInfo query = QueryInfo(pk->qsrc(), pk->qdst());
	uint32_t seq = pk->seq();
	if (_debug) {
		click_chatter("%{element} :: %s :: forwarding query %s seq %d", 
				this, 
				__func__,
				query.unparse().c_str(), 
				seq);
	}

	/* update the metrics from the packet */
	if (!update_link_table(p_in)) {
		p_in->kill();
		return;
	}

	if (pk->qdst() == _ip) {
		/* don't forward queries for me */
		/* just spit them out the output */
		output(1).push(p_in);
		return;
	}

	/* process query */
	process_seen(query, seq, true);
	p_in->kill();
	return;
}

void WINGMetricFlood::start_query(IPAddress dst, int iface) {
        QueryInfo query = QueryInfo(dst, _ip);
	if (_debug) {
		click_chatter("%{element} :: %s :: start query %s seq %d iface %u", 
				this, 
				__func__,
				query.unparse().c_str(), 
				_seq,
				iface);
	}
	Packet * p = create_wing_packet(NodeAddress(_ip, iface), 
				NodeAddress(), 
				WING_PT_QUERY, 
				query._dst, 
				NodeAddress(), 
				query._src, 
				_seq, 
				PathMulti(),
				0);
	if (!p) {
		return;
	}
	append_seen(query, _seq);
	output(0).push(p);
}

void WINGMetricFlood::start_flood(Packet *p_in) {
	IPAddress dst = p_in->dst_ip_anno();
	p_in->kill();
	Vector<int> ifs = _link_table->get_local_interfaces();
	for (int i = 0; i < ifs.size(); i++) {
		start_query(dst, ifs[i]);
	}
	_seq++;
}

void WINGMetricFlood::push(int port, Packet *p_in) {
	if (port == 0) {
		start_flood(p_in);
	} else if (port == 1) {
		process_flood(p_in);
	} else {
		p_in->kill();
	}	
}

enum {
	H_CLEAR_SEEN, 
	H_FLOODS
};

String WINGMetricFlood::read_handler(Element *e, void *thunk) {
	WINGMetricFlood *td = (WINGMetricFlood *) e;
	switch ((uintptr_t) thunk) {
		case H_FLOODS: {
			StringAccum sa;
			int x;
			for (x = 0; x < td->_seen.size(); x++) {
				sa << "src " << td->_seen[x]._seen.unparse();
				sa << " seq " << td->_seen[x]._seq;
				sa << " count " << td->_seen[x]._count;
				sa << " forwarded " << td->_seen[x]._forwarded;
				sa << "\n";
			}
			return sa.take_string();
		}
		default: {
			return WINGBase<QueryInfo>::read_handler(e, thunk);
		}
	}
}

int WINGMetricFlood::write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh) {
	WINGMetricFlood *f = (WINGMetricFlood *) e;
	String s = cp_uncomment(in_s);
	switch ((intptr_t) vparam) {
		case H_CLEAR_SEEN: {
			f->_seen.clear();
			break;
		}
		default: {
			return WINGBase<QueryInfo>::write_handler(in_s, e, vparam, errh);
		}
	}
	return 0;
}

void WINGMetricFlood::add_handlers() {
	WINGBase<QueryInfo>::add_handlers();
	add_read_handler("floods", read_handler, (void *) H_FLOODS);
	add_write_handler("clear", write_handler, (void *) H_CLEAR_SEEN);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(WINGBase LinkTableMulti ARPTableMulti)
EXPORT_ELEMENT(WINGMetricFlood)

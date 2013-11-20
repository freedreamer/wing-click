/*
 * empoweropenauthresponder.{cc,hh} -- send 802.11 authentication response packets (EmPOWER Access Point)
 * John Bicket, Roberto Riggio
 *
 * Copyright (c) 2013 CREATE-NET
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/llc.h>
#include <click/straccum.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include "empoweropenauthresponder.hh"
#include "empowerlvapmanager.hh"
CLICK_DECLS

EmpowerOpenAuthResponder::EmpowerOpenAuthResponder() :
		_rtable(0), _el(0), _debug(false) {
}

EmpowerOpenAuthResponder::~EmpowerOpenAuthResponder() {
}

int EmpowerOpenAuthResponder::configure(Vector<String> &conf,
		ErrorHandler *errh) {

	return Args(conf, this, errh)
			.read_m("RT", ElementCastArg("AvailableRates"),_rtable)
			.read_m("EL", ElementCastArg("EmpowerLVAPManager"), _el)
			.read("DEBUG", _debug).complete();

}

void EmpowerOpenAuthResponder::push(int, Packet *p) {

	if (p->length() < sizeof(struct click_wifi)) {
		click_chatter("%{element} :: %s :: Packet too small: %d Vs. %d",
				      this,
				      __func__,
				      p->length(),
				      sizeof(struct click_wifi));
		p->kill();
		return;
	}

	struct click_wifi *w = (struct click_wifi *) p->data();

	uint8_t type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;

	if (type != WIFI_FC0_TYPE_MGT) {
		click_chatter("%{element} :: %s :: Received non-management packet",
				      this,
				      __func__);
		p->kill();
		return;
	}

	uint8_t subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

	if (subtype != WIFI_FC0_SUBTYPE_AUTH) {
		click_chatter("%{element} :: %s :: Received non-authentication request packet",
				      this,
				      __func__);
		p->kill();
		return;
	}

	uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

	uint16_t algo = le16_to_cpu(*(uint16_t *) ptr);
	ptr += 2;

	uint16_t seq = le16_to_cpu(*(uint16_t *) ptr);
	ptr += 2;

	uint16_t status = le16_to_cpu(*(uint16_t *) ptr);
	ptr += 2;

	EtherAddress src = EtherAddress(w->i_addr2);

    EmpowerStationState *ess = _el->lvaps()->get_pointer(src);

    //If we're not aware of this LVAP, ignore
	if (!ess) {
		click_chatter("%{element} :: %s :: Unknown station %s",
				      this,
				      __func__,
				      src.unparse().c_str());
		p->kill();
		return;
	}

	EtherAddress bssid = EtherAddress(w->i_addr3);

	//If the bssid does not match, ignore
	if (ess->_bssid != bssid) {
		click_chatter("%{element} :: %s :: BSSID does not match, expected %s received %s",
				      this,
				      __func__,
				      ess->_bssid.unparse().c_str(),
				      bssid.unparse().c_str());
		p->kill();
		return;
	}

	if (algo != WIFI_AUTH_ALG_OPEN) {
		click_chatter("%{element} :: %s :: Algorithm %d from %s not supported",
				      this,
				      __func__,
				      algo,
				      src.unparse().c_str());
		p->kill();
		return;
	}

	if (seq != 1) {
		click_chatter("%{element} :: %s :: Algorithm %u weird sequence number %d",
				      this,
				      __func__,
				      algo,
				      seq);
		p->kill();
		return;
	}

	if (_debug) {
		click_chatter("%{element} :: %s :: Algorithm %u sequence number %u status %u",
				      this,
				      __func__,
				      algo,
				      seq,
				      status);
	}

	if (ess->_authentication_status) {
		send_auth_response(src, 2, WIFI_STATUS_SUCCESS);
	} else {
		_el->send_auth_request(src);
	}

	p->kill();

}

void EmpowerOpenAuthResponder::send_auth_response(EtherAddress dst, uint16_t seq,
		uint16_t status) {

	if (_debug) {
		click_chatter("%{element} :: %s :: authentication %s sequence number %u status %u",
				      this,
				      __func__,
				      dst.unparse().c_str(),
				      seq,
				      status);

	}

    EmpowerStationState *ess = _el->lvaps()->get_pointer(dst);
	ess->_authentication_status = true;

	int len = sizeof(struct click_wifi) + 2 + /* alg */
		2 + /* seq */
		2 + /* status */
		0;

	WritablePacket *p = Packet::make(len);

	if (p == 0)
		return;

	struct click_wifi *w = (struct click_wifi *) p->data();

	w->i_fc[0] = WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_MGT | WIFI_FC0_SUBTYPE_AUTH;
	w->i_fc[1] = WIFI_FC1_DIR_NODS;

	memcpy(w->i_addr1, dst.data(), 6);
	memcpy(w->i_addr2, ess->_bssid.data(), 6);
	memcpy(w->i_addr3, ess->_bssid.data(), 6);

	w->i_dur = 0;
	w->i_seq = 0;

	uint8_t *ptr;

	ptr = (uint8_t *) p->data() + sizeof(struct click_wifi);

	*(uint16_t *) ptr = cpu_to_le16(WIFI_AUTH_ALG_OPEN);
	ptr += 2;

	*(uint16_t *) ptr = cpu_to_le16(seq);
	ptr += 2;

	*(uint16_t *) ptr = cpu_to_le16(status);
	ptr += 2;

	_el->send_status_lvap(dst);

	output(0).push(p);

}

enum {
	H_DEBUG
};

String EmpowerOpenAuthResponder::read_handler(Element *e, void *thunk) {
	EmpowerOpenAuthResponder *td = (EmpowerOpenAuthResponder *) e;
	switch ((uintptr_t) thunk) {
	case H_DEBUG:
		return String(td->_debug) + "\n";
	default:
		return String();
	}
}

int EmpowerOpenAuthResponder::write_handler(const String &in_s, Element *e,
		void *vparam, ErrorHandler *errh) {

	EmpowerOpenAuthResponder *f = (EmpowerOpenAuthResponder *) e;
	String s = cp_uncomment(in_s);

	switch ((intptr_t) vparam) {
	case H_DEBUG: {    //debug
		bool debug;
		if (!BoolArg().parse(s, debug))
			return errh->error("debug parameter must be boolean");
		f->_debug = debug;
		break;
	}
	}
	return 0;
}

void EmpowerOpenAuthResponder::add_handlers() {
	add_read_handler("debug", read_handler, (void *) H_DEBUG);
	add_write_handler("debug", write_handler, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EmpowerOpenAuthResponder)

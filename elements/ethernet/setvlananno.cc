/*
 * setvlananno.{cc,hh} -- set VLAN annotation
 *
 * Copyright (c) 2010 Intel Corporation
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
#include "setvlananno.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

SetVLANAnno::SetVLANAnno()
{
}

SetVLANAnno::~SetVLANAnno()
{
}

int
SetVLANAnno::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int vlan = 0, vlan_pcp = 0;
    if (Args(conf, this, errh)
	.read_mp("VLAN", vlan)
	.read_p("VLAN_PCP", vlan_pcp)
	.complete() < 0)
	return -1;
    if (vlan < 0 || vlan >= 0x0FFF)
	return errh->error("bad VLAN");
    if (vlan_pcp < 0 || vlan_pcp > 0x7)
	return errh->error("bad VLAN_PCP");
    _vlan_tci = htons(vlan | (vlan_pcp << 13));
    return 0;
}

Packet *
SetVLANAnno::simple_action(Packet *p)
{
    SET_VLAN_ANNO(p, _vlan_tci);
    return p;
}

String
SetVLANAnno::read_handler(Element *e, void *user_data)
{
    SetVLANAnno *eve = static_cast<SetVLANAnno *>(e);
    switch (reinterpret_cast<uintptr_t>(user_data)) {
    case h_vlan:
	return String(ntohs(eve->_vlan_tci) & 0x0FFF);
    case h_vlan_pcp:
	return String((ntohs(eve->_vlan_tci) >> 13) & 0x7);
    }
    return String();
}

void
SetVLANAnno::add_handlers()
{
    add_read_handler("vlan", read_handler, h_vlan);
    add_write_handler("vlan", reconfigure_keyword_handler, "0 VLAN");
    add_read_handler("vlan_pcp", read_handler, h_vlan_pcp);
    add_write_handler("vlan_pcp", reconfigure_keyword_handler, "1 VLAN_PCP");
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetVLANAnno SetVLANAnno-SetVlanAnno)

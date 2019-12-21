/*
 * Copyright (C) 2014-2019 fdns Authors
 *
 * This file is part of fdns project
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "fdns.h"
#include "dnslint.h"
#include "timetrace.h"

static int print_stats = 0;
static uint8_t rbuf[MAXBUF];
static ssize_t rbuf_len;


// redirect to 127.0.0.1
static uint8_t loopback_tail[] = {
	0xc0, 0x0c, 0, 1, 0, 1, 0, 0, 0xaa, 0xaa, 0, 4, 0x7f, 0, 0, 1
};

static void  build_response_loopback(uint8_t *pkt, ssize_t *lenptr) {
	// build answer RR
	pkt[2] = 0x81;
	pkt[3] = 0x80;
	pkt[6] = 0;
	pkt[7] = 0x01;
	memcpy(pkt + *lenptr, loopback_tail, sizeof(loopback_tail));
	*lenptr += sizeof(loopback_tail);
}

// build a NXDOMAIN package on top of the existing dns request
inline static void build_response_nxdomain(uint8_t *pkt) {
	// lenptr remains unchanged
	pkt[2] = 0x81;
	pkt[3] = 0x83;
}

// attempt to extract the domain name and run it through the filter
uint8_t *dns_parser(uint8_t *buf, ssize_t *lenptr, int *error) {
	assert(buf);
	assert(lenptr);
	uint8_t *pkt = buf;
	*error = 0;

	unsigned delta;
	DnsHeader *h = dnslint_header(pkt, *lenptr, &delta);
	if (!h) {
		*error = 1;
		rlogprintf("Error: rx local %s\n", dnslint_err2str());
		return NULL;
	}

	// we allow exactly one question
	if (h->questions != 1 || h->answer != 0 || h->authority || h->additional != 0) {
		*error = 1;
		rlogprintf("Error: rx local invalid header\n");
		return NULL;
	}

	pkt = pkt + delta;
	DnsQuestion *q = dnslint_question(pkt , *lenptr - delta, &delta);
	if (!q) {
		*error = 1;
		rlogprintf("Error: %s\n", dnslint_err2str());
		return NULL;
	}

//printf("domain #%s#\n", q->domain); fflush(0);
	// check packet lentght
	if ((*lenptr - sizeof(DnsHeader) - delta ) != 0) {
		*error = 1;
		rlogprintf("Error: invalid packet length\n");
		return NULL;
	}


	// clear cache name
	cache_set_name("", 0);

	//******************************
	// query type
	//******************************
	int aaaa = 0;

	// check querry filtering configuration
	if (arg_allow_all_queries == 0) {
		// type A requests
		if (q->type == 1);

		// AAAA requests
		else if (q->type == 0x1c) {
			aaaa = 1;
			if (!arg_ipv6) {
				rlogprintf("Request: %s (ipv6), dropped\n", q->domain);
				goto drop_nxdomain;
			}
		}

		// responde NXDOMAIN to PTR in order to fix apps as ping
		else if (q->type == 0x0c) {
			rlogprintf("Request: %s (PTR), dropped\n", q->domain);
			goto drop_nxdomain;
		}

		// drop all the rest and respond with NXDOMAIN
		else {
			*error = 1;	// just let him try again
			rlogprintf("Error: RR type %u rejected\n", q->type);
			return NULL;
		}
	}

	//*****************************
	// trackers/adblock filtering
	//*****************************
	if (arg_nofilter) {
		rlogprintf("Request: %s\n", q->domain);
		return NULL;
	}

	int rv = dnsfilter_blocked(q->domain, 0);
	if (rv) {
		rlogprintf("Request: %s%s, dropped\n", q->domain, (aaaa)? " (ipv6)": "");
		stats.drop++;
		build_response_loopback(buf, lenptr);
		print_stats = 1;
		return buf;
	}

	//*****************************
	// cache - only domains smaller than CACHE_NAME_LEN
	//*****************************
	if (q->len <= CACHE_NAME_LEN) {
//printf("******* %u %s\n", q->len, q->domain);
		// check cache
		uint8_t *rv = cache_check(h->id, q->domain, lenptr, aaaa);
		if (rv) {
			stats.cached++;
			rlogprintf("Request: %s%s, cached\n", q->domain, (aaaa)? " (ipv6)": "");
			print_stats = 1;
			return rv;
		}

		// set the stage for caching the reply
		cache_set_name(q->domain, aaaa);
	}

	rlogprintf("Request: %s%s, %s\n", q->domain, (aaaa)? " (ipv6)": "",
		(ssl_state == SSL_OPEN)? "encrypted": "not encrypted");

	return NULL;

drop_nxdomain:
	stats.drop++;
	build_response_nxdomain(buf);
	print_stats = 1;
	return buf;
}


#if 0
	else if (strlen(q->domain <= CACHE_NAME_LEN) {
		// check cache
		uint8_t *rv = cache_check(*buf, *(buf + 1), output + QOFFSET + 1, lenptr, aaaa);
		if (rv) {
			stats.cached++;
			rlogprintf("Request: %s%s, cached\n", output + QOFFSET + 1, (aaaa)? " (ipv6)": "");
			return rv;
		}

		// set the stage for caching the reply
		cache_set_name(output + QOFFSET + 1, aaaa);
		rlogprintf("Request: %s%s, %s\n", output + QOFFSET + 1, (aaaa)? " (ipv6)": "",
			(ssl_state == SSL_OPEN)? "encrypted": "not encrypted");
		return NULL;
	}
	else {
		rlogprintf("Request: %s%s, %s\n", output + QOFFSET + 1, (aaaa)? " (ipv6)": "",
			(ssl_state == SSL_OPEN)? "encrypted": "not encrypted");
		return NULL;
	}
#endif


#if 0
drop_nxdomain:
	stats.drop++;
	build_response_nxdomain(*buf, *(buf + 1), buf + QOFFSET, strlen(output + QOFFSET + 1) + 1 + 5);
	*lenptr = rbuf_len;
	print_stats = 1;
	return rbuf;
#endif



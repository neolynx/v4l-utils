/*
 * Copyright (c) 2013 - Andre Roth <neolynx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 */

#include <libdvbv5/mpeg_ts.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>

ssize_t dvb_mpeg_ts_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, ssize_t buflen, struct dvb_mpeg_ts **table)
{
	const uint8_t *p = buf;

	if (buf[0] != DVB_MPEG_TS) {
		dvb_logerr("mpeg ts invalid marker 0x%02x, sould be 0x%02x", buf[0], DVB_MPEG_TS);
		return -1;
	}

	if (!*table) {
		*table = calloc(sizeof(struct dvb_mpeg_ts), 1);
		if (!*table) {
			dvb_logerr("%s: out of memory", __func__);
			return -2;
		}
	}

	memcpy(*table, p, sizeof(struct dvb_mpeg_ts));
	p += sizeof(struct dvb_mpeg_ts);

	bswap16((*table)->bitfield);

	(*table)->adaption = NULL;
	if ((*table)->adaptation_field) {
		(*table)->adaption = calloc(sizeof(struct dvb_mpeg_ts_adaption), 1);
		memcpy((*table)->adaption, p, sizeof(struct dvb_mpeg_ts_adaption));
		p += (*table)->adaption->length + 1;
		/* FIXME: copy adaption->lenght bytes */
	}

	return p - buf;
}

void dvb_mpeg_ts_free(struct dvb_mpeg_ts *ts)
{
	if (ts->adaption)
		free(ts->adaption);
	free(ts);
}

void dvb_mpeg_ts_print(struct dvb_v5_fe_parms *parms, struct dvb_mpeg_ts *ts)
{
	dvb_loginfo("MPEG TS");
	dvb_loginfo("| sync            0x%02x", ts->sync_byte);
	dvb_loginfo("| tei                %d", ts->tei);
	dvb_loginfo("| payload_start      %d", ts->payload_start);
	dvb_loginfo("| priority           %d", ts->priority);
	dvb_loginfo("| pid           0x%04x", ts->pid);
	dvb_loginfo("| scrambling         %d", ts->scrambling);
	dvb_loginfo("| adaptation_field   %d", ts->adaptation_field);
	dvb_loginfo("| payload            %d", ts->payload);
	dvb_loginfo("| continuity_counter %d", ts->continuity_counter);
	if (ts->adaptation_field) {
		dvb_loginfo("|- Adaptation Field");
                dvb_loginfo("|   length         %d", ts->adaption->length);
                dvb_loginfo("|   discontinued   %d", ts->adaption->discontinued);
                dvb_loginfo("|   random_access  %d", ts->adaption->random_access);
                dvb_loginfo("|   priority       %d", ts->adaption->priority);
                dvb_loginfo("|   PCR            %d", ts->adaption->PCR);
                dvb_loginfo("|   OPCR           %d", ts->adaption->OPCR);
                dvb_loginfo("|   splicing_point %d", ts->adaption->splicing_point);
                dvb_loginfo("|   private_data   %d", ts->adaption->private_data);
                dvb_loginfo("|   extension      %d", ts->adaption->extension);
	}
}

ssize_t dvb_mpeg_ts_create(struct dvb_v5_fe_parms *parms, uint8_t *buf, ssize_t buflen,
		uint8_t **data, uint16_t pid, int padding)
{
	const uint8_t *p = buf, *endbuf = buf + buflen;
	ssize_t size, total_size, first_size;

	if (padding > 183)
		padding = 183;

	total_size = DVB_MPEG_TS_PACKET_SIZE;
	first_size = sizeof(struct dvb_mpeg_ts);
	if (padding >= 0)
		first_size += padding + 1;

	if (buflen + first_size > DVB_MPEG_TS_PACKET_SIZE) {
		int packets = ((buflen - (DVB_MPEG_TS_PACKET_SIZE - total_size)) /
			(DVB_MPEG_TS_PACKET_SIZE - sizeof(struct dvb_mpeg_ts))) + 1;
		total_size += packets * DVB_MPEG_TS_PACKET_SIZE;
	}

	*data = malloc(total_size);
	uint8_t *q = *data;

	size = offsetof( struct dvb_mpeg_ts, adaption );
	struct dvb_mpeg_ts ts, *ts_dump;
	memset(&ts, 0x00, size);
	ts.sync_byte = DVB_MPEG_TS;
	ts.pid = pid;
	ts.payload_start = 1;
	ts.payload = 1;

	memcpy(q, &ts, size );
	ts_dump = (struct dvb_mpeg_ts *) q;
	bswap16(ts_dump->bitfield);
	q += size;

	if (padding >= 0) {
		*q++ = padding;
		if (padding > 0) {
			memset(q, 0xFF, padding);
			q += padding;
		}

		size = DVB_MPEG_TS_PACKET_SIZE - sizeof(struct dvb_mpeg_ts) -
			padding - 1; /* minus padding length byte */
		if (size > buflen)
			size = buflen;
		memcpy(q, p, size);
		q += size;
		p += size;
	}

	ts.payload_start = 0;
	while (p < endbuf) {
		ts.continuity_counter++;
		memcpy(q, &ts, size );
		ts_dump = (struct dvb_mpeg_ts *) q;
		bswap16(ts_dump->bitfield);
		q += size;

		size = DVB_MPEG_TS_PACKET_SIZE - sizeof(struct dvb_mpeg_ts);
		if (size > p - endbuf)
			size = p - endbuf;
		memcpy(q, p, size);
		q += size;
		p += size;
	}

	size = (q - *data) % DVB_MPEG_TS_PACKET_SIZE;
	if (size)
		memset(q, 0xFF, DVB_MPEG_TS_PACKET_SIZE - size);

	return total_size;
}


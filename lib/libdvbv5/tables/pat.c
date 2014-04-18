/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012-2014 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/pat.h>
#include <libdvbv5/descriptors.h>
#include <libdvbv5/dvb-fe.h>
#include <libdvbv5/mpeg_ts.h>
#include <libdvbv5/crc32.h>

ssize_t dvb_table_pat_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf,
			ssize_t buflen, struct dvb_table_pat **table)
{
	const uint8_t *p = buf, *endbuf = buf + buflen;
	struct dvb_table_pat *pat;
	struct dvb_table_pat_program **head;
	size_t size;

	size = offsetof(struct dvb_table_pat, programs);
	if (p + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - p, size);
		return -1;
	}

	if (buf[0] != DVB_TABLE_PAT) {
		dvb_logerr("%s: invalid marker 0x%02x, sould be 0x%02x",
				__func__, buf[0], DVB_TABLE_PAT);
		return -2;
	}

	if (!*table) {
		*table = calloc(sizeof(struct dvb_table_pat), 1);
		if (!*table) {
			dvb_logerr("%s: out of memory", __func__);
			return -3;
		}
	}
	pat = *table;
	memcpy(pat, buf, size);
	p += size;
	dvb_table_header_init(&pat->header);

	/* find end of current list */
	head = &pat->program;
	while (*head != NULL)
		head = &(*head)->next;

	size = pat->header.section_length + 3 - DVB_CRC_SIZE; /* plus header, minus CRC */
	if (buf + size > endbuf) {
		dvb_logerr("%s: short read %zd/%zd bytes", __func__,
			   endbuf - buf, size);
		return -4;
	}
	endbuf = buf + size;

	size = offsetof(struct dvb_table_pat_program, next);
	while (p + size <= endbuf) {
		struct dvb_table_pat_program *prog;

		prog = malloc(sizeof(struct dvb_table_pat_program));
		if (!prog) {
			dvb_logerr("%s: out of memory", __func__);
			return -5;
		}

		memcpy(prog, p, size);
		p += size;

		bswap16(prog->service_id);

		if (prog->pid == 0x1fff) { /* ignore null packets */
			free(prog);
			break;
		}
		bswap16(prog->bitfield);
		pat->programs++;

		prog->next = NULL;

		*head = prog;
		head = &(*head)->next;
	}
	if (endbuf - p)
		dvb_logwarn("%s: %zu spurious bytes at the end",
			   __func__, endbuf - p);
	return p - buf;
}

void dvb_table_pat_free(struct dvb_table_pat *pat)
{
	struct dvb_table_pat_program *prog = pat->program;

	while (prog) {
		struct dvb_table_pat_program *tmp = prog;
		prog = prog->next;
		free(tmp);
	}
	free(pat);
}

void dvb_table_pat_print(struct dvb_v5_fe_parms *parms, struct dvb_table_pat *pat)
{
	struct dvb_table_pat_program *prog = pat->program;

	dvb_loginfo("PAT");
	dvb_table_header_print(parms, &pat->header);
	dvb_loginfo("|\\ program  service");

	while (prog) {
		dvb_loginfo("|-  0x%04x   0x%04x", prog->pid, prog->service_id);
		prog = prog->next;
	}
	dvb_loginfo("|_ %d program%s", pat->programs, pat->programs != 1 ? "s" : "");
}

struct dvb_table_pat *dvb_table_pat_create()
{
	struct dvb_table_pat *pat;

	pat = calloc(sizeof(struct dvb_table_pat), 1);
	pat->header.table_id = DVB_TABLE_PAT;
	pat->header.one = 3;
	pat->header.syntax = 1;
	pat->header.current_next = 1;
	pat->header.id = 1;
	pat->header.one2 = 3;
	pat->programs = 0;

	return pat;
}

struct dvb_table_pat_program *dvb_table_pat_program_create(struct dvb_table_pat *pat, uint16_t pid, uint16_t service_id)
{
	struct dvb_table_pat_program **head = &pat->program;

	/* append to the list */
	while (*head != NULL)
		head = &(*head)->next;
	*head = calloc(sizeof(struct dvb_table_pat_program), 1);
	(*head)->service_id = service_id;
	(*head)->pid = pid;
	pat->programs++;
	return *head;
}

ssize_t dvb_table_pat_store(struct dvb_v5_fe_parms *parms, const struct dvb_table_pat *pat, uint8_t **data)
{
	const struct dvb_table_pat_program *program;
	uint8_t *p;
	ssize_t size, size_total;

	*data = malloc( DVB_MAX_PAYLOAD_PACKET_SIZE );
	p = *data;


	size = offsetof(struct dvb_table_pat, programs);
	memcpy(p, pat, size);
	struct dvb_table_pat *pat_dump = (struct dvb_table_pat *) p;
	p += size;

	program = pat->program;
	while (program) {
		size = offsetof(struct dvb_table_pat_program, next);

		memcpy(p, program, size);
		struct dvb_table_pat_program *program_dump = (struct dvb_table_pat_program *) p;
		p += size;

		bswap16(program_dump->service_id);
		bswap16(program_dump->bitfield);

		program = program->next;
	}

	size_total = p - *data + DVB_CRC_SIZE;
	pat_dump->header.section_length = size_total - offsetof(struct dvb_table_header, id);
	bswap16(pat_dump->header.bitfield);

	uint32_t crc = dvb_crc32(*data, size_total - DVB_CRC_SIZE, 0xFFFFFFFF);
	bswap32(crc);
	*(uint32_t *) p = crc;

	return size_total;
}


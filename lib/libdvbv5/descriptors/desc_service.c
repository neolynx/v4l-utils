/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab
 * Copyright (c) 2012 - Andre Roth <neolynx@gmail.com>
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

#include <libdvbv5/desc_service.h>
#include <libdvbv5/dvb-fe.h>
#include <parse_string.h>

int dvb_desc_service_init(struct dvb_v5_fe_parms *parms, const uint8_t *buf, struct dvb_desc *desc)
{
	struct dvb_desc_service *service = (struct dvb_desc_service *) desc;
	const uint8_t *endbuf = buf + desc->length;
	uint8_t len;        /* the length of the string in the input data */
	uint8_t len1, len2; /* the lenght of the output strings */

	service->provider = NULL;
	service->provider_emph = NULL;
	service->name = NULL;
	service->name_emph = NULL;

	if (buf + 1 > endbuf) {
		dvb_logerr("%s: short read %d bytes", __func__, 1);
		return -1;
	}
	service->service_type = buf[0];
	buf++;

	if (buf + 1 > endbuf) {
		dvb_logerr("%s: a short read %d bytes", __func__, 1);
		return -1;
	}

	len = buf[0];
	len1 = len;
	buf++;

	if (buf + len > endbuf) {
		dvb_logerr("%s: b short read %d bytes", __func__, len);
		return -1;
	}

	if (len) {
		dvb_parse_string(parms, &service->provider, &service->provider_emph, buf, len1);
		buf += len;
	}

	if (buf + 1 > endbuf) {
		dvb_logerr("%s: c short read %d bytes", __func__, 1);
		return -1;
	}

	len = buf[0];
	len2 = len;
	buf++;

	if (buf + len > endbuf) {
		dvb_logerr("%s: d short read %d bytes", __func__, len);
		return -1;
	}

	if (len) {
		dvb_parse_string(parms, &service->name, &service->name_emph, buf, len2);
		buf += len;
	}
	return 0;
}

void dvb_desc_service_free(struct dvb_desc *desc)
{
	struct dvb_desc_service *service = (struct dvb_desc_service *) desc;
	free(service->provider);
	free(service->provider_emph);
	free(service->name);
	free(service->name_emph);
}

void dvb_desc_service_print(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc)
{
	const struct dvb_desc_service *service = (const struct dvb_desc_service *) desc;
	dvb_loginfo("|           service type  %d", service->service_type);
	dvb_loginfo("|           provider      '%s'", service->provider);
	dvb_loginfo("|           name          '%s'", service->name);
}

struct dvb_desc *dvb_desc_service_create()
{
	  struct dvb_desc_service *desc = calloc( sizeof( struct dvb_desc_service ), 1 );
	  desc->type = service_descriptor;
	  return (struct dvb_desc *) desc;
}

ssize_t dvb_desc_service_store(struct dvb_v5_fe_parms *parms, const struct dvb_desc *desc, uint8_t *data)
{
	const struct dvb_desc_service *service = (const struct dvb_desc_service *) desc;
	data[0] = desc->type;
	uint8_t *p = data + 2;

	*p++ = service->service_type;

	if (service->provider) {
		int len = strlen(service->provider);
		int buflen = len * 3;
		char *buf = malloc(buflen);
		dvb_iconv_to_charset(parms, buf, buflen,
				     (const unsigned char *) service->provider,
				     len, "utf-8", parms->output_charset);
		int len2 = strlen(buf);
		if (len2) {
			*p++ = len2;
			memcpy(p, buf, len2);
			p += len2;
		}
		free(buf);
	} else {
		*p++ = 0;
	}

	if (service->name) {
		int len = strlen(service->name);
		int buflen = len * 3;
		char *buf = malloc(buflen);
		dvb_iconv_to_charset(parms, buf, buflen,
				     (const unsigned char *) service->name,
				     len, "utf-8", parms->output_charset);
		int len2 = strlen(buf);
		if (len2) {
			*p++ = len2;
			memcpy(p, buf, len2);
			p += len2;
		}
		free(buf);
	} else {
		*p++ = 0;
	}

	/* set descriptor length */
	data[1] = p - data - 2;
	return p - data;
}


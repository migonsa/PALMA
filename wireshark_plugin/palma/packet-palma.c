/* packet-palma.c
 * Routines for PALMA Address Allocation Protocol defined by ...
 * Copyright 2020, Miguel González Saiz <100346858@alumnos.uc3m.es>
 *
 * $Id$
 *  
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/etypes.h>
#include <epan/to_str.h>

#define PALMA_ETHERTYPE		0x33ff

/* PALMA Field Offsets */
#define PALMA_SUBTYPE_OFFSET				0
#define PALMA_VERSION_OFFSET				1
#define PALMA_MSG_TYPE_OFFSET				1
#define PALMA_CONTROL_OFFSET				2
#define PALMA_TOKEN_OFFSET					4
#define PALMA_STATUS_OFFSET					6
#define PALMA_LENGTH_OFFSET					6
#define PALMA_PARS_OFFSET					8

/* Bit Field Masks */
#define PALMA_VERSION_MASK					0xe0
#define PALMA_MSG_TYPE_MASK					0x1f
#define PALMA_STATUS_MASK					0xf0
#define PALMA_LENGTH_MASK					0x0fff

/* PALMA message_type */

#define PALMA_MSG_TYPE_DISCOVER				0x01
#define PALMA_MSG_TYPE_OFFER				0x02
#define PALMA_MSG_TYPE_REQUEST				0x03
#define PALMA_MSG_TYPE_ACK					0x04
#define PALMA_MSG_TYPE_RELEASE				0x05
#define PALMA_MSG_TYPE_DEFEND				0x06
#define PALMA_MSG_TYPE_RANNOUNCE			0x07

#define MAX_PARS							6
#define INITIALIZE_PARS_VARS				{-1, -1, -1, -1, -1, -1}

/* PALMA par_type */
#define PALMA_PAR_TYPE_STATION_ID		0x01
#define PALMA_PAR_TYPE_MAC_ADDR_SET		0x02
#define PALMA_PAR_TYPE_NETWORK_ID		0x03
#define PALMA_PAR_TYPE_LIFETIME			0x04
#define PALMA_PAR_TYPE_CLIENT_ADDR		0x05
#define PALMA_PAR_TYPE_VENDOR			0x06

/* PALMA Subtype */
#define PALMA_CLIENT					0x00
#define PALMA_SERVER					0x01

/* PALMA Control word bits */
#define PALMA_AAI_CW_BIT			(1<<0)
#define PALMA_ELI_CW_BIT			(1<<1)
#define PALMA_SAI_CW_BIT			(1<<2)
#define PALMA_MAC64_CW_BIT			(1<<4)
#define PALMA_MCAST_CW_BIT			(1<<5)
#define PALMA_SERVER_CW_BIT			(1<<6)
#define PALMA_SETPROV_CW_BIT		(1<<7)
#define PALMA_STATIONID_CW_BIT		(1<<8)
#define PALMA_NETWORKID_CW_BIT		(1<<9)
#define PALMA_CODEFIELD_CW_BIT		(1<<10)
#define PALMA_VENDOR_CW_BIT			(1<<11)
#define PALMA_RENEWAL_CW_BIT		(1<<12)

/* PALMA status */
#define PALMA_STATUS_FIELD_UNUSED		0
#define PALMA_STATUS_ASSIGN_OK			1
#define PALMA_STATUS_ALTERNATE_SET		2
#define PALMA_STATUS_FAIL_CONFLICT		3
#define PALMA_STATUS_FAIL_DISALLOWED	4
#define PALMA_STATUS_FAIL_TOO_LARGE		5
#define PALMA_STATUS_FAIL_OTHER			6

static const value_string palma_msg_type_vals [] = {
	{PALMA_MSG_TYPE_DISCOVER,		"DISCOVER"},
	{PALMA_MSG_TYPE_OFFER,			"OFFER"},
	{PALMA_MSG_TYPE_REQUEST,		"REQUEST"},
	{PALMA_MSG_TYPE_ACK,			"ACK"},
	{PALMA_MSG_TYPE_RELEASE,		"RELEASE"},
	{PALMA_MSG_TYPE_DEFEND,			"DEFEND"},
	{PALMA_MSG_TYPE_RANNOUNCE,		"ANNOUNCE"},
    {0,								NULL}
};

static const value_string palma_par_type_vals [] = {
    {PALMA_PAR_TYPE_STATION_ID,			"Station_ID"},
    {PALMA_PAR_TYPE_MAC_ADDR_SET,		"MAC Address Set"},
    {PALMA_PAR_TYPE_NETWORK_ID,			"Network_ID"},
    {PALMA_PAR_TYPE_LIFETIME,			"Lifetime"},
    {PALMA_PAR_TYPE_CLIENT_ADDR,		"Client Address"},
    {PALMA_PAR_TYPE_VENDOR,				"Vendor Specific"},
    {0,							NULL}
};

static const value_string palma_subtype_vals [] = {
    {PALMA_CLIENT,		"PALMA CLIENT"},
    {PALMA_SERVER,		"PALMA SERVER"},
    {0,							NULL}
};

static const value_string palma_status_vals [] = {
    {PALMA_STATUS_FIELD_UNUSED,		"Field not used"},
    {PALMA_STATUS_ASSIGN_OK,		"Proposed assignment agreed"},
    {PALMA_STATUS_ALTERNATE_SET,	"Alternate addres set provided"},
    {PALMA_STATUS_FAIL_CONFLICT,	"Assignment rejected– address conflict"},
    {PALMA_STATUS_FAIL_DISALLOWED,	"Assignment rejected – requested address administratively disallowed"},
    {PALMA_STATUS_FAIL_TOO_LARGE,	"Assignment rejected – requested address set too large"},
    {PALMA_STATUS_FAIL_OTHER,		"Assignment rejected – other administrative reasons"},
    {0,								NULL}
};

/**********************************************************/
/* Initialize the protocol and registered fields          */
/**********************************************************/
static int proto_palma = -1;
//static int proto_palma_pars = -1;

/* PALMA PDU */
static int hf_palma_subtype = -1;
static int hf_palma_version = -1;
static int hf_palma_message_type = -1;
static int hf_palma_control = -1;
static int hf_palma_token = -1;
static int hf_palma_status = -1;
static int hf_palma_length = -1;

static int hf_palma_aai_cw_bit = -1;
static int hf_palma_eli_cw_bit = -1;
static int hf_palma_sai_cw_bit = -1;
static int hf_palma_mac64_cw_bit = -1;
static int hf_palma_mcast_cw_bit = -1;
static int hf_palma_server_cw_bit = -1;
static int hf_palma_mac_provided_cw_bit = -1;
static int hf_palma_stationid_cw_bit = -1;
static int hf_palma_networkid_cw_bit = -1;
static int hf_palma_codefield_cw_bit = -1;
static int hf_palma_vendor_cw_bit = -1;
static int hf_palma_renewal_cw_bit = -1;

static int hf_palma_par_type[MAX_PARS] = INITIALIZE_PARS_VARS;
static int hf_palma_par_len[MAX_PARS] = INITIALIZE_PARS_VARS;

static int hf_palma_station_id = -1;
static int hf_palma_48_bit_address[2] = {-1, -1};
static int hf_palma_64_bit_address[2] = {-1, -1};
static int hf_palma_set_len[2] = {-1, -1};
static int hf_palma_48_bit_mask[2] = {-1, -1};
static int hf_palma_64_bit_mask[2] = {-1, -1};
static int hf_palma_network_id = -1;
static int hf_palma_lifetime = -1;
static int hf_palma_client_address = -1;
static int hf_palma_vendor = -1;

static const int* bits[] = {
	&hf_palma_aai_cw_bit,
	&hf_palma_eli_cw_bit,
	&hf_palma_sai_cw_bit,
	&hf_palma_mac64_cw_bit,
	&hf_palma_mcast_cw_bit,
	&hf_palma_server_cw_bit,
	&hf_palma_mac_provided_cw_bit,
	&hf_palma_stationid_cw_bit,
	&hf_palma_networkid_cw_bit,
	&hf_palma_codefield_cw_bit,
	&hf_palma_vendor_cw_bit,
	&hf_palma_renewal_cw_bit,
	NULL
};

/* Initialize the subtree pointers */
static int ett_palma = -1;
static int ett_control_bits = -1; 
static int ett_par[MAX_PARS] = INITIALIZE_PARS_VARS;

static int
dissect_palma(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
	gint offset = PALMA_PARS_OFFSET;
	guint16 msglen = (tvb_get_guint16(tvb, PALMA_LENGTH_OFFSET, ENC_BIG_ENDIAN) & PALMA_LENGTH_MASK) -
			PALMA_PARS_OFFSET;

    guint8      palma_msg_type;
    proto_item *palma_item     = NULL;
    proto_tree *palma_tree     = NULL;

	gint addr_set_index = 0;

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "PALMA");
    col_clear(pinfo->cinfo, COL_INFO);

    /* The palma msg type will be handy in a moment */
    palma_msg_type = tvb_get_guint8(tvb, PALMA_MSG_TYPE_OFFSET);
    palma_msg_type &= PALMA_MSG_TYPE_MASK;

    /* Display the name of the packet type in the info column. */
    col_add_fstr(pinfo->cinfo, COL_INFO, "%s:",
                val_to_str(palma_msg_type, palma_msg_type_vals,
                            "Unknown Type(0x%02x)"));
	
	/*Ack status variables */
	guint8 ack_status = tvb_get_guint8(tvb, PALMA_STATUS_OFFSET);
   	ack_status &= PALMA_STATUS_MASK;
	ack_status >>= 4;
    /* Now, we'll add pars data to the info column as appropriate */

    palma_item = proto_tree_add_item(tree, proto_palma, tvb, 0, -1, ENC_NA);
    palma_tree = proto_item_add_subtree(palma_item, ett_palma);

    proto_tree_add_item(palma_tree, hf_palma_subtype,		tvb,
		PALMA_SUBTYPE_OFFSET,			1, ENC_BIG_ENDIAN);
    proto_tree_add_item(palma_tree, hf_palma_version,		tvb,
		PALMA_VERSION_OFFSET,			1, ENC_BIG_ENDIAN);
    proto_tree_add_item(palma_tree, hf_palma_message_type,	tvb,
		PALMA_MSG_TYPE_OFFSET,			1, ENC_BIG_ENDIAN);
	proto_tree_add_bitmask(palma_tree,						tvb,
		PALMA_CONTROL_OFFSET, hf_palma_control, ett_control_bits,
		bits, ENC_BIG_ENDIAN);
    proto_tree_add_item(palma_tree, hf_palma_token,		tvb,
		PALMA_TOKEN_OFFSET,			2, ENC_BIG_ENDIAN);
    proto_tree_add_item(palma_tree, hf_palma_status,		tvb,
		PALMA_STATUS_OFFSET,			1, ENC_BIG_ENDIAN);
    proto_tree_add_item(palma_tree, hf_palma_length,		tvb,
		PALMA_LENGTH_OFFSET,			2, ENC_BIG_ENDIAN);
	
	if(palma_msg_type == PALMA_MSG_TYPE_ACK && ack_status > PALMA_STATUS_ALTERNATE_SET)
	{
		col_append_fstr(pinfo->cinfo, COL_INFO, " %s",
	            val_to_str(ack_status, palma_status_vals,
	                        "Unknown Code(0x%02x)"));
	}
	for(int npars=0; npars<MAX_PARS && msglen; npars++)
	{
		guint8 partype = tvb_get_guint8(tvb, offset);
		guint8 parlen = tvb_get_guint8(tvb, offset+1);
		proto_item *ti = NULL;
		proto_tree *subtree = proto_tree_add_subtree_format(palma_tree, tvb, 0, -1,
			ett_par[npars], &ti, 
			"PARAMETER %d: %s%s", npars, val_to_str(partype, palma_par_type_vals,"(%s)"),
			partype == PALMA_PAR_TYPE_MAC_ADDR_SET && addr_set_index ? " (IN CONFLICT)" : "");
		proto_tree_add_item(subtree, hf_palma_par_type[npars],tvb,
			offset++, 1, ENC_BIG_ENDIAN);
		proto_tree_add_item(subtree, hf_palma_par_len[npars],	tvb,
			offset++, 1, ENC_BIG_ENDIAN);
		switch(partype)
		{
			case PALMA_PAR_TYPE_STATION_ID:
				proto_tree_add_item(subtree, hf_palma_station_id, tvb, offset, parlen - 2, ENC_ASCII);
				/*col_append_fstr(pinfo->cinfo, COL_INFO, " Station_id=%s",
                    tvb_ether_to_str(tvb, offset),
                    tvb_get_ntohs(tvb, PALMA_REQ_COUNT_OFFSET));*/
				offset += parlen - 2;
				break;
			case PALMA_PAR_TYPE_MAC_ADDR_SET:
				if(parlen == 10 || parlen == 14)
				{
					proto_tree_add_item(subtree, hf_palma_48_bit_address[addr_set_index], 
						tvb, offset, 6, ENC_BIG_ENDIAN);
					col_append_fstr(pinfo->cinfo, COL_INFO, " %s:[start=%s", 
						addr_set_index ? "CONFLICT" : "SET", 
						tvb_ether_to_str(tvb, offset));
					offset += 6;
				}
				else if(parlen == 12 || parlen == 18)
				{
					proto_tree_add_item(subtree, hf_palma_64_bit_address[addr_set_index], 
						tvb, offset, 8, ENC_BIG_ENDIAN);
					col_append_fstr(pinfo->cinfo, COL_INFO, " %s:[start=%s",
						addr_set_index ? "CONFLICT" : "SET", 
						tvb_eui64_to_str(tvb, offset));
					offset += 8;
				}
				else
				{
					col_append_fstr(pinfo->cinfo, COL_INFO, " BAD SET");
					offset += parlen - 2;					
				}
				if(parlen == 10 || parlen == 12)
				{
					proto_tree_add_item(subtree, hf_palma_set_len[addr_set_index], 
						tvb, offset, 2, ENC_BIG_ENDIAN);
					col_append_fstr(pinfo->cinfo, COL_INFO, ", cnt=%d]", tvb_get_ntohs(tvb, offset));
					offset += 2;
				}
				else if(parlen == 14)
				{
					proto_tree_add_item(subtree, hf_palma_48_bit_mask[addr_set_index], 
						tvb, offset, 6, ENC_BIG_ENDIAN);
					col_append_fstr(pinfo->cinfo, COL_INFO, " mask=%s]", 
						tvb_ether_to_str(tvb, offset));
					offset += 6;
				}
				else if(parlen == 18)
				{
					proto_tree_add_item(subtree, hf_palma_64_bit_mask[addr_set_index], 
						tvb, offset, 8, ENC_BIG_ENDIAN);
					col_append_fstr(pinfo->cinfo, COL_INFO, " mask=%s]", 
						tvb_eui64_to_str(tvb, offset));
					offset += 8;
				}
				addr_set_index++;
				break;
			case PALMA_PAR_TYPE_NETWORK_ID:
				proto_tree_add_item(subtree, hf_palma_network_id, tvb, offset, parlen - 2, ENC_ASCII);
				offset += parlen - 2;
				break;
			case PALMA_PAR_TYPE_LIFETIME:
				proto_tree_add_item(subtree, hf_palma_lifetime, tvb, offset, 2, ENC_BIG_ENDIAN);
				col_append_fstr(pinfo->cinfo, COL_INFO, " lifetime=%d s", tvb_get_ntohs(tvb, offset));
				offset += 2;
				break;
			case PALMA_PAR_TYPE_CLIENT_ADDR:
				proto_tree_add_item(subtree, hf_palma_client_address, tvb, offset, 6, ENC_BIG_ENDIAN);
				col_append_fstr(pinfo->cinfo, COL_INFO, " client addr=%s", tvb_ether_to_str(tvb, offset));
				offset += 6;
				break;
			case PALMA_PAR_TYPE_VENDOR:
				proto_tree_add_item(subtree, hf_palma_vendor, tvb, offset, parlen - 2, ENC_ASCII);
				offset += parlen - 2;
				break;
		}
		msglen -= parlen;
	}
	return tvb_captured_length(tvb);
} /* end dissect_palma() */

/* Register the protocol with Wireshark */
void
proto_register_palma(void)
{
    static hf_register_info hf[] = {
        { &hf_palma_subtype,
            { "Subtype", "palma.subtype",
                FT_UINT8, BASE_HEX,
                VALS(palma_subtype_vals), 0x00,
                NULL, HFILL }},

        { &hf_palma_message_type,
            { "Message Type", "palma.message_type",
                FT_UINT8, BASE_HEX,
                VALS(palma_msg_type_vals), PALMA_MSG_TYPE_MASK,
                NULL, HFILL }},

        { &hf_palma_version,
            { "Version", "palma.version",
                FT_UINT8, BASE_HEX,
                NULL, PALMA_VERSION_MASK,
                NULL, HFILL }},

        { &hf_palma_control,
            { "Control Word", "palma.control",
                FT_UINT16, BASE_HEX,
                NULL, 0,
                NULL, HFILL }},

        { &hf_palma_aai_cw_bit,
            { "AAI bit", "palma.control.aai", 
            FT_BOOLEAN, 16,
            NULL, PALMA_AAI_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_eli_cw_bit,
            { "ELI bit", "palma.control.eli", 
            FT_BOOLEAN, 16,
            NULL, PALMA_ELI_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_sai_cw_bit,
            { "SAI bit", "palma.control.sai", 
            FT_BOOLEAN, 16,
            NULL, PALMA_SAI_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_mac64_cw_bit,
            { "MAC64 bit", "palma.control.mac64", 
            FT_BOOLEAN, 16,
            NULL, PALMA_MAC64_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_mcast_cw_bit,
            { "MULTICAST bit", "palma.control.mcast", 
            FT_BOOLEAN, 16,
            NULL, PALMA_MCAST_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_server_cw_bit,
            { "SERVER bit", "palma.control.server", 
            FT_BOOLEAN, 16,
            NULL, PALMA_SERVER_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_mac_provided_cw_bit,
            { "MAC provided bit", "palma.control.macprov", 
            FT_BOOLEAN, 16,
            NULL, PALMA_SETPROV_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_stationid_cw_bit,
            { "STATION ID bit", "palma.control.stationid", 
            FT_BOOLEAN, 16,
            NULL, PALMA_STATIONID_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_networkid_cw_bit,
            { "NETWORK ID bit", "palma.control.networkid", 
            FT_BOOLEAN, 16,
            NULL, PALMA_NETWORKID_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_codefield_cw_bit,
            { "Codefield bit", "palma.control.codefield", 
            FT_BOOLEAN, 16,
            NULL, PALMA_CODEFIELD_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_vendor_cw_bit,
            { "Specific Vendor bit", "palma.control.vendor", 
            FT_BOOLEAN, 16,
            NULL, PALMA_VENDOR_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_renewal_cw_bit,
            { "Renewal bit", "palma.control.renewal", 
            FT_BOOLEAN, 16,
            NULL, PALMA_RENEWAL_CW_BIT,
            NULL, HFILL }},

        { &hf_palma_token,
            { "Token", "palma.token",
                FT_UINT16, BASE_HEX,
                NULL, 0x00,
                NULL, HFILL }},

        { &hf_palma_status,
            { "Status", "palma.status",
                FT_UINT8, BASE_HEX,
                VALS(palma_status_vals), PALMA_STATUS_MASK,
                NULL, HFILL }},

        { &hf_palma_length,
            { "Length", "palma.length",
                FT_UINT16, BASE_DEC,
                NULL, PALMA_LENGTH_MASK,
                NULL, HFILL }},

		/* PAR */
        { &hf_palma_par_type[0],
            { "Type", "palma.par_type_0",
                FT_UINT8, BASE_HEX,
                VALS(palma_par_type_vals), 0x00,
                NULL, HFILL }},
        { &hf_palma_par_type[1],
            { "Type", "palma.par_type_1",
                FT_UINT8, BASE_HEX,
                VALS(palma_par_type_vals), 0x00,
                NULL, HFILL }},
        { &hf_palma_par_type[2],
            { "Type", "palma.par_type_2",
                FT_UINT8, BASE_HEX,
                VALS(palma_par_type_vals), 0x00,
                NULL, HFILL }},
        { &hf_palma_par_type[3],
            { "Type", "palma.par_type_3",
                FT_UINT8, BASE_HEX,
                VALS(palma_par_type_vals), 0x00,
                NULL, HFILL }},
        { &hf_palma_par_type[4],
            { "Type", "palma.par_type_4",
                FT_UINT8, BASE_HEX,
                VALS(palma_par_type_vals), 0x00,
                NULL, HFILL }},

        { &hf_palma_par_type[5],
            { "Type", "palma.par_type_5",
                FT_UINT8, BASE_HEX,
                VALS(palma_par_type_vals), 0x00,
                NULL, HFILL }},

        { &hf_palma_par_len[0],
            { "Length", "palma.par_len_0",
                FT_UINT8, BASE_DEC,
                NULL, 0x00,
                NULL, HFILL }},	
        { &hf_palma_par_len[1],
            { "Length", "palma.par_len_1",
                FT_UINT8, BASE_DEC,
                NULL, 0x00,
                NULL, HFILL }},	
        { &hf_palma_par_len[2],
            { "Length", "palma.par_len_2",
                FT_UINT8, BASE_DEC,
                NULL, 0x00,
                NULL, HFILL }},	
        { &hf_palma_par_len[3],
            { "Length", "palma.par_len_3",
                FT_UINT8, BASE_DEC,
                NULL, 0x00,
                NULL, HFILL }},	
        { &hf_palma_par_len[4],
            { "Length", "palma.par_len_4",
                FT_UINT8, BASE_DEC,
                NULL, 0x00,
                NULL, HFILL }},
        { &hf_palma_par_len[5],
            { "Length", "palma.par_len_5",
                FT_UINT8, BASE_DEC,
                NULL, 0x00,
                NULL, HFILL }},
	
        { &hf_palma_station_id,
            { "Station Id", "palma.station_id",
                FT_STRING, BASE_NONE,
                NULL, 0x00,
                NULL, HFILL }},

        { &hf_palma_48_bit_address[0],
            { "Address", "palma.addr_48",
                FT_ETHER, BASE_NONE,
                NULL, 0x00,
                NULL, HFILL }},

        { &hf_palma_64_bit_address[0],
            { "Address", "palma.addr_64",
                FT_UINT64, BASE_HEX,
                NULL, 0x00,
                NULL, HFILL }},
 
        { &hf_palma_set_len[0],
            { "Count", "palma.addr_len",
                FT_UINT16, BASE_DEC,
                NULL, 0x00,
                NULL, HFILL }},
 
        { &hf_palma_48_bit_mask[0],
            { "Mask", "palma.addr_mask_48",
                FT_ETHER, BASE_NONE,
                NULL, 0x00,
                NULL, HFILL }},

        { &hf_palma_64_bit_mask[0],
            { "Mask", "palma.addr_mask_64",
                FT_UINT64, BASE_HEX,
                NULL, 0x00,
                NULL, HFILL }},

        { &hf_palma_48_bit_address[1],
            { "Conflict Address", "palma.confict_addr_48",
                FT_ETHER, BASE_NONE,
                NULL, 0x00,
                NULL, HFILL }},

        { &hf_palma_64_bit_address[1],
            { "Conflict Address", "palma.confict_addr_64",
                FT_UINT64, BASE_HEX,
                NULL, 0x00,
                NULL, HFILL }},
 
        { &hf_palma_set_len[1],
            { "Count", "palma.confict_addr_len",
                FT_UINT16, BASE_DEC,
                NULL, 0x00,
                NULL, HFILL }},
 
        { &hf_palma_48_bit_mask[1],
            { "Mask", "palma.confict_addr_mask_48",
                FT_ETHER, BASE_NONE,
                NULL, 0x00,
                NULL, HFILL }},

        { &hf_palma_64_bit_mask[1],
            { "Mask", "palma.confict_addr_mask_64",
                FT_UINT64, BASE_HEX,
                NULL, 0x00,
                NULL, HFILL }},

        { &hf_palma_network_id,
            { "Network Id", "palma.network_id",
                FT_STRING, BASE_NONE,
                NULL, 0x00,
                NULL, HFILL }},
	
        { &hf_palma_lifetime,
            { "Lifetime", "palma.lifetime",
                FT_UINT16, BASE_DEC,
                NULL, 0x00,
                NULL, HFILL }},

        { &hf_palma_client_address,
            { "Client Address", "palma.client_addr",
                FT_ETHER, BASE_NONE,
                NULL, 0x00,
                NULL, HFILL }},

		{ &hf_palma_vendor,
            { "Vendor", "palma.vendor",
                FT_STRING, BASE_NONE,
                NULL, 0x00,
                NULL, HFILL }},

    }; /* end of static hf_register_info hf[] = */

    /* Setup protocol subtree array */
    static gint *ett[] = { &ett_palma,
						&ett_control_bits, 
						&ett_par[0],
						&ett_par[1],
						&ett_par[2],
						&ett_par[3],
						&ett_par[4],
						&ett_par[5],
						};

    /* Register the protocol name and description */
    proto_palma = proto_register_protocol (
        "IEEE PALMA Protocol", /* name */
        "PALMA", /* short name */
        "palma" /* abbrev */
        );

    /* Required function calls to register the header fields and subtrees used */
    proto_register_field_array(proto_palma, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));

} /* end proto_register_palma() */

void
proto_reg_handoff_palma(void)
{
    dissector_handle_t palma_handle;

    palma_handle = create_dissector_handle(dissect_palma, proto_palma);
    dissector_add_uint("ethertype", PALMA_ETHERTYPE, palma_handle);
}


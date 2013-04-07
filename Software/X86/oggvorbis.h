/***************************************************************************
 *   Copyright (C) 2008 by Steve Heindel   *
 *   stevenheindel@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef OGGVORBIS_H
#define OGGVORBIS_H

#include "packet.h"
#include "codebook.h"
#include "floor1.h"
#include "residue.h"
#include "mapping.h"
#include "mode.h"

typedef struct ogg_page {
	/** Capture pattern */
	int capture_pattern;
	/** Version */
	int version;
	/** Header type */
	int header_type;
	/** Granule position */
	long long granule_position;
	/** Bitstream serial number */
	int bitstream_serial_number;
	/** Page sequence number */
	int sequence_number;
	/** CRC checksum */
	int CRC_checksum;
	/** Page segments */
	int segments;
	/** Segment table */
	int *segment_table;
	
	/** Page data length */
	int data_length;
	/** Page data */
	int *data;
	
	/** Vorbis packet */
	Packet packet;
} ogg_page;

typedef struct vorbis_info {
	/** Vorbis ID header - audio channels */
	int audio_channels;
	/** Vorbis ID header - audio sample rate */
	int audio_sample_rate;
	
	/** Vorbis ID header - bitrate maximum */
	int bitrate_maximum;
	/** Vorbis ID header - bitrate nominal */
	int bitrate_nominal;
	/** Vorbis ID header - bitrate minimum */
	int bitrate_minimum;
	
	/** Vorbis ID header - blocksize 0 */
	int blocksize_0;
	/** Vorbis ID header - blocksize 1 */
	int blocksize_1;
	
	/** Vorbis comment header - vendor length */
	int vendor_length;
	/** Vorbis comment header - vendor string */
	int *vendor_string;
	
	/** Vorbis comment header - user comment list length */
	int user_comment_list_length;
	/** Vorbis comment header - user comment length array */
	int *user_comment_length;
	/** Vorbis comment header - user comment array */
	int **user_comment;
	
	/** Vorbis setup header - codebooks - codebook count */
	int vorbis_codebook_count;
	/** Vorbis setup header - codebooks - storage array */
	Codebook *codebook_config;
	
	/** Vorbis setup header - time domain transforms - time count */
	int vorbis_time_count;
	
	/** Vorbis setup header - floors - floor count */
	int vorbis_floor_count;
	/** Vorbis setup header - floors - floor types array */
	int *vorbis_floor_types;
	/** Vorbis setup header - floors - storage array */
	Floor1 *floor_config;
	
	/** Vorbis setup header - residues - residue count */
	int vorbis_residue_count;
	/** Vorbis setup header - residues - residue types array */
	int *vorbis_residue_types;
	/** Vorbis setup header - residues - storage array */
	Residue *residue_config;
	
	/** Vorbis setup header - mappings - mapping count */
	int vorbis_mapping_count;
	/** Vorbis setup header - mappings - storage array */
	Mapping *mapping_config;
	
	/** Vorbis setup header - modes - mode count */
	int vorbis_mode_count;
	/** Vorbis setup header - modes - storage array */
	Mode *mode_config;
} vorbis_info;

#endif

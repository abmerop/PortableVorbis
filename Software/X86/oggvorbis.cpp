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

#include <iostream>
#include <cstdlib>
#include <stdio.h>

#define PI 3.14159265

#include "oggvorbis.h"

#include "audio.h"
#include "bitfile.h"
#include "codebook.h"
#include "floor1.h"
#include "mapping.h"
#include "mode.h"
#include "packet.h"
#include "residue.h"

#include "CRC_lookup_table.h"
#include "floor1_inverse_dB_table.h"

using namespace std;

#include "mdct.h"
#include "mdct.c"

class OggVorbis {
public:
	bool debug; // Set true to see debug info
	bool warning; // Set true to see warnings
	
	/** The file to read data from */
	BitFile *file;
	
	/** Ogg page info */
	ogg_page page;
	
	/** Our header information: ID, comment, setup */
	vorbis_info info;
	
	/** Vorbis audio decode storage */
	Audio audio;
	/** Is *audio an unpreceded audio packet? */
	bool first_packet;
	
	int **mdctright;
	char *pcmout;
	
	/**
	 * Constructor which sets off the Ogg/Vorbis decoding process
	 * @param infile The file to read from
	 */
	OggVorbis(char *filename) {
		// Initializations
		cout << hex;
		
		debug = false;
		warning = false;
		
		page.segment_table = NULL;
		page.data = NULL;
		page.packet.data = NULL;
		
		first_packet = true;
		
		pcmout = new char[4096];
		for (int i=0; i<4096; i++)
			pcmout[i] = 0;
		
		file = new BitFile(filename);
		
		// Do something real
		read_ogg_header();
		
		init_vorbis_packet();
		read_vorbis_id_header();
		
		init_vorbis_packet();
		read_vorbis_comment_header();
		
		init_vorbis_packet();
		read_vorbis_setup_header();
		
		mdctright = new int*[info.audio_channels];
		for (int i=0; i<info.audio_channels; i++) {
			mdctright[i] = new int[info.blocksize_1/4];
			for (int j=0; j<info.blocksize_1/4; j++)
				mdctright[i][j] = 0;
		}


		// Writes a wav header (for testing)
		/*
		unsigned int wave_magic[11] = { 0x46464952, 0x00056424, 0x45564157, 0x20746d66, 0x00000010, 0x00010001, 0x0000ac44, 0x00015888, 0x00100002, 0x61746164, 0x00056400 };
		for (int i=0; i<11; i++) {
			fwrite(&(wave_magic[i]), 4, 1, stdout);
		}
		*/
		
		while (!file->eof) {
			init_vorbis_packet();
			decode_audio();
		}
		
		// Cleanup
		for (int i=0; i<info.audio_channels; i++)
			delete mdctright[i];
		delete mdctright;
		delete pcmout;
	}
	
	/**
	 * Reads in and verifies the ogg header - program exits in case of bad header
	 */
	void read_ogg_header() {
		int *in = new int[8];
		
		// Magic capture pattern "OggS"
		page.capture_pattern = 0;
		file->readbytes(in, 4);
		for (int i=0; i<4; i++) // This could potentially be worked around
			page.capture_pattern |= ((in[i]&0xFF) << (i*8));
		
		if (page.capture_pattern != 0x5367674F) {
			cout << "Error: Magic capture pattern \"OggS\" not found" << endl;
			exit(1);
		}
		
		// Version
		file->readbytes(in, 1);
		page.version = in[0];
		
		if (page.version != 0) {
			cout << "Error: Not Ogg version 0" << endl;
			exit(1);
		}
		
		// Header type
		file->readbytes(in, 1);
		page.header_type = in[0];
		
		if (page.header_type < 0 || page.header_type > 7) {
			cout << "Error: Illegal header type" << endl;
			exit(1);
		}
		
		// Granule position
		page.granule_position = 0;
		file->readbytes(in, 8);
		for (int i=0; i<8; i++)
			page.granule_position |= ((in[i]&0xFF) << (i*8));
		
		// Bitstream serial number
		page.bitstream_serial_number = 0;
		file->readbytes(in, 4);
		for (int i=0; i<4; i++)
			page.bitstream_serial_number |= ((in[i]&0xFF) << (i*8));
		
		// Page sequence number
		page.sequence_number = 0;
		file->readbytes(in, 4);
		for (int i=0; i<4; i++)
			page.sequence_number |= ((in[i]&0xFF) << (i*8));
		
		// CRC checksum
		page.CRC_checksum = 0;
		file->readbytes(in, 4);
		for (int i=0; i<4; i++)
			page.CRC_checksum |= ((in[i]&0xFF) << (i*8));
		
		// Page segments
		file->readbytes(in, 1);
		page.segments = in[0];
		
		// Segment table
		delete page.segment_table; // This should probably be moved somewhere better...
		page.segment_table = new int[page.segments];  // !5
		file->readbytes(page.segment_table, page.segments);
		
		// Page data length
		page.data_length = 0;
		for (int i=0; i<page.segments; i++)
			page.data_length += page.segment_table[i];
		
		// Page data
		delete page.data; // This should probably be moved somewhere better...
		page.data = new int[page.data_length];
		file->readbytes(page.data, page.data_length);
		
		// Verify the CRC checksum
		if (!verify_CRC_checksum()) {
			cout << "Error: CRC checksum failed!" << endl;
			exit(1);
		}
		
		page.packet.data_offset = 0;
		page.packet.segment_offset = 0;
		
		delete in;
	}
	
	/**
	 * Verifies the CRC checksum of the header
	 * @return True if verified, false if not
	 */
	bool verify_CRC_checksum() {
		int CRC_register = 0;
		
		// Capture pattern
		for (int i=0; i<4; i++)
			CRC_register = (CRC_register<<8) ^ CRC_lookup[((CRC_register >> 24)&0xFF) ^ ((page.capture_pattern >> (i*8))&0xFF)];
		
		// Version
		CRC_register = (CRC_register<<8) ^ CRC_lookup[((CRC_register >> 24)&0xFF) ^ (page.version&0xFF)];
		
		// Header type
		CRC_register = (CRC_register<<8) ^ CRC_lookup[((CRC_register >> 24)&0xFF) ^ (page.header_type&0xFF)];
		
		// Granule position
		for (int i=0; i<8; i++)
			CRC_register = (CRC_register<<8) ^ CRC_lookup[((CRC_register >> 24)&0xFF) ^ (int)((page.granule_position >> (i*8))&0xFF)];
		
		// Bitstream serial number
		for (int i=0; i<4; i++)
			CRC_register = (CRC_register<<8) ^ CRC_lookup[((CRC_register >> 24)&0xFF) ^ ((page.bitstream_serial_number >> (i*8))&0xFF)];
		
		// Page sequence number
		for (int i=0; i<4; i++)
			CRC_register = (CRC_register<<8) ^ CRC_lookup[((CRC_register >> 24)&0xFF) ^ ((page.sequence_number >> (i*8))&0xFF)];
		
		// CRC checksum - zero'd out
		for (int i=0; i<4; i++)
			CRC_register = (CRC_register<<8) ^ CRC_lookup[((CRC_register >> 24)&0xFF)];
		
		// Page segments
		CRC_register = (CRC_register<<8) ^ CRC_lookup[((CRC_register >> 24)&0xFF) ^ (page.segments&0xFF)];
		
		// Segment table
		for (int i=0; i<page.segments; i++)
			CRC_register = (CRC_register<<8) ^ CRC_lookup[((CRC_register >> 24)&0xFF) ^ ((page.segment_table[i])&0xFF)];
		
		// Page data
		for (int i=0; i<page.data_length; i++)
			CRC_register = (CRC_register<<8) ^ CRC_lookup[((CRC_register >> 24)&0xFF) ^ ((page.data[i])&0xFF)];
		
		if (CRC_register == page.CRC_checksum)
			return true;
		else
			return false;
	}
	
	/**
	 * When a new packet (or packet segment) is needed, this function will
	 * set up the Packet structure accordingly
	 */
	void init_vorbis_packet() {
		// If the last packet of the last page didn't span a page boundary,
		// i.e. it ended exactly at the page's end, then we need a new page header
		if (page.packet.segment_offset >= page.segments)
			read_ogg_header();
		
		int len = 0;
		bool end_of_packet = false;
		int i;
		// Get the length of the new packet
		for (i=page.packet.segment_offset; i<page.segments && !end_of_packet; i++) {
			len += page.segment_table[i];
			if (page.segment_table[i] < 255)
				end_of_packet = true;
		}
		page.packet.length = len;
		page.packet.end_of_packet = end_of_packet; // Does this packet end on this page?
		page.packet.segment_offset = i; // How far we've read in the segment table
		
		delete page.packet.data; // This should probably be moved somewhere better...
		page.packet.data = new int[page.packet.length];
		for (int j=0; j<page.packet.length; j++) // Copy over the data
			page.packet.data[j] = page.data[j + page.packet.data_offset];
		
		page.packet.data_offset += page.packet.length; // Update the packet data offset
		
		page.packet.bytepos = 0; // Reset read byte position
		page.packet.bitpos = 0; // Reset read bit position
	}
	
	/**
	 * Reads up to 32 of the next bits from the packet's data
	 * @param bits The number of bits to read in
	 * @return An int containing the bits in the order read from LSB to MSB
	 */
	int readbits(int bits) {
		if (bits > 32) { // We return an int: don't try more than 32 bits
			if (warning) cout << "Warning: Trying to read more than 32 bits!" << endl;
			bits = 32;
		}
		
		int output = 0;
		
		for (int i=0; i<bits; i++) {
			// If we've read to the end of this packet's data
			if (page.packet.bytepos >= page.packet.length) {
				if (!page.packet.end_of_packet) { // Is it a page-spanning packet?
					read_ogg_header(); // Read in the next page
					
					page.packet.data_offset = 0; // Reset data values
					page.packet.segment_offset = 0;
					init_vorbis_packet(); // Set packet data to new page's continuation
				}
				else { // We tried to read past the true end of a packet
					if (warning) cout << "Warning: Tried to read past the end of a packet" << endl;
					break;
				}
			}
			
			output |= ((page.packet.data[page.packet.bytepos] >> page.packet.bitpos)&0x01) << i;
			page.packet.bitpos++;
			
			if (page.packet.bitpos == 8) {
				page.packet.bytepos++;
				page.packet.bitpos = 0;
			}
		}
		
		return output;
	}
	
	/**
	 * Reads values in from the Vorbis ID header packet
	 */
	void read_vorbis_id_header() {
		// Packet type
		int packet_type = readbits(8);
		if (packet_type != 0x01) {
			cout << "Error: Not a Vorbis ID header" << endl;
			exit(1);
		}
		
		// Magic capture pattern "vorbis"
		int vor = readbits(24);
		int bis = readbits(24);
		if (vor != 0x00726f76 && bis != 0x00736962) {
			cout << "Error: Magic capture pattern \"vorbis\" not found" << endl;
			exit(1);
		}
		
		// Vorbis version
		int vorbis_version = readbits(32); // *
		if (vorbis_version != 0) {
			cout << "Error: Only Vorbis version 0 supported" << endl;
			exit(1);
		}
		
		// Audio channels
		info.audio_channels = readbits(8);
		if (info.audio_channels == 0) {
			cout << "Error: Must have at least one audio channel" << endl;
			exit(1);
		}
		
		// Audio sample rate
		info.audio_sample_rate = readbits(32); // *
		if (info.audio_sample_rate == 0) {
			cout << "Error: Sample rate must not be zero" << endl;
			exit(1);
		}
		
		// Bitrate maximum
		info.bitrate_maximum = readbits(32); // *
		
		// Bitrate nominal
		info.bitrate_nominal = readbits(32); // *
		
		// Bitrate minimum
		info.bitrate_minimum = readbits(32); // *
		
		// Blocksize 0
		info.blocksize_0 = 1 << readbits(4);
		bool legal = false;
		for (int i=8; i<14; i++) {
			if ((1<<i) == info.blocksize_0)
				legal = true;
		}
		if (!legal) {
			cout << "Error: Illegal blocksize_0" << endl;
			exit(1);
		}
		
		// Blocksize 1
		info.blocksize_1 = 1 << readbits(4);
		legal = false;
		for (int i=8; i<14; i++) {
			if ((1<<i) == info.blocksize_1)
				legal = true;
		}
		if (!legal) {
			cout << "Error: Illegal blocksize_1" << endl;
			exit(1);
		}
		
		if (info.blocksize_0 > info.blocksize_1) {
			cout << "Error: blocksize_0 must be <= blocksize_1" << endl;
			exit(1);
		}
		
		// Framing flag
		int framing_flag = readbits(1);
		if (framing_flag == 0) {
			cout << "Error: ID framing flag not set" << endl;
			exit(1);
		}
	}
	
	/**
	 * Reads values in from the Vorbis comment header packet
	 */
	void read_vorbis_comment_header() {
		// Packet type
		int packet_type = readbits(8);
		if (packet_type != 0x03) {
			cout << "Error: Not a Vorbis comment header" << endl;
			exit(1);
		}
		
		// Magic capture pattern "vorbis"
		int vor = readbits(24);
		int bis = readbits(24);
		if (vor != 0x00726f76 && bis != 0x00736962) {
			cout << "Error: Magic capture pattern \"vorbis\" not found" << endl;
			exit(1);
		}
		
		// Any of the variables marked with a * might mess up
		// if the read-in value was somehow large enough to reach 2's
		// complement negatives (negative array index).  That is, they
		// are intended to be read as unsigned variables
		
		// Vendor length
		info.vendor_length = readbits(32); // *
		
		// Vendor string
		info.vendor_string = new int[info.vendor_length];
		for (int i=0; i<info.vendor_length; i++)
			info.vendor_string[i] = readbits(8);
		
		// User comment list length
		info.user_comment_list_length = readbits(32); // *
		
		// User comments
		info.user_comment = new int *[info.user_comment_list_length];
		info.user_comment_length = new int[info.user_comment_list_length];
		for (int i=0; i<info.user_comment_list_length; i++) {
			info.user_comment_length[i] = readbits(32); // *
			
			info.user_comment[i] = new int[info.user_comment_length[i]];
			for (int j=0; j<info.user_comment_length[i]; j++)
				info.user_comment[i][j] = readbits(8);
		}
		
		// Framing bit
		int framing_flag = readbits(1);
		if (framing_flag != 1) {
			cout << "Error: Comment framing bit is not set" << endl;
			exit(1);
		}
	}
	
	
	/**
	 * Calculates the base 2 logarithm of an integer in 2's complement
	 * @param x The integer to find the log of
	 * @return The log of x
	 */
	int ilog(int x) {
		int return_value = 0;
		while (x > 0) {
			return_value++;
			x >>= 1;
		}
		
		return return_value;
	}
	
	/**
	 * Unpacks a Vorbis float32 into a native float32
	 * @param x A Vorbis float32
	 * @return The unpacked native float32
	 */
	float float32_unpack(int x) {
		int mantissa = x & 0x1FFFFF; // * Unsigned
		int sign = x & 0x80000000; // * Unsigned
		int exponent = (x & 0x7FE00000) >> 21; // * Unsigned
		if (sign != 0)
			mantissa *= -1;
		return (mantissa * (float)(1 << (exponent - 788)));
	}
	
	/**
	 * Raises a number to an integral power
	 * @param base The base number
	 * @param exp The exponent to be raised to
	 * @return Base raised to the exponent power
	 */
	int int32_pow(int base, int exp) {
		int result = 1;
		for (int i=0; i<exp; i++)
			result *= base;
		return (result);
	}
	
	/**
	 * Computes the correct length of the value index for a
	 * codebook VQ lookup table of lookup type 1
	 * @param entries The number of entries in the codebook
	 * @param dimensions The dimensions of the codebook
	 * @return The correct length
	 */
	int lookup1_values(int entries, int dimensions) {
		int return_value = 0;
		while (int32_pow(return_value, dimensions) <= entries)
			return_value++;
		return (return_value - 1);
	}
	
	/**
	 * Unpack codebooks
	 */
	void unpack_codebooks() {
		// Codebook count
		info.vorbis_codebook_count = readbits(8) + 1; // * Unsigned
		
		// Codebook decode
		info.codebook_config = new Codebook[info.vorbis_codebook_count];
		for (int i=0; i<info.vorbis_codebook_count; i++) {
			// Codebook sync pattern
			int codebook_sync = readbits(24);
			if (codebook_sync != 0x564342) {
				cout << "Error: Codebook sync pattern not found" << endl;
				exit(1);
			}
			
			// Codebook dimensions
			info.codebook_config[i].dimensions = readbits(16);
			
			// Codebook entries
			info.codebook_config[i].entries = readbits(24);
			
			// Ordered flag
			info.codebook_config[i].ordered = readbits(1);
			
			// Codeword lengths
			info.codebook_config[i].codeword_lengths = new int[info.codebook_config[i].entries];
			if (info.codebook_config[i].ordered == 0) { // If the codeword list is not ordered
				info.codebook_config[i].sparse = readbits(1);
				
				for (int j=0; j<info.codebook_config[i].entries; j++) {
					if (info.codebook_config[i].sparse == 1) { // If the codeword list is sparsely populated
						int flag = readbits(1);
						if (flag == 1) // This entry is set
							info.codebook_config[i].codeword_lengths[j] = readbits(5) + 1;
						else // This entry is unused
							info.codebook_config[i].codeword_lengths[j] = -1;
					} else // The codeword list is packed
						info.codebook_config[i].codeword_lengths[j] = readbits(5) + 1;
				}
			} else { // The codeword list is length ordered
				int current_entry = 0;
				int current_length = readbits(5) + 1;
				while (current_entry < info.codebook_config[i].entries) {
					int number = readbits(ilog(info.codebook_config[i].entries - current_entry));
					for (int j=0; j<number; j++)
						info.codebook_config[i].codeword_lengths[j] = current_length;
					current_entry += number;
					current_length++;
					
					if (current_entry >= info.codebook_config[i].entries) { // Spec says >
						cout << "Error: Codebook entry > total entry number" << endl;
						exit(1);
					}
				}
			}
			
			// Codebook lookup type
			info.codebook_config[i].lookup_type = readbits(4);
			
			if (info.codebook_config[i].lookup_type == 0) { // No lookups
				; // Do nothing
			} else if (info.codebook_config[i].lookup_type == 1) { // Type 1
				info.codebook_config[i].minimum_value = float32_unpack(readbits(32)); // *
				info.codebook_config[i].delta_value = float32_unpack(readbits(32)); // *
				info.codebook_config[i].value_bits = readbits(4) + 1;
				info.codebook_config[i].sequence_p = readbits(1);
				
				info.codebook_config[i].lookup_values = lookup1_values(info.codebook_config[i].entries, info.codebook_config[i].dimensions);
				
				info.codebook_config[i].multiplicands = new int[info.codebook_config[i].lookup_values];
				for (int j=0; j<info.codebook_config[i].lookup_values; j++)
					info.codebook_config[i].multiplicands[j] = readbits(info.codebook_config[i].value_bits);
			} else if (info.codebook_config[i].lookup_type == 2) { // Type 2
				info.codebook_config[i].minimum_value = float32_unpack(readbits(32)); // *
				info.codebook_config[i].delta_value = float32_unpack(readbits(32)); // *
				info.codebook_config[i].value_bits = readbits(4) + 1;
				info.codebook_config[i].sequence_p = readbits(1);
				
				info.codebook_config[i].lookup_values = info.codebook_config[i].entries * info.codebook_config[i].dimensions;
				
				info.codebook_config[i].multiplicands = new int[info.codebook_config[i].lookup_values];
				for (int j=0; j<info.codebook_config[i].lookup_values; j++)
					info.codebook_config[i].multiplicands[j] = readbits(info.codebook_config[i].value_bits);
			} else {
				cout << "Error: Illegal codebook lookup type" << endl;
				exit(1);
			}
			
			// Build Huffman tree
			info.codebook_config[i].htree = new HuffmanTree();
			for (int j=0; j<info.codebook_config[i].entries; j++) {
				info.codebook_config[i].htree->AddNode(info.codebook_config[i].codeword_lengths[j], j);
			}
		}
	}
	
	/**
	 * Unpack time domain transforms; this part of the format is unused in this version
	 */
	void unpack_time_domain_transforms() {
		// Placeholder hooks in this version
		info.vorbis_time_count = readbits(6) + 1;
		for (int i=0; i<info.vorbis_time_count; i++) {
			if (readbits(16) != 0) {
				cout << "Error: Nonzero value encountered while unpacking time domain transforms" << endl;
				exit(1);
			}
		}
	}
	
	/**
	 * Unpack floors
	 */
	void unpack_floors() {
		// Floor count
		info.vorbis_floor_count = readbits(6) + 1;
		
		info.floor_config = new Floor1[info.vorbis_floor_count];
		info.vorbis_floor_types = new int[info.vorbis_floor_count];
		for (int i=0; i<info.vorbis_floor_count; i++) {
			// Floor type
			info.vorbis_floor_types[i] = readbits(16);
			
			if (info.vorbis_floor_types[i] == 0)
				cout << "Error: Floor type zero unsupported" << endl;
			else if (info.vorbis_floor_types[i] == 1) {
				// Number of partitions
				info.floor_config[i].partitions = readbits(5);
				
				// Partition class list and maximum value therein
				info.floor_config[i].partition_class_list = new int[info.floor_config[i].partitions];
				info.floor_config[i].maximum_class = -1;
				for (int j=0; j<info.floor_config[i].partitions; j++) {
					info.floor_config[i].partition_class_list[j] = readbits(4);
					if (info.floor_config[i].partition_class_list[j] > info.floor_config[i].maximum_class)
						info.floor_config[i].maximum_class = info.floor_config[i].partition_class_list[j];
				}
				
				info.floor_config[i].class_dimensions = new int[info.floor_config[i].maximum_class + 1];
				info.floor_config[i].class_subclasses = new int[info.floor_config[i].maximum_class + 1];
				info.floor_config[i].class_masterbooks = new int[info.floor_config[i].maximum_class + 1];
				info.floor_config[i].subclass_books = new int*[info.floor_config[i].maximum_class + 1];
				for (int j=0; j<=info.floor_config[i].maximum_class; j++) {
					// Class dimensions and number of subclasses
					info.floor_config[i].class_dimensions[j] = readbits(3) + 1;
					info.floor_config[i].class_subclasses[j] = readbits(2);
					
					// Masterbooks
					if (info.floor_config[i].class_subclasses[j] != 0) {
						info.floor_config[i].class_masterbooks[j] = readbits(8);
						if (info.floor_config[i].class_masterbooks[j] > info.vorbis_codebook_count) {
							cout << "Error: Floor 1 masterbook greater than valid codebook" << endl;
							exit(1);
						}
					}
					
					// Subclass books
					info.floor_config[i].subclass_books[j] = new int[1 << info.floor_config[i].class_subclasses[j]];
					for (int k=0; k<(1<<info.floor_config[i].class_subclasses[j]); k++) {
						info.floor_config[i].subclass_books[j][k] = readbits(8) - 1;
						if (info.floor_config[i].subclass_books[j][k] > info.vorbis_codebook_count) {
							cout << "Error: Floor 1 subclass book greater than valid codebook" << endl;
							exit(1);
						}
					}
				}
				
				// Multiplier
				info.floor_config[i].multiplier = readbits(2) + 1;
				
				// Range bits
				info.floor_config[i].rangebits = readbits(4);
				
				// Get number of floor values
				info.floor_config[i].floor1_values = 2;
				for (int j=0; j<info.floor_config[i].partitions; j++) {
					int current_class_number = info.floor_config[i].partition_class_list[j];
					for (int k=0; k<info.floor_config[i].class_dimensions[current_class_number]; k++) {
						info.floor_config[i].floor1_values++;
					}
				}
				
				// Read in the X coordinates
				info.floor_config[i].X_list = new int[info.floor_config[i].floor1_values];
				info.floor_config[i].X_list[0] = 0;
				info.floor_config[i].X_list[1] = 1 << info.floor_config[i].rangebits;
				info.floor_config[i].floor1_values = 2;
				for (int j=0; j<info.floor_config[i].partitions; j++) {
					int current_class_number = info.floor_config[i].partition_class_list[j];
					for (int k=0; k<info.floor_config[i].class_dimensions[current_class_number]; k++) {
						info.floor_config[i].X_list[info.floor_config[i].floor1_values] = readbits(info.floor_config[i].rangebits);
						info.floor_config[i].floor1_values++;
					}
				}
			}
			else {
				cout << "Error: Invalid floor type" << endl;
				exit(1);
			}
		}
	}
	
	/**
	 * Unpack residues
	 */
	void unpack_residues() {
		// Residue count
		info.vorbis_residue_count = readbits(6) + 1;
		
		info.vorbis_residue_types = new int[info.vorbis_residue_count];
		info.residue_config = new Residue[info.vorbis_residue_count];
		for (int i=0; i<info.vorbis_residue_count; i++) {
			// Residue type
			info.vorbis_residue_types[i] = readbits(16);
			
			if (info.vorbis_residue_types[i] == 0 || info.vorbis_residue_types[i] == 1 || info.vorbis_residue_types[i] == 2) {
				// "Bandpass" begin
				info.residue_config[i].begin = readbits(24);
				
				// "Bandpass" end
				info.residue_config[i].end = readbits(24);
				
				// Partition size
				info.residue_config[i].partition_size = readbits(24) + 1;
				
				// Classifications
				info.residue_config[i].classifications = readbits(6) + 1;
				
				// Classbook
				info.residue_config[i].classbook = readbits(8);
				
				// Cascade vector
				info.residue_config[i].cascade = new int[info.residue_config[i].classifications];
				for (int j=0; j<info.residue_config[i].classifications; j++) {
					int high_bits = 0;
					int low_bits = readbits(3);
					int bitflag = readbits(1);
					
					if (bitflag == 1)
						high_bits = readbits(5);
					
					info.residue_config[i].cascade[j] = high_bits * 8 + low_bits;
				}
				
				// Book numbers
				info.residue_config[i].books = new int*[info.residue_config[i].classifications];
				for (int j=0; j<info.residue_config[i].classifications; j++) {
					info.residue_config[i].books[j] = new int[8];
					for (int k=0; k<8; k++) {
						// If bit k of cascade[j] is set
						if (((info.residue_config[i].cascade[j] >> k) & 0x01) == 1) {
							info.residue_config[i].books[j][k] = readbits(8);
							if (info.residue_config[i].books[j][k] > info.vorbis_codebook_count) {
								cout << "Error: Residue book greater than valid codebook" << endl;
								exit(1);
							}
						}
						else
							info.residue_config[i].books[j][k] = -1;
					}
				}
			} else {
				cout << "Error: Invalid residue type" << endl;
				exit(1);
			}
		}
	}
	
	/**
	 * Unpack mappings
	 */
	void unpack_mappings() {
		// Mapping count
		info.vorbis_mapping_count = readbits(6) + 1;
		
		info.mapping_config = new Mapping[info.vorbis_mapping_count];
		for (int i=0; i<info.vorbis_mapping_count; i++) {
			int mapping_type = readbits(16);
			if (mapping_type == 0) {
				// Submaps
				if (readbits(1) == 1)
					info.mapping_config[i].submaps = readbits(4) + 1;
				else
					info.mapping_config[i].submaps = 1;
				
				// Magnitude and angle vectors
				if (readbits(1) == 1) {
					info.mapping_config[i].coupling_steps = readbits(8) + 1;
					info.mapping_config[i].magnitude = new int[info.mapping_config[i].coupling_steps];
					info.mapping_config[i].angle = new int[info.mapping_config[i].coupling_steps];
					for (int j=0; j<info.mapping_config[i].coupling_steps; j++) {
						info.mapping_config[i].magnitude[j] = readbits(ilog(info.audio_channels - 1));
						info.mapping_config[i].angle[j] = readbits(ilog(info.audio_channels - 1));
						
						// Errors
						if (info.mapping_config[i].magnitude[j] == info.mapping_config[i].angle[j]) {
							cout << "Error: Mapping magnitude and angle channels are identical" << endl;
							exit(1);
						}
						
						if (info.mapping_config[i].magnitude[j] >= info.audio_channels) {
							cout << "Error: Mapping magnitude greater than valid audio channel" << endl;
							exit(1);
						}
						
						if (info.mapping_config[i].angle[j] >= info.audio_channels) {
							cout << "Error: Mapping angle greater than valid audio channel" << endl;
							exit(1);
						}
					}
				} else {
					info.mapping_config[i].coupling_steps = 0;
				}
				
				// Reserved field
				if (readbits(2) != 0) {
					cout << "Error: Nonzero reserved mapping field" << endl;
					exit(1);
				}
				
				// Channel multiplex settings
				info.mapping_config[i].mux = new int[info.audio_channels];
				if (info.mapping_config[i].submaps > 1) {
					for (int j=0; j<info.audio_channels; j++) {
						info.mapping_config[i].mux[j] = readbits(4);
						if (info.mapping_config[i].mux[j] >= info.mapping_config[i].submaps) {
							cout << "Error: Mapping mux greater than valid submap" << endl;
							exit(1);
						}
					}
				} else {
					for (int j=0; j<info.audio_channels; j++)
						info.mapping_config[i].mux[j] = 0;
				}
				
				info.mapping_config[i].submap_floor = new int[info.mapping_config[i].submaps];
				info.mapping_config[i].submap_residue = new int[info.mapping_config[i].submaps];
				for (int j=0; j<info.mapping_config[i].submaps; j++) {
					// Unused time configuration placeholder
					readbits(8);
					
					// Submap floor
					info.mapping_config[i].submap_floor[j] = readbits(8);
					if (info.mapping_config[i].submap_floor[j] >= info.vorbis_floor_count) {
						cout << "Error: Mapping submap floor greater than valid floor" << endl;
						exit(1);
					}
					
					// Submap residue
					info.mapping_config[i].submap_residue[j] = readbits(8);
					if (info.mapping_config[i].submap_residue[j] >= info.vorbis_residue_count) {
						cout << "Error: Mapping submap residue greater than valid residue" << endl;
						exit(1);
					}
				}
			} else {
				cout << "Error: Invalid mapping type" << endl;
				exit(1);
			}
		}
	}
	
	/**
	 * Unpack modes
	 */
	void unpack_modes() {
		info.vorbis_mode_count = readbits(6) + 1;
		
		info.mode_config = new Mode[info.vorbis_mode_count];
		for (int i=0; i<info.vorbis_mode_count; i++) {
			// Block flag
			info.mode_config[i].blockflag = readbits(1);
			
			// Window type
			info.mode_config[i].windowtype = readbits(16);
			if (info.mode_config[i].windowtype != 0) {
				cout << "Error: Nonzero mode window type" << endl;
				exit(1);
			}
			
			// Transform type
			info.mode_config[i].transformtype = readbits(16);
			if (info.mode_config[i].transformtype != 0) {
				cout << "Error: Nonzero mdoe transform type" << endl;
				exit(1);
			}
			
			// Mode mapping
			info.mode_config[i].mode_mapping = readbits(8);
			if (info.mode_config[i].mode_mapping >= info.vorbis_mapping_count) {
				cout << "Error: Mode mapping greater than valid mapping" << endl;
				exit(1);
			}
		}
		
		int framing_flag = readbits(1);
		if (framing_flag != 1) {
			cout << "Error: Framing error at end of setup header" << endl;
			exit(1);
		}
	}
	
	/**
	 * Reads values in from the Vorbis setup header packet
	 */
	void read_vorbis_setup_header() {
		// Packet type
		int packet_type = readbits(8);
		if (packet_type != 0x05) {
			cout << "Error: Not a Vorbis setup header" << endl;
			exit(1);
		}
		
		// Magic capture pattern "vorbis"
		int vor = readbits(24);
		int bis = readbits(24);
		if (vor != 0x00726f76 && bis != 0x00736962) {
			cout << "Error: Magic capture pattern \"vorbis\" not found" << endl;
			exit(1);
		}
		
		unpack_codebooks();
		unpack_time_domain_transforms();
		unpack_floors();
		unpack_residues();
		unpack_mappings();
		unpack_modes();
	}
	
	/**
	 * Read bits in and find the entry corresponding to the bit pattern
	 * @param book The codebook to search
	 * @return The entry corresponding to the first recognized bit pattern
	 */
	int decode_codebook_scalar(int book) {
		HuffmanNode *node = info.codebook_config[book].htree->root;
		
		while (node->entry == -1) {
			// Descend accordingly
			if (readbits(1) == 0)
				node = node->lchild;
			else
				node = node->rchild;
			
			// We should never encounter a NULL node
			if (node == NULL) {
				cout << "Error: NULL node encountered while decoding bit pattern using codebook" << endl;
				exit(1);
			}
		}
		
		return node->entry;
	}
	
	/**
	 * Decodes a vector from the bitstream
	 * @param book The codebook to use
	 * @return A vector of the constructed values
	 */
	double *decode_codebook_VQ(int book) {
		int lookup_offset = decode_codebook_scalar(book);
		double last = 0;
		int index_divisor = 1;
		
		Codebook *codebook = &info.codebook_config[book];
		
		int multiplicand_offset;
		if (codebook->lookup_type == 2)
			multiplicand_offset = lookup_offset * codebook->dimensions;
		
		double *value_vector = new double[codebook->dimensions];
		for (int i=0; i<codebook->dimensions; i++) {
			if (codebook->lookup_type == 1)
				multiplicand_offset = (lookup_offset / index_divisor) % codebook->lookup_values;
			
			value_vector[i] = codebook->multiplicands[multiplicand_offset] * codebook->delta_value + codebook->minimum_value + last;
			
			if (codebook->sequence_p == 1)
				last = value_vector[i];
			
			if (codebook->lookup_type == 2)
				multiplicand_offset++;
			else
				index_divisor *= codebook->lookup_values;
		}
		
		return value_vector;
	}
	
	/**
	 * Finds the position n in vector [v] of the greatest value scalar element for which
	 * n is less than [x] and vector [v] element n is less than vector [v] element [x]
	 * @param v The vector to search
	 * @param x The position to compare to
	 * @return The low neighbor position
	 */
	int low_neighbor(int *v, int x) {
		int max_value = v[0];
		int n = 0;
		for (int i=0; i<x; i++) {
			if (v[i] > max_value && v[i] < v[x]) {
				max_value = v[i];
				n = i;
			}
		}
		
		return n;
	}
	
	/**
	 * Finds the position n in vector [v] of the lowest value scalar element for which n
	 * is less than [x] and vector [v] element n is greater than vector [v] element [x]
	 * @param v The vector to search
	 * @param x The position to compare to
	 * @return The high neighbor position
	 */
	int high_neighbor(int *v, int x) {
		int min_value = v[1];
		int n = 1;
		for (int i=0; i<x; i++) {
			if (v[i] < min_value && v[i] > v[x]) {
				min_value = v[i];
				n = i;
			}
		}
		
		return n;
	}
	
	/**
	 * Finds the Y value at point X along the line specified by x0, x1, y0 and y1; this function
	 * uses an integer algorithm to solve for the point directly without calculating intervening
	 * values along the line.
	 * @param x0
	 * @param y0
	 * @param x1
	 * @param y1
	 * @param X
	 * @return 
	 */
	int render_point(int x0, int y0, int x1, int y1, int X) {
		int dy = y1 - y0;
		int adx = x1 - x0;
		int ady = dy;
		if (ady < 0)
			ady *= -1;
		int err = ady * (X - x0);
		int off = err / adx;
		
		int Y;
		if (dy < 0)
			Y = y0 - off;
		else
			Y = y0 + off;
		
		return Y;
	}
	
	void render_line(int x0, int y0, int x1, int y1, int* v) {
		int dy = y1 - y0;
		int adx = x1 - x0;
		int ady = dy;
		if (ady < 0)
			ady *= -1;
		int base = dy / adx;
		int x = x0;
		int y = y0;
		int err = 0;
		
		int sy;
		if (dy < 0)
			sy = base - 1;
		else
			sy = base + 1;
		
		int abase = base;
		if (abase < 0)
			abase *=-1;
		ady -= abase * adx;
		v[x] = y;
		
		for (++x; x<x1; x++) {
			err += ady;
			if (err >= adx) {
				err -= adx;
				y += sy;
			} else
				y += base;
				v[x] = y;
		}
	}
	
	/**
	 * Floor decode and synthesis
	 */
	void decode_floors() {
		audio.no_residue = new int[info.audio_channels];
		
		audio.floor_out = new double*[info.audio_channels];
		for (int i=0; i<info.audio_channels; i++) {
			int submap_number = audio.mapping->mux[i];
			int floor_number = audio.mapping->submap_floor[submap_number];
			Floor1 *floor1 = &info.floor_config[floor_number];
			
			if (info.vorbis_floor_types[floor_number] == 0) {
				cout << "Error: Floor type 0 unsupported" << endl;
				exit(1);
			} else if (info.vorbis_floor_types[floor_number] == 1) {
				int nonzero = readbits(1);
				if (nonzero == 0) {
					// There's no audio energy in this frame for this channel
					audio.no_residue[i] = 1;
					
					// Even so, we need a zero'd floor vector
					audio.floor_out[i] = new double[audio.n / 2];
					for (int j=0; j<audio.n/2; j++)
						audio.floor_out[i][j] = 0;
				} else {
					audio.no_residue[i] = 0;
					
					// Range
					int rangev[] = {256, 128, 86, 64};
					int range = rangev[floor1->multiplier-1];
					
					// Populate Y values
					int *floor1_Y = new int[floor1->floor1_values];
					floor1_Y[0] = readbits(ilog(range-1));
					floor1_Y[1] = readbits(ilog(range-1));
					int offset = 2;
					for (int j=0; j<floor1->partitions; j++) {
						int clas = floor1->partition_class_list[j];
						int cdim = floor1->class_dimensions[clas];
						int cbits = floor1->class_subclasses[clas];
						int csub = (1 << cbits) - 1;
						
						int cval = 0;
						if (cbits > 0)
							cval = decode_codebook_scalar(floor1->class_masterbooks[clas]);
						
						for (int k=0; k<cdim; k++) {
							int book = floor1->subclass_books[clas][cval&csub];
							cval >>= cbits;
							if (book >= 0)
								floor1_Y[k+offset] = decode_codebook_scalar(book);
							else
								floor1_Y[k+offset] = 0;
						}
						offset += cdim;
					}
					
					// Amplitude value synthesis
					int *floor1_step2_flag = new int[floor1->floor1_values];
					floor1_step2_flag[0] = 1;
					floor1_step2_flag[1] = 1;
					int *floor1_final_Y = new int[floor1->floor1_values];
					floor1_final_Y[0] = floor1_Y[0];
					floor1_final_Y[1] = floor1_Y[1];
					for (int j=2; j<floor1->floor1_values; j++) {
						int low_neighbor_offset = low_neighbor(floor1->X_list, j);
						int high_neighbor_offset = high_neighbor(floor1->X_list, j);
						int predicted = render_point(floor1->X_list[low_neighbor_offset], 
								floor1_final_Y[low_neighbor_offset],
								floor1->X_list[high_neighbor_offset],
								floor1_final_Y[high_neighbor_offset],
								floor1->X_list[j]);
						int val = floor1_Y[j];
						int highroom = range - predicted;
						int lowroom = predicted;
						int room;
						if (highroom < lowroom)
							room = highroom * 2;
						else
							room = lowroom * 2;
						
						if (val != 0) {
							floor1_step2_flag[low_neighbor_offset] = 1;
							floor1_step2_flag[high_neighbor_offset] = 1;
							floor1_step2_flag[j] = 1;
							
							if (val >= room) {
								if (highroom > lowroom)
									floor1_final_Y[j] = val - lowroom + predicted;
								else
									floor1_final_Y[j] = predicted - val + highroom - 1;
							} else {
								if ((val&0x01) == 1)
									floor1_final_Y[j] = predicted - ((val+1) / 2);
								else
									floor1_final_Y[j] = predicted + val/2;
							}
						} else {
							floor1_step2_flag[j] = 0;
							floor1_final_Y[j] = predicted;
						}
					}
					
					// Sort the three vectors according to ascending X_list
					int *X_list_sort = new int[floor1->floor1_values];
					for (int j=0; j<floor1->floor1_values; j++)
						X_list_sort[j] = floor1->X_list[j];
					for (int j=0; j<floor1->floor1_values; j++) { // This is a vvvvveeeeerrrrryyyyy slow sort
						int min_value = X_list_sort[j];
						int min_pos = j;
						for (int k=j; k<floor1->floor1_values; k++) {
							if (X_list_sort[k] < min_value) {
								min_value = X_list_sort[k];
								min_pos = k;
							}
						}
						
						int tmp = X_list_sort[min_pos];
						X_list_sort[min_pos] = X_list_sort[j];
						X_list_sort[j] = tmp;
						
						tmp = floor1_final_Y[min_pos];
						floor1_final_Y[min_pos] = floor1_final_Y[j];
						floor1_final_Y[j] = tmp;
						
						tmp = floor1_step2_flag[min_pos];
						floor1_step2_flag[min_pos] = floor1_step2_flag[j];
						floor1_step2_flag[j] = tmp;
					}
					
					// Curve synthesis
					int *floor_out_int = new int[audio.n / 2];
					int hx = 0;
					int hy = 0;
					int lx = 0;
					int ly = floor1_final_Y[0] * floor1->multiplier;
					for (int j=1; j<floor1->floor1_values; j++) {
						if (floor1_step2_flag[j] == 1) {
							hy = floor1_final_Y[j] * floor1->multiplier;
							hx = X_list_sort[j];
							render_line(lx, ly, hx, hy, floor_out_int);
							lx = hx;
							ly = hy;
						}
					}
					
					if (hx < audio.n / 2)
						render_line(hx, hy, audio.n / 2, hy, floor_out_int);
					if (hx > audio.n / 2)
						if (warning) cout << "Warning: hx > n / 2; floor_out should be truncated" << endl;
					
					audio.floor_out[i] = new double[audio.n / 2];
					for (int j=0; j<audio.n/2; j++)
						audio.floor_out[i][j] = floor1_inverse_dB_table[floor_out_int[j]];
					
					// Cleanup
					delete floor_out_int;
					delete X_list_sort;
					delete floor1_final_Y;
					delete floor1_step2_flag;
					delete floor1_Y;
				}
			}
		}
	}
	
	/**
	 * Residue decode
	 */
	void decode_residues() {
		int *do_not_decode_flag = new int[info.audio_channels];
		for (int i=0; i<info.audio_channels; i++)
			do_not_decode_flag[i] = 0;
		
		for (int i=0; i<audio.mapping->submaps; i++) {
			int ch = 0;
			for (int j=0; j<info.audio_channels; j++) {
				if (audio.mapping->mux[j] == i) {
					if (audio.no_residue[j] == 1)
						do_not_decode_flag[ch] = 1;
					else
						do_not_decode_flag[ch] = 0;
					ch++;
				}
			}
			
			int residue_number = audio.mapping->submap_residue[i];
			int residue_type = info.vorbis_residue_types[residue_number];
			Residue *residue = &info.residue_config[residue_number];
			
			// Limit residue sizes
			int actual_size = audio.n / 2;
			if (residue_type == 2) {
				actual_size *= ch;
				ch = 1;
			}
			int limit_residue_begin = residue->begin;
			int limit_residue_end;
			if (residue->end < actual_size)
				limit_residue_end = residue->end;
			else
				limit_residue_end = actual_size;
			
			// Convenience values
			int classwords_per_codeword = info.codebook_config[residue->classbook].dimensions;
			int n_to_read = limit_residue_end - limit_residue_begin;
			int partitions_to_read = n_to_read / residue->partition_size;
			
			// Allocate all returned vectors
			double **decoded = new double*[info.audio_channels];
			audio.residue_out = new double*[info.audio_channels];
			for (int j=0; j<info.audio_channels; j++) {
				decoded[j] = new double[actual_size];
				
				for (int k=0; k<actual_size; k++) // Zero it
					decoded[j][k] = 0;
			}
			
			// Decode vectors
			if (n_to_read != 0) {
				int **classifications = new int*[ch];
				for (int j=0; j<ch; j++) {
					classifications[j] = new int[classwords_per_codeword + partitions_to_read];
					for (int k=0; k<classwords_per_codeword + partitions_to_read; k++)
						classifications[j][k] = 0;
				}
				
				for (int pass=0; pass<8; pass++) {
					int partition_count = 0;
					while (partition_count < partitions_to_read) {
						if (pass == 0) {
							for (int j=0; j<ch; j++) {
								if (do_not_decode_flag[j] == 0) {
									int temp = decode_codebook_scalar(residue->classbook);
									for (int k=classwords_per_codeword-1; k>=0; k--) {
										classifications[j][k+partition_count] = temp % residue->classifications;
										temp /= residue->classifications;
									}
								}
							}
						}
						
						for (int j=0; (j<classwords_per_codeword) && (partition_count<partitions_to_read); j++) {
							for (int k=0; k<ch; k++) {
								if (do_not_decode_flag[k] == 0) {
									int vqclass = classifications[k][partition_count];
									int vqbook = residue->books[vqclass][pass];
									if (vqbook != -1) {
										int n = residue->partition_size;
										int offset = limit_residue_begin + partition_count * n;
										if (residue_type == 0) {
											int step = n / info.codebook_config[vqbook].dimensions;
											
											for (int l=0; l<step; l++) {
												double *entry_temp = decode_codebook_VQ(vqbook);
												for (int m=0; m<info.codebook_config[vqbook].dimensions; m++)
													decoded[k][offset + l + m*step] += entry_temp[m];
												delete entry_temp;
											}
										}
										else if (residue_type == 1) {
											int l = 0;
											while (l < n) {
												double *entry_temp = decode_codebook_VQ(vqbook);
												for (int m=0; m<info.codebook_config[vqbook].dimensions; m++) {
													decoded[k][offset + l] += entry_temp[m];
													l++;
												}
												delete entry_temp;
											}
										} else if (residue_type == 2) {
											// Pre-step
											bool decode = false;
											for (int a=0; a<info.audio_channels; a++) {
												if (do_not_decode_flag[a] == 0) {
													decode = true;
													break;
												}
											}
											
											double *interleave = new double[actual_size];
											for (int l=0; l<actual_size; l++) // Zero it
												interleave[l] = 0;
											
											if (decode) {
												// Type 1 decode
												int l = 0;
												while (l < n) {
													double *entry_temp = decode_codebook_VQ(vqbook);
													for (int m=0; m<info.codebook_config[vqbook].dimensions; m++) {
														interleave[offset + l] += entry_temp[m];
														l++;
													}
													delete entry_temp;
												}
											}
											
											// Post-step
											for (int a=0; a<audio.n/2; a++) {
												for (int b=0; b<info.audio_channels; b++)
													decoded[b][a] += interleave[a*info.audio_channels + b];
											}
											delete interleave;
										}
									}
								}
								partition_count++;
							}
						}
					}
				}
				
				for (int j=0; j<ch; j++)
					delete classifications[j];
				delete classifications;
			}
			
			// Correctly assign data to channels
			ch = 0;
			for (int j=0; j<info.audio_channels; j++) {
				if (audio.mapping->mux[j] == i) {
					audio.residue_out[j] = decoded[ch];
					ch++;
				}
			}
		}
		
		delete do_not_decode_flag;
	}
	
	/* partial; doesn't perform last-step deinterleave/unrolling.  That
   can be done more efficiently during pcm output */
void mdct_backward(int n, DATA_TYPE *in){
	int shift;
	int step;
  
	for (shift=4;!(n&(1<<shift));shift++);
	shift=13-shift;
	step=2<<shift;
   
	presymmetry(in,n>>1,step);
	mdct_butterflies(in,n>>1,shift);
	mdct_bitreverse(in,n,shift);
	mdct_step7(in,n,step);
	mdct_step8(in,n,step);
}

void mdct_shift_right(int n, DATA_TYPE *in, DATA_TYPE *right){
	int i;
	n>>=2;
	in+=1;

	for(i=0;i<n;i++)
		right[i]=in[i<<1];
}

void mdct_unroll_lap(int n0,int n1,
						int lW,int W,
						DATA_TYPE *in,
						DATA_TYPE *right,
						LOOKUP_T *w0,
						LOOKUP_T *w1,
						ogg_int16_t *out,
						int step,
						int start, /* samples, this frame */
						int end    /* samples, this frame */) {

	   DATA_TYPE *l=in+(W&&lW ? n1>>1 : n0>>1);
	   DATA_TYPE *r=right+(lW ? n1>>2 : n0>>2);
	   DATA_TYPE *post;
	   LOOKUP_T *wR=(W && lW ? w1+(n1>>1) : w0+(n0>>1));
	   LOOKUP_T *wL=(W && lW ? w1         : w0        );

	   int preLap=(lW && !W ? (n1>>2)-(n0>>2) : 0 );
	   int halfLap=(lW && W ? (n1>>2) : (n0>>2) );
	   int postLap=(!lW && W ? (n1>>2)-(n0>>2) : 0 );
	   int n,off;

	   /* preceeding direct-copy lapping from previous frame, if any */
	   if(preLap){
		   n      = (end<preLap?end:preLap);
		   off    = (start<preLap?start:preLap);
		   post   = r-n;
		   r     -= off;
		   start -= off;
		   end   -= n;
		   while(r>post){
			   *out = CLIP_TO_15((*--r)>>9);
			   out+=step;
		   }
	   }
  
	   /* cross-lap; two halves due to wrap-around */
	   n      = (end<halfLap?end:halfLap);
	   off    = (start<halfLap?start:halfLap);
	   post   = r-n;
	   r     -= off;
	   l     -= off*2;
	   start -= off;
	   wR    -= off;
	   wL    += off;
	   end   -= n;
	   while(r>post){
		   l-=2;
		   *out = CLIP_TO_15((MULT31(*--r,*--wR) + MULT31(*l,*wL++))>>9);
		   out+=step;
	   }

	   n      = (end<halfLap?end:halfLap);
	   off    = (start<halfLap?start:halfLap);
	   post   = r+n;
	   r     += off;
	   l     += off*2;
	   start -= off;
	   end   -= n;
	   wR    -= off;
	   wL    += off;
	   while(r<post){
		   *out = CLIP_TO_15((MULT31(*r++,*--wR) - MULT31(*l,*wL++))>>9);
		   out+=step;
		   l+=2;
	   }

	   /* preceeding direct-copy lapping from previous frame, if any */
	   if(postLap){
		   n      = (end<postLap?end:postLap);
		   off    = (start<postLap?start:postLap);
		   post   = l+n*2;
		   l     += off*2;
		   while(l<post){
			   *out = CLIP_TO_15((-*l)>>9);
			   out+=step;
			   l+=2;
		   }
	   }
   }
	
	void imdct(int *in, int n) {;
		mdct_backward(n, in);
	}
	
	/**
	 * Decode an audio packet
	 */
	void decode_audio() {
		// Packet type
		int packet_type = readbits(1);
		if (packet_type != 0) {
			if (warning)
				cout << "Warning: Non-audio packet found where audio packet expected" << endl;
		} else {
			// Mode number
			int mode_number = readbits(ilog(info.vorbis_mode_count - 1));
			audio.mode = &info.mode_config[mode_number];
			audio.mapping = &info.mapping_config[audio.mode->mode_mapping];
			
			if (!first_packet)
				audio.last_n = audio.n;
			else
				audio.last_n = 0;
			
			// Decode blocksize
			if (audio.mode->blockflag == 0)
				audio.n = info.blocksize_0;
			else
				audio.n = info.blocksize_1;
			
			// Window selection
			audio.previous_window_flag = 0;
			audio.next_window_flag = 0;
			if (audio.mode->blockflag == 1) {
				audio.previous_window_flag = readbits(1);
				audio.next_window_flag = readbits(1);
			}
			
			// Window setup
			audio.window_center = audio.n/2;
			if (audio.mode->blockflag == 1 && audio.previous_window_flag == 0) {
				audio.left_window_start = audio.n/4 - info.blocksize_0/4;
				audio.left_window_end = audio.n/4 + info.blocksize_0/4;
				audio.left_n = info.blocksize_0/2;
			} else {
				audio.left_window_start = 0;
				audio.left_window_end = audio.window_center;
				audio.left_n = audio.n/2;
			}
			
			if (audio.mode->blockflag == 1 && audio.next_window_flag == 0) {
				audio.right_window_start = audio.n*3/4 - info.blocksize_0/4;
				audio.right_window_end = audio.n*3/4 + info.blocksize_0/4;
				audio.right_n = info.blocksize_0/2;
			} else {
				audio.right_window_start = audio.window_center;
				audio.right_window_end = audio.n;
				audio.right_n = audio.n/2;
			}
			
			decode_floors();
			
			// Nonzero vector propagate: match magnitude and angle if either one is 'unused'
			for (int i=0; i<audio.mapping->coupling_steps; i++) {
				int mag = audio.mapping->magnitude[i];
				int ang = audio.mapping->angle[i];
				if (audio.no_residue[mag] == 0 || audio.no_residue[ang] == 0) {
					audio.no_residue[mag] = 0;
					audio.no_residue[ang] = 0;
				}
			}
			
			decode_residues();
			
			// Inverse coupling
			for (int i=audio.mapping->coupling_steps-1; i>=0; i--) {
				double *magnitude_vector = audio.residue_out[audio.mapping->magnitude[i]];
				double *angle_vector = audio.residue_out[audio.mapping->angle[i]];
				for (int j=0; j<audio.n/2; j++) {
					double M = magnitude_vector[j];
					double A = angle_vector[j];
					double new_M;
					double new_A;
					
					if (M > 0) {
						if (A > 0) {
							new_M = M;
							new_A = M - A;
						} else {
							new_A = M;
							new_M = M + A;
						}
					} else {
						if (A > 0) {
							new_M = M;
							new_A = M + A;
						} else {
							new_A = M;
							new_M = M - A;
						}
					}
					
					magnitude_vector[j] = new_M;
					angle_vector[j] = new_A;
				}
			}
			
			// Allocate space for spectrum data
			audio.spectrum = new int*[info.audio_channels];
			for (int i=0; i<info.audio_channels; i++)
				audio.spectrum[i] = new int[audio.n/2];
			
			// Dot product
			for (int i=0; i<info.audio_channels; i++) {
				for (int j=0; j<audio.n/2; j++)
					audio.spectrum[i][j] = (int)(audio.floor_out[i][j] * audio.residue_out[i][j] / 256);
			}
			
			// IMDCT
			for (int i=0; i<info.audio_channels; i++)
				imdct(audio.spectrum[i], audio.n);
			
			/** Begin stolen */
			int samples = 4096/2 / info.audio_channels;
			int out_begin = 0;
			int out_end = 0;
			
			if (!first_packet)
				out_end = audio.n/4 + audio.last_n/4;
			else
				first_packet = false;
			
			if(out_begin>-1 && out_begin<out_end){
				int n=out_end-out_begin;
				if(pcmout){
					int i;
					if(n>samples)n=samples;
					for(i=0;i<info.audio_channels;i++)
						mdct_unroll_lap(info.blocksize_0,
										info.blocksize_1,
										(audio.last_n==info.blocksize_1),
										(audio.n==info.blocksize_1),
										audio.spectrum[i],
										mdctright[i],
										_vorbis_window(info.blocksize_0/2),
										_vorbis_window(info.blocksize_1/2),
										(short *)pcmout+i,
										info.audio_channels,
										out_begin,
										out_begin+n);
				}
				fwrite(pcmout, 1, n*info.audio_channels*2, stdout);
			}
			for(int i=0;i<info.audio_channels;i++)
				mdct_shift_right(audio.n,audio.spectrum[i],mdctright[i]);
			
			/** End stolen*/
			
			// Cleanup
			for (int i=0; i<info.audio_channels; i++)
				delete audio.floor_out[i];
			delete audio.floor_out;
			
			delete audio.residue_out;
			
			delete audio.no_residue;
			
			for (int i=0; i<info.audio_channels; i++)
				delete audio.spectrum[i];
			delete audio.spectrum;
		}
	}
};

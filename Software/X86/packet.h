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

#ifndef PACKET_H
#define PACKET_H

class Packet {
	public:
		/** packet data */
		int *data;
		/** The offset within the data where the packet begins */
		int data_offset;
		/** The offset in the segment table where the next packet's length info resides */
		int segment_offset;
		/** The length of the packet */
		int length;
		/** Byte position within the packet */
		int bytepos;
		/** Bit position */
		int bitpos;
		/** Does this packet span a page boundary? False if yes. */
		bool end_of_packet;
};

#endif

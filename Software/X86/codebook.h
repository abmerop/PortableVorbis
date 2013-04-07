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

#ifndef CODEBOOK_H
#define CODEBOOK_H

#include "huffmantree.h"

class Codebook {
	public:
		/** Codebook dimensions */
		int dimensions;
		/** Number of entries ("codewords") in this codebook */
		int entries;
		/** Is this codebook ordered? */
		int ordered;
		/** Is this codebook sparse? */
		int sparse;
		/** Array of codeword lengths */
		int *codeword_lengths;
		/** Lookup type */
		int lookup_type;
		/** Minimum value */
		float minimum_value;
		/** Delta value */
		float delta_value;
		/** Value bits */
		int value_bits;
		/** Sequence p */
		int sequence_p;
		/** Lookup values */
		int lookup_values;
		/** Multiplicands array */
		int *multiplicands;
	
		/** Huffman decoder tree */
		HuffmanTree *htree;
};

#endif

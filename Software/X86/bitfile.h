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

#ifndef BITFILE_H
#define BITFILE_H

#include <iostream>
#include <cstdlib>

using namespace std;

class BitFile {
	public:
		/** Stream to the input file */
		FILE *infile;
		/** The currently read-in byte for bitstreams */
		int curbyte;
		/** The bit position, from LSB to MSB, in the current byte for bitstreams */
		int bitpos;
		/** The current byte number in the file */
		int bytenum;
		/** End of file? */
		bool eof;
		
		/**
		* Constructor to initialize the FileInputStream
		* @param filename The name of the file to open and read
		*/
		BitFile(char *filename) {
			infile = fopen(filename, "rb"); // Initialize
				
			if (!infile) {
				cout << "Unable to open file!" << endl;
				exit(1);
			}
			
			eof = false;
			
			curbyte = -1;
			bitpos = 0;
			bytenum = 0;
		}
		
		/**
		* Reads in bytes to a buffer
		* @param bytes An array to place the bytes read in
		* @return The actual number of bytes read in
		*/
		int readbytes(int *bytes, int len) {
			unsigned char *b = new unsigned char[len];
			
			int bytesread = 0;
			
			bytesread = fread(b, 1, len, infile);
			
			for (int i=0; i<bytesread; i++)
				bytes[i] = b[i]&0xFF; // We only want 8 bits
			
			if (bytesread < len) {
				if (feof(infile)) {
					eof = true;
					exit(1);
				}
			}
			
			bytenum += bytesread;
			
			delete b;
			
			return bytesread;
		}
};

#endif

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

#ifndef AUDIO_H
#define AUDIO_H

#include "floor1.h"
#include "mapping.h"
#include "mode.h"
#include "residue.h"

class Audio {
	public:
		/** Windowing curve data */
		double *window;
		/** X size of the window */
		int n;
		/** X size of previous window */
		int last_n;
		/** Flag for the previous window */
		int previous_window_flag;
		/** Flag for the next window */
		int next_window_flag;
		/** Left window starting point */
		int left_window_start;
		/** Left window ending point */
		int left_window_end;
		/** Right window starting point */
		int right_window_start;
		/** Right window ending point */
		int right_window_end;
		/** X size of previous window overlap */
		int left_n;
		/** X size of next window overlap */
		int right_n;
		/** Center of this window */
		int window_center;

		/** Vector to keep track of which channels have no residue in this frame */
		int *no_residue;

		/** The decoded floor data, in channel order */
		double **floor_out;
		/** The decoded residue data, associated with the correct channel */
		double **residue_out;

		/** The spectrum data */
		int **spectrum;

		/** Pointer to the mapping for this frame */
		Mapping *mapping;
		/** Pointer to the mode for this frame */
		Mode *mode;
};

#endif

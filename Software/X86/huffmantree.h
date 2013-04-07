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


#ifndef HUFFMANTREE_H
#define HUFFMANTREE_H

#include <iostream>

#include "huffmannode.h"

class HuffmanTree {
	public:
		/** Root of the tree */
		HuffmanNode *root;
		
		/** Creates an initial node for the root */
		HuffmanTree() {
			root = new HuffmanNode(-1);
		}
		
		/**
		* Adds a new node to the tree in the first spot (left to right)
		* available at a given tree depth
		* @param length The depth from the root where the new node should be added
		* @param value The value to give the new node
		*/
		void AddNode(int length, int entry) {
			AddNodeR(root, length, entry);
		}
		
		/**
		* Recursively finds the correct spot to add a node, then adds it
		* @param node The current recursion's node to search under
		* @param length The depth from the root where the new node should be added
		* @param value The value to give the new node
		* @return True when the node has been successfully added
		*/
		bool AddNodeR(HuffmanNode *node, int length, int entry) {
			if (length > 1) { // We still need to search deeper
				if (node->lchild == NULL) {
					// We need to create a new link to continue
					node->lchild = new HuffmanNode(-1);
				}
				
				if (node->lchild->entry == -1) {
					// We're going left
					if (AddNodeR(node->lchild, length-1, entry)) {
						return true;
					}
				}
				// There's a leaf node this way; back up
				
				if (node->rchild == NULL) {
					// We need to create a new link to continue
					node->rchild = new HuffmanNode(-1);
				}
				
				if (node->rchild->entry == -1) {
					// We're going right
					if (AddNodeR(node->rchild, length-1, entry)) {
						return true;
					}
				}
				// There's a leaf node this way; back up
			} else if (length == 1) {
				// We're at the correct depth
				if (node->lchild == NULL) {
					// Do we have room to the left?
					node->lchild = new HuffmanNode(entry);
					return true;
				}
				if (node->rchild == NULL) {
					// Do we have room to the right?
					node->rchild = new HuffmanNode(entry);
					return true;
				}
			}
			
			// This should never be reached pending proper function use
			return false;
		}
};

#endif

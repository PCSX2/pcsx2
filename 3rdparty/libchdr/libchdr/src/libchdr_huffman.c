/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
****************************************************************************

    huffman.c

    Static Huffman compression and decompression helpers.

****************************************************************************

    Maximum codelength is officially (alphabetsize - 1). This would be 255 bits
    (since we use 1 byte values). However, it is also dependent upon the number
    of samples used, as follows:

         2 bits -> 3..4 samples
         3 bits -> 5..7 samples
         4 bits -> 8..12 samples
         5 bits -> 13..20 samples
         6 bits -> 21..33 samples
         7 bits -> 34..54 samples
         8 bits -> 55..88 samples
         9 bits -> 89..143 samples
        10 bits -> 144..232 samples
        11 bits -> 233..376 samples
        12 bits -> 377..609 samples
        13 bits -> 610..986 samples
        14 bits -> 987..1596 samples
        15 bits -> 1597..2583 samples
        16 bits -> 2584..4180 samples   -> note that a 4k data size guarantees codelength <= 16 bits
        17 bits -> 4181..6764 samples
        18 bits -> 6765..10945 samples
        19 bits -> 10946..17710 samples
        20 bits -> 17711..28656 samples
        21 bits -> 28657..46367 samples
        22 bits -> 46368..75024 samples
        23 bits -> 75025..121392 samples
        24 bits -> 121393..196417 samples
        25 bits -> 196418..317810 samples
        26 bits -> 317811..514228 samples
        27 bits -> 514229..832039 samples
        28 bits -> 832040..1346268 samples
        29 bits -> 1346269..2178308 samples
        30 bits -> 2178309..3524577 samples
        31 bits -> 3524578..5702886 samples
        32 bits -> 5702887..9227464 samples

    Looking at it differently, here is where powers of 2 fall into these buckets:

          256 samples -> 11 bits max
          512 samples -> 12 bits max
           1k samples -> 14 bits max
           2k samples -> 15 bits max
           4k samples -> 16 bits max
           8k samples -> 18 bits max
          16k samples -> 19 bits max
          32k samples -> 21 bits max
          64k samples -> 22 bits max
         128k samples -> 24 bits max
         256k samples -> 25 bits max
         512k samples -> 27 bits max
           1M samples -> 28 bits max
           2M samples -> 29 bits max
           4M samples -> 31 bits max
           8M samples -> 32 bits max

****************************************************************************

    Delta-RLE encoding works as follows:

    Starting value is assumed to be 0. All data is encoded as a delta
    from the previous value, such that final[i] = final[i - 1] + delta.
    Long runs of 0s are RLE-encoded as follows:

        0x100 = repeat count of 8
        0x101 = repeat count of 9
        0x102 = repeat count of 10
        0x103 = repeat count of 11
        0x104 = repeat count of 12
        0x105 = repeat count of 13
        0x106 = repeat count of 14
        0x107 = repeat count of 15
        0x108 = repeat count of 16
        0x109 = repeat count of 32
        0x10a = repeat count of 64
        0x10b = repeat count of 128
        0x10c = repeat count of 256
        0x10d = repeat count of 512
        0x10e = repeat count of 1024
        0x10f = repeat count of 2048

    Note that repeat counts are reset at the end of a row, so if a 0 run
    extends to the end of a row, a large repeat count may be used.

    The reason for starting the run counts at 8 is that 0 is expected to
    be the most common symbol, and is typically encoded in 1 or 2 bits.

***************************************************************************/

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "huffman.h"

#define MAX(x,y) ((x) > (y) ? (x) : (y))

/***************************************************************************
 *  MACROS
 ***************************************************************************
 */

#define MAKE_LOOKUP(code,bits)  (((code) << 5) | ((bits) & 0x1f))

/***************************************************************************
 *  IMPLEMENTATION
 ***************************************************************************
 */

/*-------------------------------------------------
 *  huffman_context_base - create an encoding/
 *  decoding context
 *-------------------------------------------------
 */

struct huffman_decoder* create_huffman_decoder(int numcodes, int maxbits)
{
	struct huffman_decoder* decoder = NULL;

	/* limit to 24 bits */
	if (maxbits > 24)
		return NULL;

	decoder = (struct huffman_decoder*)malloc(sizeof(struct huffman_decoder));
	decoder->numcodes = numcodes;
	decoder->maxbits = maxbits;
	decoder->lookup = (lookup_value*)malloc(sizeof(lookup_value) * (1 << maxbits));
	decoder->huffnode = (struct node_t*)malloc(sizeof(struct node_t) * numcodes);
	decoder->datahisto = NULL;
	decoder->prevdata = 0;
	decoder->rleremaining = 0;
	return decoder;
}

void delete_huffman_decoder(struct huffman_decoder* decoder)
{
	if (decoder != NULL)
	{
		if (decoder->lookup != NULL)
			free(decoder->lookup);
		if (decoder->huffnode != NULL)
			free(decoder->huffnode);
		free(decoder);
	}
}

/*-------------------------------------------------
 *  decode_one - decode a single code from the
 *  huffman stream
 *-------------------------------------------------
 */

uint32_t huffman_decode_one(struct huffman_decoder* decoder, struct bitstream* bitbuf)
{
	/* peek ahead to get maxbits worth of data */
	uint32_t bits = bitstream_peek(bitbuf, decoder->maxbits);

	/* look it up, then remove the actual number of bits for this code */
	lookup_value lookup = decoder->lookup[bits];
	bitstream_remove(bitbuf, lookup & 0x1f);

	/* return the value */
	return lookup >> 5;
}

/*-------------------------------------------------
 *  import_tree_rle - import an RLE-encoded
 *  huffman tree from a source data stream
 *-------------------------------------------------
 */

enum huffman_error huffman_import_tree_rle(struct huffman_decoder* decoder, struct bitstream* bitbuf)
{
	int numbits, curnode;
	enum huffman_error error;

	/* bits per entry depends on the maxbits */
	if (decoder->maxbits >= 16)
		numbits = 5;
	else if (decoder->maxbits >= 8)
		numbits = 4;
	else
		numbits = 3;

	/* loop until we read all the nodes */
	for (curnode = 0; curnode < decoder->numcodes; )
	{
		/* a non-one value is just raw */
		int nodebits = bitstream_read(bitbuf, numbits);
		if (nodebits != 1)
			decoder->huffnode[curnode++].numbits = nodebits;

		/* a one value is an escape code */
		else
		{
			/* a double 1 is just a single 1 */
			nodebits = bitstream_read(bitbuf, numbits);
			if (nodebits == 1)
				decoder->huffnode[curnode++].numbits = nodebits;

			/* otherwise, we need one for value for the repeat count */
			else
			{
				int repcount = bitstream_read(bitbuf, numbits) + 3;
				while (repcount--)
					decoder->huffnode[curnode++].numbits = nodebits;
			}
		}
	}

	/* make sure we ended up with the right number */
	if (curnode != decoder->numcodes)
		return HUFFERR_INVALID_DATA;

	/* assign canonical codes for all nodes based on their code lengths */
	error = huffman_assign_canonical_codes(decoder);
	if (error != HUFFERR_NONE)
		return error;

	/* build the lookup table */
	huffman_build_lookup_table(decoder);

	/* determine final input length and report errors */
	return bitstream_overflow(bitbuf) ? HUFFERR_INPUT_BUFFER_TOO_SMALL : HUFFERR_NONE;
}


/*-------------------------------------------------
 *  import_tree_huffman - import a huffman-encoded
 *  huffman tree from a source data stream
 *-------------------------------------------------
 */

enum huffman_error huffman_import_tree_huffman(struct huffman_decoder* decoder, struct bitstream* bitbuf)
{
	int start;
	int last = 0;
	int count = 0;
	int index;
	int curcode;
	uint8_t rlefullbits = 0;
	uint32_t temp;
	enum huffman_error error;
	/* start by parsing the lengths for the small tree */
	struct huffman_decoder* smallhuff = create_huffman_decoder(24, 6);
	smallhuff->huffnode[0].numbits = bitstream_read(bitbuf, 3);
	start = bitstream_read(bitbuf, 3) + 1;
	for (index = 1; index < 24; index++)
	{
		if (index < start || count == 7)
			smallhuff->huffnode[index].numbits = 0;
		else
		{
			count = bitstream_read(bitbuf, 3);
			smallhuff->huffnode[index].numbits = (count == 7) ? 0 : count;
		}
	}

	/* then regenerate the tree */
	error = huffman_assign_canonical_codes(smallhuff);
	if (error != HUFFERR_NONE)
		return error;
	huffman_build_lookup_table(smallhuff);

	/* determine the maximum length of an RLE count */
	temp = decoder->numcodes - 9;
	while (temp != 0)
		temp >>= 1, rlefullbits++;

	/* now process the rest of the data */
	for (curcode = 0; curcode < decoder->numcodes; )
	{
		int value = huffman_decode_one(smallhuff, bitbuf);
		if (value != 0)
			decoder->huffnode[curcode++].numbits = last = value - 1;
		else
		{
			int count = bitstream_read(bitbuf, 3) + 2;
			if (count == 7+2)
				count += bitstream_read(bitbuf, rlefullbits);
			for ( ; count != 0 && curcode < decoder->numcodes; count--)
				decoder->huffnode[curcode++].numbits = last;
		}
	}

	/* make sure we ended up with the right number */
	if (curcode != decoder->numcodes)
		return HUFFERR_INVALID_DATA;

	/* assign canonical codes for all nodes based on their code lengths */
	error = huffman_assign_canonical_codes(decoder);
	if (error != HUFFERR_NONE)
		return error;

	/* build the lookup table */
	huffman_build_lookup_table(decoder);

	/* determine final input length and report errors */
	return bitstream_overflow(bitbuf) ? HUFFERR_INPUT_BUFFER_TOO_SMALL : HUFFERR_NONE;
}

/*-------------------------------------------------
 *  compute_tree_from_histo - common backend for
 *  computing a tree based on the data histogram
 *-------------------------------------------------
 */

enum huffman_error huffman_compute_tree_from_histo(struct huffman_decoder* decoder)
{
	int i;
	uint32_t lowerweight;
	uint32_t upperweight;
	/* compute the number of data items in the histogram */
	uint32_t sdatacount = 0;
	for (i = 0; i < decoder->numcodes; i++)
		sdatacount += decoder->datahisto[i];

	/* binary search to achieve the optimum encoding */
	lowerweight = 0;
	upperweight = sdatacount * 2;
	while (1)
	{
		/* build a tree using the current weight */
		uint32_t curweight = (upperweight + lowerweight) / 2;
		int curmaxbits = huffman_build_tree(decoder, sdatacount, curweight);

		/* apply binary search here */
		if (curmaxbits <= decoder->maxbits)
		{
			lowerweight = curweight;

			/* early out if it worked with the raw weights, or if we're done searching */
			if (curweight == sdatacount || (upperweight - lowerweight) <= 1)
				break;
		}
		else
			upperweight = curweight;
	}

	/* assign canonical codes for all nodes based on their code lengths */
	return huffman_assign_canonical_codes(decoder);
}

/***************************************************************************
 *  INTERNAL FUNCTIONS
 ***************************************************************************
 */

/*-------------------------------------------------
 *  tree_node_compare - compare two tree nodes
 *  by weight
 *-------------------------------------------------
 */

static int huffman_tree_node_compare(const void *item1, const void *item2)
{
	const struct node_t *node1 = *(const struct node_t **)item1;
	const struct node_t *node2 = *(const struct node_t **)item2;
	if (node2->weight != node1->weight)
		return node2->weight - node1->weight;
	if (node2->bits - node1->bits == 0)
		fprintf(stderr, "identical node sort keys, should not happen!\n");
	return (int)node1->bits - (int)node2->bits;
}

/*-------------------------------------------------
 *  build_tree - build a huffman tree based on the
 *  data distribution
 *-------------------------------------------------
 */

int huffman_build_tree(struct huffman_decoder* decoder, uint32_t totaldata, uint32_t totalweight)
{
	int curcode;
	int nextalloc;
	int listitems = 0;
	int maxbits = 0;
	/* make a list of all non-zero nodes */
	struct node_t** list = (struct node_t**)malloc(sizeof(struct node_t*) * decoder->numcodes * 2);
	memset(decoder->huffnode, 0, decoder->numcodes * sizeof(decoder->huffnode[0]));
	for (curcode = 0; curcode < decoder->numcodes; curcode++)
		if (decoder->datahisto[curcode] != 0)
		{
			list[listitems++] = &decoder->huffnode[curcode];
			decoder->huffnode[curcode].count = decoder->datahisto[curcode];
			decoder->huffnode[curcode].bits = curcode;

			/* scale the weight by the current effective length, ensuring we don't go to 0 */
			decoder->huffnode[curcode].weight = ((uint64_t)decoder->datahisto[curcode]) * ((uint64_t)totalweight) / ((uint64_t)totaldata);
			if (decoder->huffnode[curcode].weight == 0)
				decoder->huffnode[curcode].weight = 1;
		}

#if 0
        fprintf(stderr, "Pre-sort:\n");
        for (int i = 0; i < listitems; i++) {
            fprintf(stderr, "weight: %d code: %d\n", list[i]->m_weight, list[i]->m_bits);
        }
#endif

	/* sort the list by weight, largest weight first */
	qsort(&list[0], listitems, sizeof(list[0]), huffman_tree_node_compare);

#if 0
        fprintf(stderr, "Post-sort:\n");
        for (int i = 0; i < listitems; i++) {
            fprintf(stderr, "weight: %d code: %d\n", list[i]->m_weight, list[i]->m_bits);
        }
        fprintf(stderr, "===================\n");
#endif

	/* now build the tree */
	nextalloc = decoder->numcodes;
	while (listitems > 1)
	{
		int curitem;
		/* remove lowest two items */
		struct node_t* node1 = &(*list[--listitems]);
		struct node_t* node0 = &(*list[--listitems]);

		/* create new node */
		struct node_t* newnode = &decoder->huffnode[nextalloc++];
		newnode->parent = NULL;
		node0->parent = node1->parent = newnode;
		newnode->weight = node0->weight + node1->weight;

		/* insert into list at appropriate location */
		for (curitem = 0; curitem < listitems; curitem++)
			if (newnode->weight > list[curitem]->weight)
			{
				memmove(&list[curitem+1], &list[curitem], (listitems - curitem) * sizeof(list[0]));
				break;
			}
		list[curitem] = newnode;
		listitems++;
	}

	/* compute the number of bits in each code, and fill in another histogram */
	for (curcode = 0; curcode < decoder->numcodes; curcode++)
	{
		struct node_t *curnode;
		struct node_t* node = &decoder->huffnode[curcode];
		node->numbits = 0;
		node->bits = 0;

		/* if we have a non-zero weight, compute the number of bits */
		if (node->weight > 0)
		{
			/* determine the number of bits for this node */
			for (curnode = node; curnode->parent != NULL; curnode = curnode->parent)
				node->numbits++;
			if (node->numbits == 0)
				node->numbits = 1;

			/* keep track of the max */
			maxbits = MAX(maxbits, ((int)node->numbits));
		}
	}
	return maxbits;
}

/*-------------------------------------------------
 *  assign_canonical_codes - assign canonical codes
 *  to all the nodes based on the number of bits
 *  in each
 *-------------------------------------------------
 */

enum huffman_error huffman_assign_canonical_codes(struct huffman_decoder* decoder)
{
	int curcode, codelen;
	uint32_t curstart = 0;
	/* build up a histogram of bit lengths */
	uint32_t bithisto[33] = { 0 };
	for (curcode = 0; curcode < decoder->numcodes; curcode++)
	{
		struct node_t* node = &decoder->huffnode[curcode];
		if (node->numbits > decoder->maxbits)
			return HUFFERR_INTERNAL_INCONSISTENCY;
		if (node->numbits <= 32)
			bithisto[node->numbits]++;
	}

	/* for each code length, determine the starting code number */
	for (codelen = 32; codelen > 0; codelen--)
	{
		uint32_t nextstart = (curstart + bithisto[codelen]) >> 1;
		if (codelen != 1 && nextstart * 2 != (curstart + bithisto[codelen]))
			return HUFFERR_INTERNAL_INCONSISTENCY;
		bithisto[codelen] = curstart;
		curstart = nextstart;
	}

	/* now assign canonical codes */
	for (curcode = 0; curcode < decoder->numcodes; curcode++)
	{
		struct node_t* node = &decoder->huffnode[curcode];
		if (node->numbits > 0)
			node->bits = bithisto[node->numbits]++;
	}
	return HUFFERR_NONE;
}

/*-------------------------------------------------
 *  build_lookup_table - build a lookup table for
 *  fast decoding
 *-------------------------------------------------
 */

void huffman_build_lookup_table(struct huffman_decoder* decoder)
{
	int curcode;
	/* iterate over all codes */
	for (curcode = 0; curcode < decoder->numcodes; curcode++)
	{
		/* process all nodes which have non-zero bits */
		struct node_t* node = &decoder->huffnode[curcode];
		if (node->numbits > 0)
		{
         int shift;
         lookup_value *dest;
         lookup_value *destend;
			/* set up the entry */
			lookup_value value = MAKE_LOOKUP(curcode, node->numbits);

			/* fill all matching entries */
			shift = decoder->maxbits - node->numbits;
			dest = &decoder->lookup[node->bits << shift];
			destend = &decoder->lookup[((node->bits + 1) << shift) - 1];
			while (dest <= destend)
				*dest++ = value;
		}
	}
}

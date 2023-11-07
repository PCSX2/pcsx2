/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
 ***************************************************************************

    huffman.h

    Static Huffman compression and decompression helpers.

***************************************************************************/

#pragma once

#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

#include <libchdr/bitstream.h>


/***************************************************************************
 *  CONSTANTS
 ***************************************************************************
 */

enum huffman_error
{
	HUFFERR_NONE = 0,
	HUFFERR_TOO_MANY_BITS,
	HUFFERR_INVALID_DATA,
	HUFFERR_INPUT_BUFFER_TOO_SMALL,
	HUFFERR_OUTPUT_BUFFER_TOO_SMALL,
	HUFFERR_INTERNAL_INCONSISTENCY,
	HUFFERR_TOO_MANY_CONTEXTS
};

/***************************************************************************
 *  TYPE DEFINITIONS
 ***************************************************************************
 */

typedef uint16_t lookup_value;

/* a node in the huffman tree */
struct node_t
{
	struct node_t*		parent;		/* pointer to parent node */
	uint32_t			count;		/* number of hits on this node */
	uint32_t			weight;		/* assigned weight of this node */
	uint32_t			bits;		/* bits used to encode the node */
	uint8_t				numbits;	/* number of bits needed for this node */
};

/* ======================> huffman_context_base */

/* context class for decoding */
struct huffman_decoder
{
	/* internal state */
	uint32_t			numcodes;             /* number of total codes being processed */
	uint8_t				maxbits;           /* maximum bits per code */
	uint8_t 			prevdata;             /* value of the previous data (for delta-RLE encoding) */
	int             	rleremaining;         /* number of RLE bytes remaining (for delta-RLE encoding) */
	lookup_value *  	lookup;               /* pointer to the lookup table */
	struct node_t *     huffnode;             /* array of nodes */
	uint32_t *      	datahisto;            /* histogram of data values */

	/* array versions of the info we need */
#if 0
	node_t*			huffnode_array; /* [_NumCodes]; */
	lookup_value*	lookup_array; /* [1 << _MaxBits]; */
#endif
};

/* ======================> huffman_decoder */

struct huffman_decoder* create_huffman_decoder(int numcodes, int maxbits);
void delete_huffman_decoder(struct huffman_decoder* decoder);

/* single item operations */
uint32_t huffman_decode_one(struct huffman_decoder* decoder, struct bitstream* bitbuf);

enum huffman_error huffman_import_tree_rle(struct huffman_decoder* decoder, struct bitstream* bitbuf);
enum huffman_error huffman_import_tree_huffman(struct huffman_decoder* decoder, struct bitstream* bitbuf);

int huffman_build_tree(struct huffman_decoder* decoder, uint32_t totaldata, uint32_t totalweight);
enum huffman_error huffman_assign_canonical_codes(struct huffman_decoder* decoder);
enum huffman_error huffman_compute_tree_from_histo(struct huffman_decoder* decoder);

void huffman_build_lookup_table(struct huffman_decoder* decoder);

#endif

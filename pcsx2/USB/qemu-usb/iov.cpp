/*
 * Helpers for getting linearized buffers from iov / filling buffers into iovs
 *
 * Copyright IBM, Corp. 2007, 2008
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * Author(s):
 *  Anthony Liguori <aliguori@us.ibm.com>
 *  Amit Shah <amit.shah@redhat.com>
 *  Michael Tokarev <mjt@tls.msk.ru>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "PrecompiledHeader.h"
#include "vl.h"
//#include "qemu-common.h"
#include "iov.h"
#include "glib.h"
//#include "qemu/sockets.h"
//#include "qemu/cutils.h"
#include <cassert>

size_t iov_from_buf_full(const struct iovec* iov, unsigned int iov_cnt,
						 size_t offset, const void* buf, size_t bytes)
{
	size_t done;
	unsigned int i;
	for (i = 0, done = 0; (offset || done < bytes) && i < iov_cnt; i++)
	{
		if (offset < iov[i].iov_len)
		{
			size_t len = MIN(iov[i].iov_len - offset, bytes - done);
			memcpy((char*)iov[i].iov_base + offset, (char*)buf + done, len);
			done += len;
			offset = 0;
		}
		else
		{
			offset -= iov[i].iov_len;
		}
	}
	assert(offset == 0);
	return done;
}

size_t iov_to_buf_full(const struct iovec* iov, const unsigned int iov_cnt,
					   size_t offset, void* buf, size_t bytes)
{
	size_t done;
	unsigned int i;
	for (i = 0, done = 0; (offset || done < bytes) && i < iov_cnt; i++)
	{
		if (offset < iov[i].iov_len)
		{
			size_t len = MIN(iov[i].iov_len - offset, bytes - done);
			memcpy((char*)buf + done, (char*)iov[i].iov_base + offset, len);
			done += len;
			offset = 0;
		}
		else
		{
			offset -= iov[i].iov_len;
		}
	}
	assert(offset == 0);
	return done;
}

size_t iov_memset(const struct iovec* iov, const unsigned int iov_cnt,
				  size_t offset, int fillc, size_t bytes)
{
	size_t done;
	unsigned int i;
	for (i = 0, done = 0; (offset || done < bytes) && i < iov_cnt; i++)
	{
		if (offset < iov[i].iov_len)
		{
			size_t len = MIN(iov[i].iov_len - offset, bytes - done);
			memset((char*)iov[i].iov_base + offset, fillc, len);
			done += len;
			offset = 0;
		}
		else
		{
			offset -= iov[i].iov_len;
		}
	}
	assert(offset == 0);
	return done;
}

size_t iov_size(const struct iovec* iov, const unsigned int iov_cnt)
{
	size_t len;
	unsigned int i;

	len = 0;
	for (i = 0; i < iov_cnt; i++)
	{
		len += iov[i].iov_len;
	}
	return len;
}

unsigned iov_copy(struct iovec* dst_iov, unsigned int dst_iov_cnt,
				  const struct iovec* iov, unsigned int iov_cnt,
				  size_t offset, size_t bytes)
{
	size_t len;
	unsigned int i, j;
	for (i = 0, j = 0;
		 i < iov_cnt && j < dst_iov_cnt && (offset || bytes); i++)
	{
		if (offset >= iov[i].iov_len)
		{
			offset -= iov[i].iov_len;
			continue;
		}
		len = MIN(bytes, iov[i].iov_len - offset);

		dst_iov[j].iov_base = (char*)iov[i].iov_base + offset;
		dst_iov[j].iov_len = len;
		j++;
		bytes -= len;
		offset = 0;
	}
	assert(offset == 0);
	return j;
}

/* io vectors */

void qemu_iovec_init(QEMUIOVector* qiov, int alloc_hint)
{
	qiov->iov = my_g_new(struct iovec, alloc_hint);
	qiov->niov = 0;
	qiov->nalloc = alloc_hint;
	qiov->size = 0;
}

void qemu_iovec_init_external(QEMUIOVector* qiov, struct iovec* iov, int niov)
{
	int i;

	qiov->iov = iov;
	qiov->niov = niov;
	qiov->nalloc = -1;
	qiov->size = 0;
	for (i = 0; i < niov; i++)
		qiov->size += iov[i].iov_len;
}

void qemu_iovec_add(QEMUIOVector* qiov, void* base, size_t len)
{
	assert(qiov->nalloc != -1);

	if (qiov->niov == qiov->nalloc)
	{
		qiov->nalloc = 2 * qiov->nalloc + 1;
		qiov->iov = my_g_renew(struct iovec, qiov->iov, qiov->nalloc);
	}
	qiov->iov[qiov->niov].iov_base = base;
	qiov->iov[qiov->niov].iov_len = len;
	qiov->size += len;
	++qiov->niov;
}

/*
 * Concatenates (partial) iovecs from src_iov to the end of dst.
 * It starts copying after skipping `soffset' bytes at the
 * beginning of src and adds individual vectors from src to
 * dst copies up to `sbytes' bytes total, or up to the end
 * of src_iov if it comes first.  This way, it is okay to specify
 * very large value for `sbytes' to indicate "up to the end
 * of src".
 * Only vector pointers are processed, not the actual data buffers.
 */
size_t qemu_iovec_concat_iov(QEMUIOVector* dst,
							 struct iovec* src_iov, unsigned int src_cnt,
							 size_t soffset, size_t sbytes)
{
	unsigned int i;
	size_t done;

	if (!sbytes)
	{
		return 0;
	}
	assert(dst->nalloc != -1);
	for (i = 0, done = 0; done < sbytes && i < src_cnt; i++)
	{
		if (soffset < src_iov[i].iov_len)
		{
			size_t len = MIN(src_iov[i].iov_len - soffset, sbytes - done);
			qemu_iovec_add(dst, (char*)src_iov[i].iov_base + soffset, len);
			done += len;
			soffset = 0;
		}
		else
		{
			soffset -= src_iov[i].iov_len;
		}
	}
	assert(soffset == 0); /* offset beyond end of src */

	return done;
}

/*
 * Concatenates (partial) iovecs from src to the end of dst.
 * It starts copying after skipping `soffset' bytes at the
 * beginning of src and adds individual vectors from src to
 * dst copies up to `sbytes' bytes total, or up to the end
 * of src if it comes first.  This way, it is okay to specify
 * very large value for `sbytes' to indicate "up to the end
 * of src".
 * Only vector pointers are processed, not the actual data buffers.
 */
void qemu_iovec_concat(QEMUIOVector* dst,
					   QEMUIOVector* src, size_t soffset, size_t sbytes)
{
	qemu_iovec_concat_iov(dst, src->iov, src->niov, soffset, sbytes);
}

/*
 * Check if the contents of the iovecs are all zero
 */
/*bool qemu_iovec_is_zero(QEMUIOVector *qiov)
{
    int i;
    for (i = 0; i < qiov->niov; i++) {
        size_t offs = QEMU_ALIGN_DOWN(qiov->iov[i].iov_len, 4 * sizeof(long));
        uint8_t *ptr = (uint8_t *)qiov->iov[i].iov_base;
        if (offs && !buffer_is_zero(qiov->iov[i].iov_base, offs)) {
            return false;
        }
        for (; offs < qiov->iov[i].iov_len; offs++) {
            if (ptr[offs]) {
                return false;
            }
        }
    }
    return true;
}*/

void qemu_iovec_destroy(QEMUIOVector* qiov)
{
	assert(qiov->nalloc != -1);

	qemu_iovec_reset(qiov);
	my_g_free(qiov->iov);
	qiov->nalloc = 0;
	qiov->iov = NULL;
}

void qemu_iovec_reset(QEMUIOVector* qiov)
{
	assert(qiov->nalloc != -1);

	qiov->niov = 0;
	qiov->size = 0;
}

size_t qemu_iovec_to_buf(QEMUIOVector* qiov, size_t offset,
						 void* buf, size_t bytes)
{
	return iov_to_buf(qiov->iov, qiov->niov, offset, buf, bytes);
}

size_t qemu_iovec_from_buf(QEMUIOVector* qiov, size_t offset,
						   const void* buf, size_t bytes)
{
	return iov_from_buf(qiov->iov, qiov->niov, offset, buf, bytes);
}

size_t qemu_iovec_memset(QEMUIOVector* qiov, size_t offset,
						 int fillc, size_t bytes)
{
	return iov_memset(qiov->iov, qiov->niov, offset, fillc, bytes);
}

/**
 * Check that I/O vector contents are identical
 *
 * The IO vectors must have the same structure (same length of all parts).
 * A typical usage is to compare vectors created with qemu_iovec_clone().
 *
 * @a:          I/O vector
 * @b:          I/O vector
 * @ret:        Offset to first mismatching byte or -1 if match
 */
ssize_t qemu_iovec_compare(QEMUIOVector* a, QEMUIOVector* b)
{
	int i;
	ssize_t offset = 0;

	assert(a->niov == b->niov);
	for (i = 0; i < a->niov; i++)
	{
		size_t len = 0;
		uint8_t* p = (uint8_t*)a->iov[i].iov_base;
		uint8_t* q = (uint8_t*)b->iov[i].iov_base;

		assert(a->iov[i].iov_len == b->iov[i].iov_len);
		while (len < a->iov[i].iov_len && *p++ == *q++)
		{
			len++;
		}

		offset += len;

		if (len != a->iov[i].iov_len)
		{
			return offset;
		}
	}
	return -1;
}

typedef struct
{
	int src_index;
	struct iovec* src_iov;
	void* dest_base;
} IOVectorSortElem;

static int sortelem_cmp_src_base(const void* a, const void* b)
{
	const IOVectorSortElem* elem_a = (const IOVectorSortElem*)a;
	const IOVectorSortElem* elem_b = (const IOVectorSortElem*)b;

	/* Don't overflow */
	if (elem_a->src_iov->iov_base < elem_b->src_iov->iov_base)
	{
		return -1;
	}
	else if (elem_a->src_iov->iov_base > elem_b->src_iov->iov_base)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int sortelem_cmp_src_index(const void* a, const void* b)
{
	const IOVectorSortElem* elem_a = (const IOVectorSortElem*)a;
	const IOVectorSortElem* elem_b = (const IOVectorSortElem*)b;

	return elem_a->src_index - elem_b->src_index;
}

size_t iov_discard_front(struct iovec** iov, unsigned int* iov_cnt,
						 size_t bytes)
{
	size_t total = 0;
	struct iovec* cur;

	for (cur = *iov; *iov_cnt > 0; cur++)
	{
		if (cur->iov_len > bytes)
		{
			cur->iov_base = (uint8_t*)cur->iov_base + bytes;
			cur->iov_len -= bytes;
			total += bytes;
			break;
		}

		bytes -= cur->iov_len;
		total += cur->iov_len;
		*iov_cnt -= 1;
	}

	*iov = cur;
	return total;
}

size_t iov_discard_back(struct iovec* iov, unsigned int* iov_cnt,
						size_t bytes)
{
	size_t total = 0;
	struct iovec* cur;

	if (*iov_cnt == 0)
	{
		return 0;
	}

	cur = iov + (*iov_cnt - 1);

	while (*iov_cnt > 0)
	{
		if (cur->iov_len > bytes)
		{
			cur->iov_len -= bytes;
			total += bytes;
			break;
		}

		bytes -= cur->iov_len;
		total += cur->iov_len;
		cur--;
		*iov_cnt -= 1;
	}

	return total;
}

void qemu_iovec_discard_back(QEMUIOVector* qiov, size_t bytes)
{
	unsigned int niov = qiov->niov;

	assert(qiov->size >= bytes);
	assert(iov_discard_back(qiov->iov, &niov, bytes) == bytes);

	qiov->niov = niov;
	qiov->size -= bytes;
}

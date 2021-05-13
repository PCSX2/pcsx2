/*
 * Helpers for using (partial) iovecs.
 *
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * Author(s):
 *  Amit Shah <amit.shah@redhat.com>
 *  Michael Tokarev <mjt@tls.msk.ru>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include "USB/platcompat.h"

#ifndef IOV_H
#define IOV_H

#if !defined(_BITS_UIO_H) && !defined(__iovec_defined) /* /usr/include/bits/uio.h */
struct iovec
{
	void* iov_base;
	size_t iov_len;
};
#endif

/**
 * count and return data size, in bytes, of an iovec
 * starting at `iov' of `iov_cnt' number of elements.
 */
size_t iov_size(const struct iovec* iov, const unsigned int iov_cnt);

/**
 * Copy from single continuous buffer to scatter-gather vector of buffers
 * (iovec) and back like memcpy() between two continuous memory regions.
 * Data in single continuous buffer starting at address `buf' and
 * `bytes' bytes long will be copied to/from an iovec `iov' with
 * `iov_cnt' number of elements, starting at byte position `offset'
 * within the iovec.  If the iovec does not contain enough space,
 * only part of data will be copied, up to the end of the iovec.
 * Number of bytes actually copied will be returned, which is
 *  min(bytes, iov_size(iov)-offset)
 * `Offset' must point to the inside of iovec.
 * It is okay to use very large value for `bytes' since we're
 * limited by the size of the iovec anyway, provided that the
 * buffer pointed to by buf has enough space.  One possible
 * such "large" value is -1 (sinice size_t is unsigned),
 * so specifying `-1' as `bytes' means 'up to the end of iovec'.
 */
size_t iov_from_buf_full(const struct iovec* iov, unsigned int iov_cnt,
						 size_t offset, const void* buf, size_t bytes);
size_t iov_to_buf_full(const struct iovec* iov, const unsigned int iov_cnt,
					   size_t offset, void* buf, size_t bytes);

static inline size_t
iov_from_buf(const struct iovec* iov, unsigned int iov_cnt,
			 size_t offset, const void* buf, size_t bytes)
{
	if (__builtin_constant_p(bytes) && iov_cnt &&
		offset <= iov[0].iov_len && bytes <= iov[0].iov_len - offset)
	{
		memcpy((char*)iov[0].iov_base + offset, buf, bytes);
		return bytes;
	}
	else
	{
		return iov_from_buf_full(iov, iov_cnt, offset, buf, bytes);
	}
}

static inline size_t
iov_to_buf(const struct iovec* iov, const unsigned int iov_cnt,
		   size_t offset, void* buf, size_t bytes)
{
	if (__builtin_constant_p(bytes) && iov_cnt &&
		offset <= iov[0].iov_len && bytes <= iov[0].iov_len - offset)
	{
		memcpy(buf, (char*)iov[0].iov_base + offset, bytes);
		return bytes;
	}
	else
	{
		return iov_to_buf_full(iov, iov_cnt, offset, buf, bytes);
	}
}

/**
 * Set data bytes pointed out by iovec `iov' of size `iov_cnt' elements,
 * starting at byte offset `start', to value `fillc', repeating it
 * `bytes' number of times.  `Offset' must point to the inside of iovec.
 * If `bytes' is large enough, only last bytes portion of iovec,
 * up to the end of it, will be filled with the specified value.
 * Function return actual number of bytes processed, which is
 * min(size, iov_size(iov) - offset).
 * Again, it is okay to use large value for `bytes' to mean "up to the end".
 */
size_t iov_memset(const struct iovec* iov, const unsigned int iov_cnt,
				  size_t offset, int fillc, size_t bytes);

/*
 * Send/recv data from/to iovec buffers directly
 *
 * `offset' bytes in the beginning of iovec buffer are skipped and
 * next `bytes' bytes are used, which must be within data of iovec.
 *
 *   r = iov_send_recv(sockfd, iov, iovcnt, offset, bytes, true);
 *
 * is logically equivalent to
 *
 *   char *buf = malloc(bytes);
 *   iov_to_buf(iov, iovcnt, offset, buf, bytes);
 *   r = send(sockfd, buf, bytes, 0);
 *   free(buf);
 *
 * For iov_send_recv() _whole_ area being sent or received
 * should be within the iovec, not only beginning of it.
 */
ssize_t iov_send_recv(int sockfd, const struct iovec* iov, unsigned iov_cnt,
					  size_t offset, size_t bytes, bool do_send);
#define iov_recv(sockfd, iov, iov_cnt, offset, bytes) \
	iov_send_recv(sockfd, iov, iov_cnt, offset, bytes, false)
#define iov_send(sockfd, iov, iov_cnt, offset, bytes) \
	iov_send_recv(sockfd, iov, iov_cnt, offset, bytes, true)

/**
 * Produce a text hexdump of iovec `iov' with `iov_cnt' number of elements
 * in file `fp', prefixing each line with `prefix' and processing not more
 * than `limit' data bytes.
 */
void iov_hexdump(const struct iovec* iov, const unsigned int iov_cnt,
				 FILE* fp, const char* prefix, size_t limit);

/*
 * Partial copy of vector from iov to dst_iov (data is not copied).
 * dst_iov overlaps iov at a specified offset.
 * size of dst_iov is at most bytes. dst vector count is returned.
 */
unsigned iov_copy(struct iovec* dst_iov, unsigned int dst_iov_cnt,
				  const struct iovec* iov, unsigned int iov_cnt,
				  size_t offset, size_t bytes);

/*
 * Remove a given number of bytes from the front or back of a vector.
 * This may update iov and/or iov_cnt to exclude iovec elements that are
 * no longer required.
 *
 * The number of bytes actually discarded is returned.  This number may be
 * smaller than requested if the vector is too small.
 */
size_t iov_discard_front(struct iovec** iov, unsigned int* iov_cnt,
						 size_t bytes);
size_t iov_discard_back(struct iovec* iov, unsigned int* iov_cnt,
						size_t bytes);

typedef struct QEMUIOVector
{
	struct iovec* iov;
	int niov;
	int nalloc;
	size_t size;
} QEMUIOVector;

void qemu_iovec_init(QEMUIOVector* qiov, int alloc_hint);
void qemu_iovec_init_external(QEMUIOVector* qiov, struct iovec* iov, int niov);
void qemu_iovec_add(QEMUIOVector* qiov, void* base, size_t len);
void qemu_iovec_concat(QEMUIOVector* dst,
					   QEMUIOVector* src, size_t soffset, size_t sbytes);
size_t qemu_iovec_concat_iov(QEMUIOVector* dst,
							 struct iovec* src_iov, unsigned int src_cnt,
							 size_t soffset, size_t sbytes);
bool qemu_iovec_is_zero(QEMUIOVector* qiov);
void qemu_iovec_destroy(QEMUIOVector* qiov);
void qemu_iovec_reset(QEMUIOVector* qiov);
size_t qemu_iovec_to_buf(QEMUIOVector* qiov, size_t offset,
						 void* buf, size_t bytes);
size_t qemu_iovec_from_buf(QEMUIOVector* qiov, size_t offset,
						   const void* buf, size_t bytes);
size_t qemu_iovec_memset(QEMUIOVector* qiov, size_t offset,
						 int fillc, size_t bytes);
ssize_t qemu_iovec_compare(QEMUIOVector* a, QEMUIOVector* b);
void qemu_iovec_clone(QEMUIOVector* dest, const QEMUIOVector* src, void* buf);
void qemu_iovec_discard_back(QEMUIOVector* qiov, size_t bytes);

#endif

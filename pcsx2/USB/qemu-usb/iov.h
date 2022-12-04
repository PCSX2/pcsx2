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

#pragma once

struct usb_iovec
{
	void* iov_base;
	size_t iov_len;
};

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
size_t iov_from_buf(const struct usb_iovec* iov, unsigned int iov_cnt,
						 size_t offset, const void* buf, size_t bytes);
size_t iov_to_buf(const struct usb_iovec* iov, const unsigned int iov_cnt,
					   size_t offset, void* buf, size_t bytes);

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
size_t iov_memset(const struct usb_iovec* iov, const unsigned int iov_cnt,
				  size_t offset, int fillc, size_t bytes);

typedef struct QEMUIOVector
{
	struct usb_iovec* iov;
	int niov;
	int nalloc;
	size_t size;
} QEMUIOVector;

void qemu_iovec_init(QEMUIOVector* qiov);
void qemu_iovec_add(QEMUIOVector* qiov, void* base, size_t len);
void qemu_iovec_destroy(QEMUIOVector* qiov);
void qemu_iovec_reset(QEMUIOVector* qiov);

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
#include "USB/qemu-usb/iov.h"
#include <algorithm>
#include <cassert>
#include <cstring>

 // TODO(Stenzek): Rewrite this stuff to LGPL.

size_t iov_from_buf(const struct usb_iovec* iov, unsigned int iov_cnt,
						 size_t offset, const void* buf, size_t bytes)
{
	size_t done;
	unsigned int i;
	for (i = 0, done = 0; (offset || done < bytes) && i < iov_cnt; i++)
	{
		if (offset < iov[i].iov_len)
		{
			size_t len = std::min(iov[i].iov_len - offset, bytes - done);
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

size_t iov_to_buf(const struct usb_iovec* iov, const unsigned int iov_cnt,
					   size_t offset, void* buf, size_t bytes)
{
	size_t done;
	unsigned int i;
	for (i = 0, done = 0; (offset || done < bytes) && i < iov_cnt; i++)
	{
		if (offset < iov[i].iov_len)
		{
			size_t len = std::min(iov[i].iov_len - offset, bytes - done);
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

size_t iov_memset(const struct usb_iovec* iov, const unsigned int iov_cnt,
				  size_t offset, int fillc, size_t bytes)
{
	size_t done;
	unsigned int i;
	for (i = 0, done = 0; (offset || done < bytes) && i < iov_cnt; i++)
	{
		if (offset < iov[i].iov_len)
		{
			size_t len = std::min(iov[i].iov_len - offset, bytes - done);
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


/* io vectors */

void qemu_iovec_init(QEMUIOVector* qiov)
{
	qiov->iov = (struct usb_iovec*)std::malloc(sizeof(usb_iovec));
	qiov->niov = 0;
	qiov->nalloc = 1;
	qiov->size = 0;
}

void qemu_iovec_add(QEMUIOVector* qiov, void* base, size_t len)
{
	assert(qiov->nalloc != -1);

	if (qiov->niov == qiov->nalloc)
	{
		qiov->nalloc = 2 * qiov->nalloc + 1;
		qiov->iov = (struct usb_iovec*)std::realloc(qiov->iov, sizeof(usb_iovec) * qiov->nalloc);
	}
	qiov->iov[qiov->niov].iov_base = base;
	qiov->iov[qiov->niov].iov_len = len;
	qiov->size += len;
	++qiov->niov;
}

void qemu_iovec_destroy(QEMUIOVector* qiov)
{
	assert(qiov->nalloc != -1);

	qemu_iovec_reset(qiov);
	std::free(qiov->iov);
	qiov->nalloc = 0;
	qiov->iov = NULL;
}

void qemu_iovec_reset(QEMUIOVector* qiov)
{
	assert(qiov->nalloc != -1);

	qiov->niov = 0;
	qiov->size = 0;
}

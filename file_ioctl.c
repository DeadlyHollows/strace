/*
 * Copyright (c) 2016 Jeff Mahoney <jeffm@suse.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "defs.h"
#include <sys/ioctl.h>
#include <linux/fs.h>

#ifdef HAVE_LINUX_FIEMAP_H
# include <linux/fiemap.h>
# include "xlat/fiemap_flags.h"
# include "xlat/fiemap_extent_flags.h"
#endif

#ifndef FICLONE
#define FICLONE         _IOW(0x94, 9, int)
#endif

#ifndef FICLONERANGE
#define FICLONERANGE    _IOW(0x94, 13, struct file_clone_range)
struct file_clone_range {
	int64_t src_fd;
	uint64_t src_offset;
	uint64_t src_length;
	uint64_t dest_offset;
};
#endif

#ifndef FIDEDUPERANGE
#define FIDEDUPERANGE   _IOWR(0x94, 54, struct file_dedupe_range)
struct file_dedupe_range_info {
	int64_t dest_fd;	/* in - destination file */
	uint64_t dest_offset;	/* in - start of extent in destination */
	uint64_t bytes_deduped;	/* out - total # of bytes we were able
				 * to dedupe from this file. */
	/* status of this dedupe operation:
	 * < 0 for error
	 * == FILE_DEDUPE_RANGE_SAME if dedupe succeeds
	 * == FILE_DEDUPE_RANGE_DIFFERS if data differs
	 */
	int32_t status;		/* out - see above description */
	uint32_t reserved;	/* must be zero */
};

struct file_dedupe_range {
	uint64_t src_offset;	/* in - start of extent in source */
	uint64_t src_length;	/* in - length of extent */
	uint16_t dest_count;	/* in - total elements in info array */
	uint16_t reserved1;	/* must be zero */
	uint32_t reserved2;	/* must be zero */
	struct file_dedupe_range_info info[0];
};
#endif

int
file_ioctl(struct tcb *tcp, const unsigned int code, const long arg)
{
	switch (code) {
	/* take a signed int */
	case FICLONE:	/* W */
		tprintf(", %d", (int)arg);
		break;

	case FICLONERANGE: { /* W */
		struct file_clone_range args;

		tprints(", ");

		if (umove_or_printaddr(tcp, arg, &args))
			break;

		tprintf("{src_fd=%" PRIi64 ", "
			"src_offset=%" PRIu64 ", "
			"src_length=%" PRIu64 ", "
			"dest_offset=%" PRIu64 "}",
			(int64_t)args.src_fd, (uint64_t)args.src_offset,
			(uint64_t)args.src_length, (uint64_t)args.dest_offset);
		break;
	}

	case FIDEDUPERANGE: { /* RW */
		struct file_dedupe_range args;
		uint64_t info_addr;
		uint16_t i;

		if (entering(tcp))
			tprints(", ");
		else if (syserror(tcp))
			break;
		else
			tprints(" => ");

		if (umove_or_printaddr(tcp, arg, &args))
			break;

		if (entering(tcp)) {
			tprintf("{src_offset=%" PRIu64 ", "
				"src_length=%" PRIu64 ", "
				"dest_count=%hu, info=",
				(uint64_t)args.src_offset,
				(uint64_t)args.src_length,
				(uint16_t)args.dest_count);
		} else
			tprints("{info=");

		if (abbrev(tcp)) {
			tprints("...}");
		} else {
			tprints("[");
			info_addr = arg + offsetof(typeof(args), info);
			for (i = 0; i < args.dest_count; i++) {
				struct file_dedupe_range_info info;
				uint64_t addr = info_addr + sizeof(info) * i;
				if (i)
					tprints(", ");

				if (umoven(tcp, addr, sizeof(info), &info)) {
					tprints("...");
					break;
				}

				if (entering(tcp))
					tprintf("{dest_fd=%" PRIi64 ", "
						"dest_offset=%" PRIu64 "}",
						(int64_t)info.dest_fd,
						(uint64_t)info.dest_offset);
				else {
					tprintf("{bytes_deduped=%" PRIu64 ", "
						"status=%d}",
						(uint64_t)info.bytes_deduped,
						info.status);
				}
			}
			tprints("]}");
		}
		if (entering(tcp))
			return 0;
		break;
	}

#ifdef HAVE_LINUX_FIEMAP_H
	case FS_IOC_FIEMAP: {
		struct fiemap args;
		struct fiemap_extent fe;
		unsigned int i;

		if (entering(tcp))
			tprints(", ");
		else if (syserror(tcp))
			break;
		else
			tprints(" => ");

		if (umove_or_printaddr(tcp, arg, &args))
			break;

		if (entering(tcp)) {
			tprintf("{fm_start=%" PRI__u64 ", "
				"fm_length=%" PRI__u64 ", "
				"fm_flags=",
				args.fm_start, args.fm_length);
			printflags64(fiemap_flags, args.fm_flags,
				     "FIEMAP_FLAG_???");
			tprintf(", fm_extent_count=%u}", args.fm_extent_count);
			return 0;
		}

		tprints("{fm_flags=");
		printflags64(fiemap_flags, args.fm_flags,
			     "FIEMAP_FLAG_???");
		tprintf(", fm_mapped_extents=%u",
			args.fm_mapped_extents);
		tprints(", fm_extents=");
		if (abbrev(tcp)) {
			tprints("...}");
			break;
		}

		tprints("[");
		for (i = 0; i < args.fm_mapped_extents; i++) {
			unsigned long offset;
			offset = offsetof(typeof(args), fm_extents[i]);
			if (i)
				tprints(", ");

			if (i > max_strlen ||
			    umoven(tcp, arg + offset, sizeof(fe), &fe)) {
				tprints("...");
				break;
			}

			tprintf("{fe_logical=%" PRI__u64
				", fe_physical=%" PRI__u64
				", fe_length=%" PRI__u64 ", ",
				fe.fe_logical, fe.fe_physical, fe.fe_length);

			printflags64(fiemap_extent_flags, fe.fe_flags,
				     "FIEMAP_EXTENT_???");
			tprints("}");
		}
		tprints("]}");
		break;
	}
#endif /* HAVE_LINUX_FIEMAP_H */

	default:
		return RVAL_DECODED;
	};

	return RVAL_DECODED | 1;
}
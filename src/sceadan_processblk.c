//===============================================================================================================//

//Copyright (c) 2012-2013 The University of Texas at San Antonio

//This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

//This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Public License for more details.

//You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

//Written by: 
//Dr. Nicole Beebe and Lishu Liu, Department of Information Systems and Cyber Security (nicole.beebe@utsa.edu)
//Laurence Maddox, Department of Computer Science
//University of Texas at San Antonio
//One UTSA Circle 
//San Antonio, Texas 78209

//===============================================================================================================//

#include <assert.h>
#include <ftw.h>
#include <stdbool.h>
#include <stdio.h>


#include "file_type.h"
#include "restart.h"
#include "sceadan_processblk.h"
#include "sceadan_predict.h"

/* LINKED INCLUDES */                                      /* LINKED INCLUDES */
#include <math.h>
/* END LINKED INCLUDES */                              /* END LINKED INCLUDES */

#ifdef RESEARCH
	#include <bzlib.h>
	#include "zlib.h"
#endif





/* number of open file descriptors for ftw
   TODO tune this parameter */
#define FTW_NOPENFD (7)

///////////////////////////////////////////////////////////////////////////////////////////////////
#define BZ2_BLOCK_SZ     (9)
#define BZ2_WORK_FACTOR  (0)

#define ZL_LEVEL (9)
#define WASTE_SZ (4096)
#define W "-10"
////////////////////////////////////////////////////////////////////////////////////////////////////





/* FUNCTIONS THAT SHOULD BE GCC BUILT INS */
static size_t
min (
	const size_t a,
	const size_t b
) {
	return a < b ? a : b;
}

static sum_t
max (
	const sum_t a,
	const sum_t b
) {
	return a > b ? a : b;
}
/* END FUNCTIONS THAT SHOULD BE GCC BUILT INS */




/* FUNCTIONS FOR PROCESSING DIRECTORIES */
// TODO full path vs relevant path may matter
int
process_dir (
	const          char path[],
	const unsigned int  block_factor,
	const output_f      do_output,
	      FILE *const outs[3],
	      file_type_e file_type
) {

	// dys-functional programming
	int
	ftw_callback (
		const char                fpath[],
		const struct stat *const sb,
		const int                 typeflag
	) {
		switch (typeflag) {
		case FTW_D: /* directory */
			break;
		case FTW_DNR: /* non-traversable */
			// TODO logging ?
			break;
		case FTW_F: /* normal file */
			// if it works, it'll work fast (by an insignificant amount)
			if (__builtin_expect (
				process_file (fpath, block_factor, do_output, outs, file_type) != 0,
				true
			)) {
				// TODO logging ?
			}
		}
		return 0;
	}

	// http://voyager.deanza.edu/~perry/ftw.html
	return ftw (path, &ftw_callback, FTW_NOPENFD);
}
/* END FUNCTIONS FOR PROCESSING DIRECTORIES */


/* FUNCTIONS FOR PROCESSING FILES */
int
process_file (
	const          char path[],
	const unsigned int  block_factor,
	const output_f      do_output,
	      FILE *const outs[3],
	      file_type_e file_type
) {
	return block_factor
	?       process_blocks    (path, block_factor, do_output, outs, file_type)
	:       process_container (path,               do_output, outs, file_type);
}
/* END FUNCTIONS FOR PROCESSING FILES */


/* FUNCTIONS FOR PROCESSING BLOCKS */
static int
process_blocks0 (
	const char          path[],
	const fd_t          fd,
	const unsigned int  block_factor,
	const output_f      do_output,
         FILE         *const outs[3],
	      file_type_e file_type
) {
#ifdef RESEARCH
	static char waste[WASTE_SZ];
#endif
	size_t offset = 0;

	while (true) {
		ucv_t ucv = UCV_LIT;
		bcv_t bcv = BCV_LIT;
		mfv_t mfv = MFV_BLOCK_LIT;

		mfv.id_container = path;
		mfv.id_block = offset;
		
#ifdef RESEARCH
		bz_stream bz;
		bz.bzalloc = NULL;
		bz.bzfree  = NULL;
		bz.opaque  = NULL;

		if (__builtin_expect (BZ2_bzCompressInit (&bz, BZ2_BLOCK_SZ, 0, 
			                                       BZ2_WORK_FACTOR) != BZ_OK,
				                false)) {
			// TODO
			return 1;
		}
		//////////////////////
		unsigned zl_have;
		z_stream zl_strm;

		/* allocate deflate state */
		zl_strm.zalloc = Z_NULL;
		zl_strm.zfree  = Z_NULL;
		zl_strm.opaque = Z_NULL;
		if (deflateInit (&zl_strm, ZL_LEVEL) != Z_OK)
			return 1;
#endif

		sum_t     last_cnt = 0;
		unigram_t last_val;


		size_t tot;
		for (tot = 0; tot != block_factor; ) {
			char   buf[BUFSIZ];
			size_t rd = r_read (fd, buf, min (block_factor - tot, BUFSIZ));
			switch (rd) {
			case -1:
				//VERBOSE_OUTPUT(
					fprintf (stderr, "fail: r_read ()\n");
				//);
				return 1;
			case  0:
				if (tot != 0) {
				//	vectors_update ((unigram_t *) buf, tot, ucv, bcv, &mfv, &last_cnt, &last_val, &max_cnt);

#ifdef RESEARCH
					int bzret;
					while (true) {
						bz.next_out  = waste;
						bz.avail_out = WASTE_SZ;

						bzret = BZ2_bzCompress (&bz, BZ_FINISH);
						if (bzret == BZ_FINISH_OK)
							continue;

						if (bzret == BZ_STREAM_END)
							break;

						// TODO
						return 1;
					}

					if (__builtin_expect (BZ2_bzCompressEnd (&bz) != BZ_OK, false)) {
						// TODO
						return 1;
					}

					mfv.bzip2_len.tot = ((uint64_t) bz.total_out_hi32 << 32) + bz.total_out_lo32;


					/* run deflate() on input until output buffer not full, finish
						compression if all of source has been read in */
					do {
						zl_strm.avail_out = WASTE_SZ;
						zl_strm.next_out  = (unsigned char *const) waste;
						int ret = deflate (&zl_strm, Z_FINISH);    /* no bad return value */
						assert (ret != Z_STREAM_ERROR);  /* state not clobbered */
						zl_have = WASTE_SZ - zl_strm.avail_out;

						mfv.lzw_len.tot += zl_have;
					} while (zl_strm.avail_out == 0);
					assert(zl_strm.avail_in == 0);     /* all input will be used */

					(void)deflateEnd(&zl_strm);
#endif


					vectors_finalize (ucv, bcv, &mfv/*, max_cnt*/);

					switch (file_type) {
						case UNCLASSIFIED:
							if (__builtin_expect (predict_liblin (ucv, (const cv_e (*const)[n_unigram]) bcv, &mfv, &file_type) != 0, false)) {
								return 6;
							}
						//	break;
						default:
							if (__builtin_expect (do_output (ucv, (const cv_e (*const)[n_unigram]) bcv, &mfv, outs, file_type) != 0, false)) {
								return 5;
							}
						}
				}

				return 0; // break from while-loop
			default:

#ifdef RESEARCH
				bz.next_in   = buf;
				bz.avail_in  = rd;
				bz.next_out  = waste;
				bz.avail_out = WASTE_SZ;
				if (__builtin_expect (BZ2_bzCompress (&bz, BZ_RUN) != BZ_RUN_OK, false)) {
					// TODO
					return 1;
				}

				int bzret;
				while (true) {
					bz.next_out  = waste;
					bz.avail_out = WASTE_SZ;

					bzret = BZ2_bzCompress (&bz, BZ_FLUSH);
					if (bzret == BZ_FLUSH_OK)
						continue;

					if (bzret == BZ_RUN_OK)
						break;

					// TODO
					return 1;
				}

			zl_strm.avail_in = rd;
			zl_strm.next_in  = (unsigned char *const) buf;

			/* run deflate() on input until output buffer not full, finish
			   compression if all of source has been read in */
			do {
				zl_strm.avail_out = WASTE_SZ;
				zl_strm.next_out  = (unsigned char *const) waste;
				int ret = deflate (&zl_strm, Z_NO_FLUSH);    /* no bad return value */
				assert (ret != Z_STREAM_ERROR);  /* state not clobbered */
				zl_have = WASTE_SZ - zl_strm.avail_out;

				mfv.lzw_len.tot += zl_have;
			} while (zl_strm.avail_out == 0);
			assert(zl_strm.avail_in == 0);     /* all input will be used */
#endif

				vectors_update ((unigram_t *) buf, rd, ucv, bcv, &mfv, &last_cnt, &last_val/*, &max_cnt*/);

				tot += rd;
				// continue
			} // end switch
		} // end for

#ifdef RESEARCH
		int bzret;
		while (true) {
			bz.next_out  = waste;
			bz.avail_out = WASTE_SZ;

			bzret = BZ2_bzCompress (&bz, BZ_FINISH);
			if (bzret == BZ_FINISH_OK)
				continue;

			if (bzret == BZ_STREAM_END)
				break;

			// TODO
			return 1;
		}

		if (__builtin_expect (BZ2_bzCompressEnd (&bz) != BZ_OK, false)) {
			// TODO
			return 1;
		}

		mfv.bzip2_len.tot = ((uint64_t) bz.total_out_hi32 << 32) + bz.total_out_lo32;


		/* run deflate() on input until output buffer not full, finish
			compression if all of source has been read in */
		do {
			zl_strm.avail_out = WASTE_SZ;
			zl_strm.next_out  = (unsigned char *const) waste;
			int ret = deflate (&zl_strm, Z_FINISH);    /* no bad return value */
			assert (ret != Z_STREAM_ERROR);  /* state not clobbered */
			zl_have = WASTE_SZ - zl_strm.avail_out;

			mfv.lzw_len.tot += zl_have;
		} while (zl_strm.avail_out == 0);
		assert(zl_strm.avail_in == 0);     /* all input will be used */

		(void)deflateEnd(&zl_strm);
#endif

		vectors_finalize (ucv, bcv, &mfv/*, max_cnt*/);

		switch (file_type) {
		case UNCLASSIFIED:
			if (__builtin_expect (predict_liblin (ucv, (const cv_e (*const)[n_unigram]) bcv, &mfv, &file_type) != 0, false)) {
				return 6;
			}
		//	break;
		default:
			if (__builtin_expect (do_output (ucv, (const cv_e (*const)[n_unigram]) bcv, &mfv, outs, file_type) != 0, false)) {
				return 5;
			}
		}

		offset += tot;
	} // end while
	__builtin_unreachable ();
}

int
process_blocks (
	const char         path[],
	const unsigned int block_factor,
	const output_f     do_output,
	      FILE     *const outs[3],
	      file_type_e file_type
) {
	const fd_t fd = r_open2 (path, O_RDONLY);
	if (__builtin_expect (fd == -1, false)) {
		//VERBOSE_OUTPUT(
			fprintf (stderr, "fail: r_open2 ()\n");
		//)
		return 2;
	}

	if (__builtin_expect (
		process_blocks0 (path, fd, block_factor, do_output, (FILE *const *const) outs, file_type) != 0,
		false
	)) {
		r_close (fd);
		return 3;
	}

	if (__builtin_expect (r_close (fd) == -1, false)) {
		//VERBOSE_OUTPUT(
			fprintf (stderr, "fail: r_close ()\n");
		//)
		return 4;
	}

	return 0;
}
/* END FUNCTIONS FOR PROCESSING BLOCKS */


/* FUNCTIONS FOR PROCESSING CONTAINERS */

/* implements a multi-level break */
static int
process_container0 (
	const fd_t             fd,
	      ucv_t            ucv,
	      bcv_t            bcv,
	      mfv_t     *const mfv,

	      sum_t     *const last_cnt,
	      unigram_t *const last_val//,
) {

#ifdef RESEARCH
	static char waste[WASTE_SZ];

	bz_stream bz;
	bz.bzalloc = NULL;
	bz.bzfree  = NULL;
	bz.opaque  = NULL;

	if (__builtin_expect (BZ2_bzCompressInit (&bz, BZ2_BLOCK_SZ, 0, 
		                                       BZ2_WORK_FACTOR) != BZ_OK,
			                false)) {
		// TODO
		return 1;
	}
	//////////////////////
	//int      zl_flush;
	unsigned zl_have;
	z_stream zl_strm;

	/* allocate deflate state */
	zl_strm.zalloc = Z_NULL;
	zl_strm.zfree  = Z_NULL;
	zl_strm.opaque = Z_NULL;
	if (deflateInit (&zl_strm, ZL_LEVEL) != Z_OK)
		return 1;
#endif

	while (true) {
		char    buf[BUFSIZ];
		const ssize_t rd = r_read (fd, buf, sizeof (buf));
		switch (rd) {
		case -1:
			//VERBOSE_OUTPUT(
				fprintf (stderr, "fail: r_read ()\n");
			//);
			return 1;
		case  0:

#ifdef RESEARCH
			/*if (rd != 0)*/ {
					int bzret;
					while (true) {
						bz.next_out  = waste;
						bz.avail_out = WASTE_SZ;

						bzret = BZ2_bzCompress (&bz, BZ_FINISH);
						if (bzret == BZ_FINISH_OK)
							continue;

						if (bzret == BZ_STREAM_END)
							break;

						// TODO
						return 1;
					}

					if (__builtin_expect (BZ2_bzCompressEnd (&bz) != BZ_OK, false)) {
						// TODO
						return 1;
					}

					mfv->bzip2_len.tot = ((uint64_t) bz.total_out_hi32 << 32) + bz.total_out_lo32;

					zl_strm.avail_in = rd;
					zl_strm.next_in  = (unsigned char *const) buf;

					/* run deflate() on input until output buffer not full, finish
						compression if all of source has been read in */
					do {
						zl_strm.avail_out = WASTE_SZ;
						zl_strm.next_out  = (unsigned char *const) waste;
						int ret = deflate (&zl_strm, Z_FINISH);    /* no bad return value */
						assert (ret != Z_STREAM_ERROR);  /* state not clobbered */
						zl_have = WASTE_SZ - zl_strm.avail_out;

						mfv->lzw_len.tot += zl_have;
					} while (zl_strm.avail_out == 0);
					assert(zl_strm.avail_in == 0);     /* all input will be used */

					(void)deflateEnd(&zl_strm);
			}
			#endif

			return 0; // break from while-loop

		default:

#ifdef RESEARCH
			bz.next_in   = buf;
			bz.avail_in  = rd;
			bz.next_out  = waste;
			bz.avail_out = WASTE_SZ;
			if (__builtin_expect (BZ2_bzCompress (&bz, BZ_RUN) != BZ_RUN_OK, false)) {
				// TODO
				return 1;
			}

			int bzret;
			while (true) {
				bz.next_out  = waste;
				bz.avail_out = WASTE_SZ;

				bzret = BZ2_bzCompress (&bz, BZ_FLUSH);
				if (bzret == BZ_FLUSH_OK)
					continue;

				if (bzret == BZ_RUN_OK)
					break;

				// TODO
				return 1;
			}

			zl_strm.avail_in = rd;
			zl_strm.next_in  = (unsigned char *const) buf;

			/* run deflate() on input until output buffer not full, finish
			   compression if all of source has been read in */
			do {
				zl_strm.avail_out = WASTE_SZ;
				zl_strm.next_out  = (unsigned char *const) waste;
				int ret = deflate (&zl_strm, Z_NO_FLUSH);    /* no bad return value */
				assert (ret != Z_STREAM_ERROR);  /* state not clobbered */
				zl_have = WASTE_SZ - zl_strm.avail_out;

				mfv->lzw_len.tot += zl_have;
			} while (zl_strm.avail_out == 0);
			assert(zl_strm.avail_in == 0);     /* all input will be used */
#endif
			vectors_update ((unigram_t *) buf, rd, ucv, bcv, mfv, last_cnt, last_val/*, max_cnt*/);
			// continue
		}
	}
	__builtin_unreachable ();
}

int
process_container (
	const char            path[],
	const output_f        do_output,
	      FILE     *const outs[3],
	      file_type_e file_type
) {
	ucv_t ucv = UCV_LIT;
	bcv_t bcv = BCV_LIT;
	mfv_t mfv = MFV_CONTAINER_LIT;

	mfv.id_container = path;

	sum_t     last_cnt = 0;
	unigram_t last_val;

	const fd_t fd = r_open2 (path, O_RDONLY);
	if (__builtin_expect (fd == -1, false)) {
		//VERBOSE_OUTPUT(
			fprintf (stderr, "fail: r_open2 ()\n");
		//)
		return 2;
	}

	if (__builtin_expect (
		process_container0 (fd, ucv, bcv, &mfv,
		                    &last_cnt, &last_val) != 0,
		false
	)) {
		r_close (fd);
		return 3;
	}

	if (__builtin_expect (r_close (fd) == -1, false)) {
			fprintf (stderr, "fail: r_close ()\n");
		return 4;
	}

	vectors_finalize (ucv, bcv, &mfv);

	switch (file_type) {
		case UNCLASSIFIED:
			if (__builtin_expect (predict_liblin (ucv, (const cv_e (*const)[n_unigram]) bcv, &mfv, &file_type) != 0, false)) {
				return 6;
			}
			//break;
		default:
			if (__builtin_expect (do_output (ucv, (const cv_e (*const)[n_unigram]) bcv, &mfv, outs, file_type) != 0, false)) {
				return 5;
			}
	}

	return 0;
}
/* END FUNCTIONS FOR PROCESSING CONTAINERS */


/* FUNCTIONS FOR VECTORS */
void
vectors_update (
	const unigram_t        buf[],
	const size_t           sz,
	      ucv_t            ucv,
	      bcv_t            bcv,
	      mfv_t     *const mfv,

	      sum_t     *const last_cnt,
	      unigram_t *const last_val
) {
	const int sz_mod = mfv->uni_sz % 2;

	size_t ndx;
	for (ndx = 0; ndx < sz; ndx++) {

		// ucv
		const unigram_t unigram = buf[ndx];
		ucv[unigram].tot++;

		{
			unigram_t prev;
			unigram_t next;

			if (ndx == 0 && sz_mod != 0) {
				prev = *last_val;
				next = unigram;

				bcv[prev][next].tot++;
//#ifdef RESEARCH
				mfv->contiguity.tot += abs (next - prev);
//#endif
			} else if (ndx + 1 < sz) {
				prev = unigram;
				next = buf[ndx + 1];
//#ifdef RESEARCH
				mfv->contiguity.tot += abs (next - prev);
//#endif
				if (ndx % 2 == sz_mod)
					bcv[prev][next].tot++;
			}
		}

//#ifdef RESEARCH
		// total count of set bits (for hamming weight)
		// this is wierd
		mfv->hamming_weight.tot += (nbit_unigram - __builtin_popcount (unigram));

		// byte value sum
		mfv->byte_value.tot += unigram;

		// standard deviation of byte values
		mfv->stddev_byte_val.tot += __builtin_powi ((double) unigram, 2);

		if (*last_cnt != 0
		 && *last_val == unigram)
			(*last_cnt)++;
		else {
			*last_cnt = 1;
			*last_val = unigram;
		}
		mfv->max_byte_streak.tot = max (*last_cnt, mfv->max_byte_streak.tot);

		// count of low ascii values
		if       (unigram < ASCII_LO_VAL)
			mfv->lo_ascii_freq.tot++;

		// count of medium ascii values
		else if (unigram < ASCII_HI_VAL)
			mfv->med_ascii_freq.tot++;

		// count of high ascii values
		else
			mfv->hi_ascii_freq.tot++;
//#endif
	}

	// item size
	mfv->uni_sz += sz;
}

void
vectors_finalize (
	      ucv_t        ucv,
	      bcv_t        bcv,
	      mfv_t *const mfv
) {
//#ifdef RESEARCH
	// hamming weight
	mfv->hamming_weight.avg = (double) mfv->hamming_weight.tot
	                        / (mfv->uni_sz * nbit_unigram);

	// mean byte value
	mfv->byte_value.avg = (double) mfv->byte_value.tot / mfv->uni_sz;

	// average contiguity between bytes
	mfv->contiguity.avg = (double) mfv->contiguity.tot / mfv->uni_sz;

	// max byte streak
	//mfv->max_byte_streak = max_cnt;
	mfv->max_byte_streak.avg = (double) mfv->max_byte_streak.tot / mfv->uni_sz;

	// TODO skewness ?
	double expectancy_x3 = 0;
	double expectancy_x4 = 0;

	const double central_tendency = mfv->byte_value.avg;
//#endif
	size_t i;
	for (i = 0; i < n_unigram; i++) {

//#ifdef RESEARCH
		mfv->abs_dev += ucv[i].tot * fabs (i - central_tendency);
//#endif
		{
			// unigram frequency
			ucv[i].avg = (double) ucv[i].tot / mfv->uni_sz;

			// item entropy
			const double pv = ucv[i].avg;
			if (pv != 0) // TODO floating point mumbo jumbo
				mfv->item_entropy += pv * log2 (1 / pv) / nbit_unigram; // more divisions for accuracy
		}

		size_t j;
		for (j = 0; j < n_unigram; j++) {

			bcv[i][j].avg = (double) bcv[i][j].tot / (mfv->uni_sz / 2); // rounds down

			// bigram entropy
			const double pv = bcv[i][j].avg;
			if (pv != 0) // TODO
				mfv->bigram_entropy  += pv * log2 (1 / pv) / nbit_bigram;
		}
//#ifdef RESEARCH
		{
			const double extmp = __builtin_powi ((double) i, 3) * ucv[i].avg;

			// for skewness
			expectancy_x3 += extmp;

			// for kurtosis
			expectancy_x4 += extmp * i;
		}
//#endif
	}
//#ifdef RESEARCH
	const double variance  = (double) mfv->stddev_byte_val.tot / mfv->uni_sz
	                       - __builtin_powi (mfv->byte_value.avg, 2);

	mfv->stddev_byte_val.avg = sqrt (variance);

	const double sigma3    = variance * mfv->stddev_byte_val.avg;
	const double variance2 = __builtin_powi (variance, 2);

	// average absolute deviation
	mfv->abs_dev /= mfv->uni_sz;
	mfv->abs_dev /= n_unigram;

	// skewness
	mfv->skewness = (expectancy_x3
	               - mfv->byte_value.avg * (3 * variance
	                                      + __builtin_powi (mfv->byte_value.avg,
	                                                        2)))
	              / sigma3;

	// kurtosis
	if (__builtin_expect (isinf (expectancy_x4), false)) {
		fprintf (stderr, "isinf (expectancy_x4)\n");
		abort ();
	}
	if (__builtin_expect (isinf (variance2), false)) {
		fprintf (stderr, "isinf (variance2)\n");
		abort ();
	}

	mfv->kurtosis = (expectancy_x4 / variance2);
	mfv->byte_value.avg      /= n_unigram;
	mfv->stddev_byte_val.avg /= n_unigram;
	mfv->kurtosis            /= n_unigram;
	mfv->contiguity.avg      /= n_unigram;

	if (mfv->bzip2_len.tot < mfv->uni_sz)
		mfv->bzip2_len.avg        = (double) mfv->bzip2_len.tot / mfv->uni_sz;
	else
		mfv->bzip2_len.avg        = 1;

	if (mfv->lzw_len.tot   < mfv->uni_sz)
		mfv->lzw_len.avg          = (double) mfv->lzw_len.tot   / mfv->uni_sz;
	else
		mfv->lzw_len.avg          = 1;

	mfv->lo_ascii_freq.avg  = (double) mfv->lo_ascii_freq.tot  / mfv->uni_sz;
	mfv->med_ascii_freq.avg = (double) mfv->med_ascii_freq.tot / mfv->uni_sz;
	mfv->hi_ascii_freq.avg  = (double) mfv->hi_ascii_freq.tot  / mfv->uni_sz;
//#endif
}
/* END FUNCTIONS FOR VECTORS */

// prints finalized vectors
int
output_competition (
	const ucv_t        ucv,
	const bcv_t        bcv,
	const mfv_t *const mfv,
	      FILE  *const unused[3],
	      file_type_e file_type
) {

	switch (mfv->id_type) {
	case ID_CONTAINER: //break;
	case ID_BLOCK:
		printf ("%"W"zu ", mfv->id_block);
		break;
	default:
		// TODO
		abort ();
	}

	switch (file_type) {
	case UNCLASSIFIED:
		// TODO
		fflush(stdout);
		abort ();

	/* file type description: Plain text
	   file extensions: .text, .txt */
	case TEXT:
		printf ("txt");
		break;

	/* file type description: Delimited
	   file extensions: .csv */
	case CSV:
		printf ("csv");
		break;

	/* file type description: Log files
	   file extensions: .log */
	case LOG:
		printf ("log");
		break;

	/* file type description: HTML
	   file extensions: .html */
	case HTML:
		printf ("html");
		break;

	/* file type description: xml
	   file extensions: .xml */
	case XML:
		printf ("xml");
		break;

	case ASPX: printf ("aspx"); break;

	/* file type description: css
	   file extensions: .css */
	case CSS:
		printf ("css");
		break;

	/* file type description: JavaScript code
	   file extensions: .js */
	case JS:
		printf ("js");
		break;

	/* file type description: JSON records
	   file extensions: .json */
	case JSON:
		printf ("json");
		break;

	case B64: printf ("b64"); break;
	case A85: printf ("a85"); break;
	case B16: printf ("b16"); break;
	case URL: printf ("url"); break;
	case PS:  printf ("ps");  break;
	case RTF: printf ("rtf"); break;
	case TBIRD: printf ("tbird"); break;
	case PST: printf ("pst"); break;
	case PNG: printf ("png"); break;
	case GIF: printf ("gif"); break;
	case TIF: printf ("tif"); break;
	case JB2: printf ("jb2"); break;
	case GZ:  printf ("gz");  break;
	case ZIP: printf ("zip"); break;
	case JAR: printf ("jar"); break;
	case RPM: printf ("rpm"); break;
	case BZ2: printf ("bz2"); break;
	case PDF: printf ("pdf"); break;
	case DOCX: printf ("docx"); break;
	case XLSX: printf ("xlsx"); break;
	case PPTX: printf ("pptx"); break;
	case MP3: printf ("mp3"); break;
	case M4A: printf ("m4a"); break;
	case MP4: printf ("mp4"); break;
	case AVI: printf ("avi"); break;
	case WMV: printf ("wmv"); break;
	case FLV: printf ("flv"); break;
	case SWF: printf ("swf"); break;
	case WAV: printf ("wav"); break;
	case WMA: printf ("wma"); break;
	case MOV: printf ("mov"); break;
	case DOC: printf ("doc"); break;
	case XLS: printf ("xls"); break;
	case PPT: printf ("ppt"); break;
	case FAT: printf ("fat"); break;
	case NTFS: printf ("ntfs"); break;
	case EXT3: printf ("ext3"); break;
	case EXE: printf ("exe"); break;
	case DLL: printf ("dll"); break;
	case ELF: printf ("elf"); break;
	case BMP: printf ("bmp"); break;
	case AES: printf ("aes"); break;
	case RAND: printf ("rand"); break;
	case PPS: printf ("pps"); break;
	case UCV_CONST: printf ("ucv_const"); break;
	case BCV_CONST: printf ("bcv_const"); break;
	case TCV_CONST: printf ("tcv_const"); break;



	/* file type description: JPG
	   file extensions: .jpg, .jpeg */
	case JPG:
		printf ("jpg");
		break;

	/* file type description: Java Source Code
	   file extensions: .java */
	case JAVA:
		printf ("java");
		break;
	}

	switch (mfv->id_type) {
	case ID_CONTAINER:
	case ID_BLOCK:
		printf (" # %s", mfv->id_container);
		break;
	default:
		// TODO
		abort ();
	}

	puts ("");

	return 0;
	
}
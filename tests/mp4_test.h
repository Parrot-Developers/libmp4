/**
 * Copyright (c) 2018 Parrot Drones SAS
 * Copyright (c) 2016 Aurelien Barre
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the copyright holders nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MP4_TEST_H_
#define _MP4_TEST_H_

#include <libmp4.h>

#include <CUnit/Automated.h>
#include <CUnit/Basic.h>
#include <CUnit/CUnit.h>

#include "mp4_priv.h"
#include <errno.h>
#include <futils/futils.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* codecheck_ignore[COMPLEX_MACRO] */
#define FN(_name) (char *)_name

#define GET_PATH(_dest, _index, _array, _type)                                 \
	do {                                                                   \
		CU_ASSERT_FATAL(_index < FUTILS_SIZEOF_ARRAY(_array));         \
		snprintf(_dest,                                                \
			 sizeof(_dest),                                        \
			 "%s/%s",                                              \
			 (getenv("ASSETS_ROOT") != NULL)                       \
				 ? getenv("ASSETS_ROOT")                       \
				 : ASSETS_ROOT,                                \
			 _array[_index]._type);                                \
		int _file_read_access = access(_dest, R_OK);                   \
		CU_ASSERT_EQUAL_FATAL(_file_read_access, 0);                   \
	} while (0)


#define ASSETS_ROOT "/mnt/DFS/MULTIMEDIA_DATA"
#define TEST_FILE_PATH "/tmp/test_mux.MP4"
#define TEST_FILE_PATH_MRF "/tmp/test_mux.MRF"
#define TEST_FILE_PATH_CHK "/tmp/test_mux.CHK"


extern CU_TestInfo g_mp4_test_demux[];
extern CU_TestInfo g_mp4_test_mux[];
extern CU_TestInfo g_mp4_test_recovery[];
extern CU_TestInfo g_mp4_test_utilities[];


#endif /* _MP4_TEST_H_ */

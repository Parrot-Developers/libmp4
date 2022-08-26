/**
 * Copyright (C) 2013 Parrot S.A.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * libulog: a minimalistic logging library derived from Android logger
 *
 */

#ifndef _PARROT_ULOG_H
#define _PARROT_ULOG_H

/*
 * HOW TO USE ULOG:
 * ----------------
 *
 * 1. Declare one or several ULOG tag names in a .c or .cpp source file, like
 * this:
 *
 *   #include "ulog.h"
 *   ULOG_DECLARE_TAG(toto);
 *   ULOG_DECLARE_TAG(Foo_Bar);
 *
 * Note that the argument of ULOG_DECLARE_TAG() is the tag name and should be a
 * valid C symbol string, such as 'my_module', 'MyTag', etc.
 *
 * 2. Then, set the default tag to use inside a given source file by defining
 * macro ULOG_TAG before including "ulog.h":
 *
 *   #define ULOG_TAG Foo_Bar
 *   #include "ulog.h"
 *
 * 3. You can now use short macros for logging:
 *
 *   ULOGW("This module will auto-destruct in %d seconds...\n", 3);
 *   ULOGE("Fatal error\n");
 *
 * If you forget to define macro ULOG_TAG, then a default empty tag is used.
 *
 * NOTE: If you need to log messages from a signal handler, make sure a first
 * message using the tag is logged at runtime before installing your handler.
 *
 * HOW TO CONTROL ULOG LOGGING LEVEL:
 * ----------------------------------
 * ULOG logging is globally controlled by environment variable ULOG_LEVEL.
 * This variable should contain a single letter ('C', 'E', 'W', 'N', 'I', or
 * 'D') or, alternatively, a single digit with an equivalent meaning:
 *
 * C = Critical = 2
 * E = Error    = 3
 * W = Warning  = 4
 * N = Notice   = 5
 * I = Info     = 6
 * D = Debug    = 7
 *
 * For instance, to enable all priorities up to and including the 'Warning'
 * level, you should set:
 * ULOG_LEVEL=W
 * or, equivalently,
 * ULOG_LEVEL=4
 *
 * The default logging level is 'I', i.e. all priorities logged except Debug.
 * ULOG_LEVEL controls logging levels globally for all tags.
 * Setting an empty ULOG_LEVEL string disables logging completely.
 * You can also control the logging level of a specific tag by defining
 * environment variable ULOG_LEVEL_<tagname>. For instance:
 *
 * ULOG_LEVEL_Foo_Bar=D   (set level Debug for tag 'Foo_Bar')
 *
 * The above environment variables are read only once, before the first use of
 * a tag. To dynamically change a logging level at any time, you can use macro
 * ULOG_SET_LEVEL() like this:
 *
 *   ULOG_SET_LEVEL(ULOG_DEBUG);
 *   ULOGD("This debug message will be logged.");
 *   ULOG_SET_LEVEL(ULOG_INFO);
 *   ULOGD("This debug message will _not_ be logged.");
 *   ULOGI("But this one will be.");
 *
 * ULOG_SET_LEVEL() takes precedence over ULOG_LEVEL_xxx environment variables.
 * If ULOG_SET_LEVEL() is used without a defined ULOG_TAG, then it sets the
 * default logging level used when no environment variable is defined.
 * ULOG_GET_LEVEL() returns the current logging level of the default tag defined
 * with ULOG_TAG.
 *
 * If you need to dynamically control the logging level of an external tag, i.e.
 * a tag not declared in your code (for instance declared and used in a library
 * to which your code is linked), you can use the following function (assuming
 * the tag is 'foobar'):
 *
 *   ulog_set_tag_level("foobar", ULOG_WARN);
 *
 * There is a restriction to the above code: the tag will be accessible and
 * controllable at runtime only after its has been used at least once. This is
 * because the tag "registers" itself during its first use, and remains unknown
 * until it does so. A library can make sure its tags are externally visible
 * by forcing early tag registration with macro ULOG_INIT() like this:
 *
 *   ULOG_INIT(foobar);
 *   // at this point, tag 'foobar' logging level is externally controllable
 *
 * This can be done typically during library initialization.
 * You can also dynamically list 'registered' tags at runtime with function
 * ulog_get_tag_names().
 *
 * HOW TO CONTROL ULOG OUTPUT DEVICE
 * ---------------------------------
 * To control which kernel logging device is used, use environment variable
 * ULOG_DEVICE; for instance:
 *
 * ULOG_DEVICE=balboa  (default device is 'main')
 *
 * To enable printing a copy of each message to stderr:
 * ULOG_STDERR=y
 */
#define ULOG_ERRNO(_err, msg, ...) do{} while(0) //printf("%d %s", _err, msg)
#define ULOGE_ERRNO(_err, _fmt, ...) do{} while(0) // printf("%d %s", _err, _fmt)
#define ULOGE(...) printf("ULOGE")
#define ULOGC(...) printf("ULOGC")
#define ULOGD(...) do{} while(0) //printf("ULOGD")
#define ULOGI(...) printf("ULOGI")
#define ULOGW(...) printf("ULOGW")
#define ULOGN(...) printf("ULOGN")
#define ULOG_ERRNO_RETURN_ERR_IF(cond, errno)                          \
		do {                                                           \
			if (cond) {                                            \
				printf("ULOG_ERRNO_RETURN_ERR_IF %d", errno);  \
				return -errno;                                 \
			}                                                      \
		} while (0)

#define ULOG_ERRNO_RETURN_VAL_IF(_cond, _err, _val)                    \
		do {                                                           \
			if (_cond) {                                           \
				ULOGE_ERRNO((_err), "");                       \
				/* codecheck_ignore[RETURN_PARENTHESES] */     \
				return (_val);                                 \
			}                                                      \
		} while (0)

#define ULOG_ERRNO_RETURN_IF(_cond, _err)                              \
		do {                                                           \
			if (_cond) {                                           \
				ULOGE_ERRNO((_err), "");                       \
				return;                                        \
			}                                                      \
		} while (0)


#define ULOG_PRI(_prio, ...) printf("ULOG_PRI")

#endif /* _PARROT_ULOG_H */

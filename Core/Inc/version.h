/*
 * version.h
 *
 *  Created on: Sep 29, 2022
 *      Author: Jim Merkle
 */

#ifndef SRC_VERSION_H__
#define SRC_VERSION_H__

#define VERSION_MAJOR	1
#define VERSION_MINOR   0
#define VERSION_BUILD	0

// Definition of a 32-bit version number:
typedef struct {
	uint32_t major : 8;
	uint32_t minor : 8;
	uint32_t build: 16;
} VERSION_MAJOR_MINOR;


extern const VERSION_MAJOR_MINOR fw_version; // command_line.c
extern char szversion[];                     // command_line.c

#endif /* SRC_VERSION_H__ */

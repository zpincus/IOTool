/*
 * Copyright 2012 Alan Burlison, alan@bleaklow.com.  All rights reserved.
 * Use is subject to license terms.  See LICENSE.txt for details.
 */

/*
 * AVR-related utilities.
 */

#ifndef utils_h
#define	utils_h

#include <avr/io.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>


/*
 * Useful numeric constants.
 */

#ifndef INT8_MAX
#define	INT8_MAX    127
#endif
#ifndef INT16_MAX
#define	INT16_MAX   32767
#endif
#ifndef INT32_MAX
#define	INT32_MAX   2147483647
#endif
#ifndef UINT8_MAX
#define	UINT8_MAX   255U
#endif
#ifndef UINT16_MAX
#define	UINT16_MAX  65535U
#endif
#ifndef UINT32_MAX
#define	UINT32_MAX  4294967295U
#endif

/*
 * Array length macro.
 */
#ifndef ARRAYLEN
#define ARRAYLEN(A) (sizeof(A) / sizeof(A[0]))
#endif

/*
 * Convenience types.
 */
typedef unsigned char uchar_t;
typedef unsigned int  uint_t;

/*
 * Set/clear bits - force 8-bit operands to prevent 8 -> 16 bit promotion.
 * BIT versions take a bit number, MASK versions take a bitmask.
 */

#define BIT(B)              (1 << (uint8_t)(B))       // Bit number.
#define GET_BIT(V, B)       ((V) & (uint8_t)BIT(B))   // Get bit number.
#define GET_BIT_HI(V, B)    ((V) | (uint8_t)BIT(B))   // Get with bit number hi.
#define GET_BIT_LO(V, B)    ((V) & (uint8_t)~BIT(B))  // Get with bit number lo.
#define SET_BIT_HI(V, B)    (V) |= (uint8_t)BIT(B)    // Set bit number hi.
#define SET_BIT_LO(V, B)    (V) &= (uint8_t)~BIT(B)   // Set bit number lo.
#define MASK(V, M)          ((V) & (uint8_t)(M))      // Mask bits.
#define GET_MASK(V, M)      ((V) & (uint8_t)(M))      // Get mask bits.
#define GET_MASK_HI(V, M)   ((V) | (uint8_t)(M))      // Get with mask bits hi.
#define GET_MASK_LO(V, M)   ((V) & (uint8_t)~(M))     // Get with mask bits lo.
#define SET_MASK_HI(V, M)   (V) |= (uint8_t)(M)       // Set mask bits hi.
#define SET_MASK_LO(V, M)   (V) &= (uint8_t)~(M)      // Set mask bits lo.

#define SET_MASKED_BITS(V, M, S) (V) = (V & (uint8_t) ~(M)) | (S & (uint8_t) (M))

#endif  /* utils_h */

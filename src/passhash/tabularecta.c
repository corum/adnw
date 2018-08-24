/*
    This file is part of the AdNW keyboard firmware.

    Copyright 2017-2018 Stefan Fröbe, <frobiac /at/ gmail [d0t] com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "sha1.h"
#include "tabularecta.h"
#include "../global_config.h"
#include "../_private_data.h"

#include "b64.h"
/**
 * HMAC-SHA1:
 * + Optimized asm available
 * + Generate arbitrary hashes from variable length input
 * + Compatible with CLI tools
 * - Needs arrays for key, tag and sha hash
 * - Needs base64 and its array
 * -/+ Output fixed at 20 -> 26 in base64
 *
 * TABULA_RECTA:
 * Given length, row and col the 13x26 matrix is queried for code[len] @ (row, col)
 * @TODO: direction
 * @TODO: only 20 selector cols: ij / pq / vwxyz same.
 *
 * When using hmac-sha1:
 * Unlock string is read and hashed via sha1. hash is stored as g_pw[20].
 * - The first two elements of currently entered unlock hash are stored in EEPROM for
 *   verification purposes on config save.
 * - Hash is used to Un-XOR stored EEPROM data like macros and the hmac-sha1 tag base.
 * - It is also used as key for tabula recta hmac-sha1 calls.
 *
 */

/// global unlock password used "descramble" stored EEPROM content and init seed
#define PWLEN SHA1_HASH_BYTES  // Must be >=20 for sha1

uint8_t g_pw[PWLEN]; // @TODO should initialize with random?

uint16_t pwfingerprint(void)
{
    // use 0xFE to prevent 0xFFFF in comparison with empty EEPROM
    return ((g_pw[0]<<8)|(g_pw[1]&0xFE));
}

/**
 * Indicates correct unlock key was entered.
 * Slight chance of mismatch is negligable.
 */
bool unlocked(void)
{
    return(g_cfg.unlock_check == pwfingerprint());
}

int8_t encrypt(uint8_t * data, uint8_t len)
{
    // xor same as decrypt
    decrypt(data,len);
    return 0;
}

int8_t decrypt(uint8_t * data, uint8_t len)
{
    for(uint8_t i=0; i<len; ++i)
        data[i] ^= g_pw[i%PWLEN];
    return len;
}


#if TR_ALGO == HMAC
/**
 *  Create hmac-sha1 from g_pw and string initialized with secret tag base, overwriting the beginning with custom tag.
 *  Simpler to implement, and regular hmac-sha1 call can always be used...
 *
 *  result_len should be <= 20, loops around otherwise
 */
void hmac_tag(uint8_t * result, uint8_t result_len, char * tag, uint8_t tag_len, uint8_t offs)
{
    if(tag_len>SHA1_HASH_BYTES || result_len> SHA1_HASH_BYTES)
        return;

    // xprintf("ht: %d %c[%d] @%d\n", result_len, tag[0], tag_len, offs);
    uint8_t sha[SHA1_HASH_BYTES];
    uint8_t hmac[SHA1_HASH_BYTES];

    memcpy(hmac, HMAC_TAG_BASE, HMAC_TAG_BASE_LEN);
    memcpy(hmac, tag, tag_len); // overwrite initial portion with given tag

    hmac_sha1(sha, g_pw, 8*PWLEN, hmac, 8*HMAC_TAG_BASE_LEN);

    // b64enc( sha, 20, (char*)hmac, 27); // little larger than array below

    // only creates 20 characters
    for(uint8_t i=0; i<result_len; ++i) {
        result[i] = b64map[sha[(i+offs)%SHA1_HASH_BYTES] & 0x3f];
    }

}
#endif

void tabula_recta(uint8_t * dst, char row, uint8_t col, uint8_t dst_len)
{

#if TR_ALGO == HMAC
    hmac_tag(dst,dst_len, &row, 1, col);
#elif TR_ALGO == XOR
    tr_code((char*)dst, dst_len, row-'a', col);
#endif
}

void unlock(uint8_t * code, uint8_t len)
{
#if TR_ALGO == XOR
    // set seed from input
    xor_init((char*)code, len);
    for(uint8_t i=0; i<PWLEN; ++i) {
        xorshift();
        g_pw[i] = xor_result() & 0xFF;
    }

#elif TR_ALGO == HMAC
    memset(g_pw, 0, PWLEN);
    // store hash of entered string as unlock password
    sha1(g_pw, code, 8*len);
    // must save config to store current password as correct on change
#endif
}


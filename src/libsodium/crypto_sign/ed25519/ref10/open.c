
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "crypto_hash_sha512.h"
#include "crypto_sign_ed25519.h"
#include "crypto_verify_32.h"
#include "utils.h"
#include "private/curve25519_ref10.h"

#ifndef ED25519_COMPAT
static int
crypto_sign_check_S_lt_L(const unsigned char *S)
{
    /* 2^252+27742317777372353535851937790883648493 */
    static const unsigned char L[32] =
      { 0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
        0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 };
    unsigned char c = 0;
    unsigned char n = 1;
    unsigned int  i = 32;

    do {
        i--;
        c |= ((S[i] - L[i]) >> 8) & n;
        n &= ((S[i] ^ L[i]) - 1) >> 8;
    } while (i != 0);

    return -(c == 0);
}

static int
small_order(const unsigned char R[32])
{
    CRYPTO_ALIGN(16) static const unsigned char blacklist[][32] = {
        /* 0 (order 4) */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        /* 1 (order 1) */
        { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        /* 2707385501144840649318225287225658788936804267575313519463743609750303402022 (order 8) */
        { 0x26, 0xe8, 0x95, 0x8f, 0xc2, 0xb2, 0x27, 0xb0, 0x45, 0xc3, 0xf4, 0x89, 0xf2, 0xef, 0x98, 0xf0, 0xd5, 0xdf, 0xac, 0x05, 0xd3, 0xc6, 0x33, 0x39, 0xb1, 0x38, 0x02, 0x88, 0x6d, 0x53, 0xfc, 0x05 },
        /* 55188659117513257062467267217118295137698188065244968500265048394206261417927 (order 8) */
        { 0xc7, 0x17, 0x6a, 0x70, 0x3d, 0x4d, 0xd8, 0x4f, 0xba, 0x3c, 0x0b, 0x76, 0x0d, 0x10, 0x67, 0x0f, 0x2a, 0x20, 0x53, 0xfa, 0x2c, 0x39, 0xcc, 0xc6, 0x4e, 0xc7, 0xfd, 0x77, 0x92, 0xac, 0x03, 0x7a },
        /* p-1 (order 2) */
        { 0x13, 0xe8, 0x95, 0x8f, 0xc2, 0xb2, 0x27, 0xb0, 0x45, 0xc3, 0xf4, 0x89, 0xf2, 0xef, 0x98, 0xf0, 0xd5, 0xdf, 0xac, 0x05, 0xd3, 0xc6, 0x33, 0x39, 0xb1, 0x38, 0x02, 0x88, 0x6d, 0x53, 0xfc, 0x85 },
        /* p (order 4) */
        { 0xb4, 0x17, 0x6a, 0x70, 0x3d, 0x4d, 0xd8, 0x4f, 0xba, 0x3c, 0x0b, 0x76, 0x0d, 0x10, 0x67, 0x0f, 0x2a, 0x20, 0x53, 0xfa, 0x2c, 0x39, 0xcc, 0xc6, 0x4e, 0xc7, 0xfd, 0x77, 0x92, 0xac, 0x03, 0xfa },
        /* p+1 (order 1) */
        { 0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f },
        /* p+2707385501144840649318225287225658788936804267575313519463743609750303402022 (order 8) */
        { 0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f },
        /* p+55188659117513257062467267217118295137698188065244968500265048394206261417927 (order 8) */
        { 0xee, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f },
        /* 2p-1 (order 2) */
        { 0xd9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
        /* 2p (order 4) */
        { 0xda, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
        /* 2p+1 (order 1) */
        { 0xdb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
    };
    size_t        i, j;
    unsigned char c;

    for (i = 0; i < sizeof blacklist / sizeof blacklist[0]; i++) {
        c = 0;
        for (j = 0; j < 32; j++) {
            c |= R[j] ^ blacklist[i][j];
        }
        if (c == 0) {
            return 1;
        }
    }
    return 0;
}
#endif

int
crypto_sign_ed25519_verify_detached(const unsigned char *sig,
                                    const unsigned char *m,
                                    unsigned long long mlen,
                                    const unsigned char *pk)
{
    crypto_hash_sha512_state hs;
    unsigned char h[64];
    unsigned char rcheck[32];
    unsigned int  i;
    unsigned char d = 0;
    ge_p3 A;
    ge_p2 R;

#ifndef ED25519_COMPAT
    if (crypto_sign_check_S_lt_L(sig + 32) != 0 ||
        small_order(sig) != 0) {
        return -1;
    }
#else
    if (sig[63] & 224) {
        return -1;
    }
#endif
    if (ge_frombytes_negate_vartime(&A, pk) != 0) {
        return -1;
    }
    for (i = 0; i < 32; ++i) {
        d |= pk[i];
    }
    if (d == 0) {
        return -1;
    }
    crypto_hash_sha512_init(&hs);
    crypto_hash_sha512_update(&hs, sig, 32);
    crypto_hash_sha512_update(&hs, pk, 32);
    crypto_hash_sha512_update(&hs, m, mlen);
    crypto_hash_sha512_final(&hs, h);
    sc_reduce(h);

    ge_double_scalarmult_vartime(&R, h, &A, sig + 32);
    ge_tobytes(rcheck, &R);

    return crypto_verify_32(rcheck, sig) | (-(rcheck == sig)) |
           sodium_memcmp(sig, rcheck, 32);
}

int
crypto_sign_ed25519_open(unsigned char *m, unsigned long long *mlen_p,
                         const unsigned char *sm, unsigned long long smlen,
                         const unsigned char *pk)
{
    unsigned long long mlen;

    if (smlen < 64 || smlen > SIZE_MAX) {
        goto badsig;
    }
    mlen = smlen - 64;
    if (crypto_sign_ed25519_verify_detached(sm, sm + 64, mlen, pk) != 0) {
        memset(m, 0, mlen);
        goto badsig;
    }
    if (mlen_p != NULL) {
        *mlen_p = mlen;
    }
    memmove(m, sm + 64, mlen);

    return 0;

badsig:
    if (mlen_p != NULL) {
        *mlen_p = 0;
    }
    return -1;
}

int
crypto_sign_ed25519_verify_cosi(const unsigned char *sig,
				unsigned long siglen,
                                const unsigned char *m,
                                unsigned long long mlen,
                                const unsigned char *pklist,
				unsigned long pkcount,
				crypto_sign_cosi_policy policy,
				void *policy_data)
{
    crypto_hash_sha512_state hs;
    const unsigned char *mask = sig + 64;
    unsigned char h[64];
    unsigned char rcheck[32];
    unsigned char pkagg[32];
    unsigned int  i, j;
    ge_p3 A, indA;
    ge_p2 R;

    if (siglen != 64 + (pkcount+7)/8) {
	return -1;
    }
    if (sig[63] & 224) {
        return -1;
    }

    if (policy == NULL) {
	policy = crypto_sign_ed25519_threshold_policy;
	policy_data = crypto_sign_ed25519_threshold_data(pkcount);
    }
    if (policy(mask, pkcount, policy_data) != 0) {
	return -1;
    }

    ge_p3_0(&A);
    for (i = 0; i < pkcount; i++) {
        const unsigned char *ipk = pklist + 32*i;
        unsigned char d = 0;

        if (ge_frombytes_negate_vartime(&indA, ipk) != 0) {
            return -1;
        }
        for (j = 0; j < 32; ++j) {
            d |= ipk[j];
        }
        if (d == 0) {
            return -1;
        }
        if ((mask[i/8] & (1 << (i&7))) == 0) {
            ge_p3_add(&A, &A, &indA);
        }
    }
    ge_p3_tobytes(pkagg, &A);
    pkagg[31] ^= 0x80;

    crypto_hash_sha512_init(&hs);
    crypto_hash_sha512_update(&hs, sig, 32);
    crypto_hash_sha512_update(&hs, pkagg, 32);
    crypto_hash_sha512_update(&hs, m, mlen);
    crypto_hash_sha512_final(&hs, h);
    sc_reduce(h);

    ge_double_scalarmult_vartime(&R, h, &A, sig + 32);
    ge_tobytes(rcheck, &R);

    return crypto_verify_32(rcheck, sig) | (-(rcheck == sig)) |
           sodium_memcmp(sig, rcheck, 32);
}

int crypto_sign_ed25519_threshold_policy(const unsigned char *mask,
					 unsigned long pkcount,
					 void *data) {
    unsigned int i, threshold = (uintptr_t)data, enabled = 0;

    for (i = 0; i < pkcount; i++) {
	if ((mask[i/8] & (1 << (i&7))) == 0) {
	    enabled++;
	}
    }
    if (enabled < threshold) {
	return -1;
    }
    return 0;
}


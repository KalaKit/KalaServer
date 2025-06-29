/*
 * Generated by util/mkerr.pl DO NOT EDIT
 * Copyright 1995-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OPENSSL_DSAERR_H
# define OPENSSL_DSAERR_H
# pragma once

# include <opensslconf.h>
# include <symhacks.h>
# include <cryptoerr_legacy.h>


# ifndef OPENSSL_NO_DSA


/*
 * DSA reason codes.
 */
#  define DSA_R_BAD_FFC_PARAMETERS                         114
#  define DSA_R_BAD_Q_VALUE                                102
#  define DSA_R_BN_DECODE_ERROR                            108
#  define DSA_R_BN_ERROR                                   109
#  define DSA_R_DECODE_ERROR                               104
#  define DSA_R_INVALID_DIGEST_TYPE                        106
#  define DSA_R_INVALID_PARAMETERS                         112
#  define DSA_R_MISSING_PARAMETERS                         101
#  define DSA_R_MISSING_PRIVATE_KEY                        111
#  define DSA_R_MODULUS_TOO_LARGE                          103
#  define DSA_R_NO_PARAMETERS_SET                          107
#  define DSA_R_PARAMETER_ENCODING_ERROR                   105
#  define DSA_R_P_NOT_PRIME                                115
#  define DSA_R_Q_NOT_PRIME                                113
#  define DSA_R_SEED_LEN_SMALL                             110
#  define DSA_R_TOO_MANY_RETRIES                           116

# endif
#endif

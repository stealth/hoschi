/*
 * This file is part of the hoschi p2p scan engine.
 *
 * (C) 2019 by Sebastian Krahmer,
 *             sebastian [dot] krahmer [at] gmail [dot] com
 *
 * hoschi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * hoschi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hoschi. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef hoschi_missing_h
#define hoschi_missing_h



extern "C" {
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
}


#if OPENSSL_VERSION_NUMBER > 0x10100000L && !(defined HAVE_LIBRESSL)

/* Idiots... Not just they are renaming EVP_MD_CTX_destroy() to EVP_MD_CTX_free() in OpenSSL >= 1.1,
 * they define EVP_MD_CTX_destroy(ctx) macro along (with braces) so we cant define the symbol
 * ourself. Forces me to introduce an entirely new name to stay compatible with older
 * versions and libressl.
 */
#define EVP_MD_CTX_delete EVP_MD_CTX_free
#else
#define EVP_MD_CTX_delete EVP_MD_CTX_destroy
#endif

#endif


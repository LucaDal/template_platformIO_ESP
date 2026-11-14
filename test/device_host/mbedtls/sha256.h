#pragma once
#include <openssl/sha.h>
using mbedtls_sha256_context = SHA256_CTX;
inline void mbedtls_sha256_init(mbedtls_sha256_context *) {}
inline void mbedtls_sha256_free(mbedtls_sha256_context *) {}
inline int mbedtls_sha256_starts_ret(mbedtls_sha256_context *ctx, int) { return SHA256_Init(ctx) == 1 ? 0 : -1; }
inline int mbedtls_sha256_update_ret(mbedtls_sha256_context *ctx, const unsigned char *data, size_t length) { return SHA256_Update(ctx, data, length) == 1 ? 0 : -1; }
inline int mbedtls_sha256_finish_ret(mbedtls_sha256_context *ctx, unsigned char *out) { return SHA256_Final(out, ctx) == 1 ? 0 : -1; }

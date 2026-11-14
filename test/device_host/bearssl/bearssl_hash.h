#pragma once
#include <openssl/sha.h>
using br_sha256_context = SHA256_CTX;
inline void br_sha256_init(br_sha256_context *ctx) { SHA256_Init(ctx); }
inline void br_sha256_update(br_sha256_context *ctx, const void *data, size_t length) { SHA256_Update(ctx, data, length); }
inline void br_sha256_out(const br_sha256_context *ctx, void *out) {
  auto copy = *ctx;
  SHA256_Final(static_cast<unsigned char *>(out), &copy);
}

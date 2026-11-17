#pragma once
#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#define SHA256_CTX CC_SHA256_CTX
#define SHA256_Init CC_SHA256_Init
#define SHA256_Update CC_SHA256_Update
#define SHA256_Final CC_SHA256_Final
#define SHA256 CC_SHA256
#else
#include <openssl/sha.h>
#endif
#include <cstddef>
#include <cstdint>
typedef SHA256_CTX mbedtls_sha256_context;
inline void mbedtls_sha256_init(mbedtls_sha256_context* c) { SHA256_Init(c); }
inline int mbedtls_sha256_starts(mbedtls_sha256_context* c,int) { return SHA256_Init(c)==1?0:-1; }
inline int mbedtls_sha256_update(mbedtls_sha256_context* c,const uint8_t* p,size_t n) { return SHA256_Update(c,p,n)==1?0:-1; }
inline int mbedtls_sha256_finish(mbedtls_sha256_context* c,uint8_t* out) { return SHA256_Final(out,c)==1?0:-1; }
inline void mbedtls_sha256_free(mbedtls_sha256_context*) {}
inline int mbedtls_sha256(const uint8_t* p,size_t n,uint8_t* out,int) { return SHA256(p,n,out)?0:-1; }

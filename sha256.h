/* Taken from <http://www.spale.com/download/scrypt/scrypt1.0/> */

#ifndef _SHA256_H
#define _SHA256_H

#ifndef uint8
#define uint8  unsigned char
#endif

#ifndef uint32
#define uint32 unsigned long int
#endif

typedef struct
{
    uint32 total[2];
    uint32 state[8];
    uint8 buffer[64];
}
sha256_context;
struct sha256 {
    static void sha256_starts( sha256_context *ctx );
    static void sha256_update( sha256_context *ctx, uint8 *input, uint32 length );
    static void sha256_finish( sha256_context *ctx, uint8 digest[32] );
};
#endif /* sha256.h */


#define SCREENMIRROR_LZ_TEST_DECODER 1
#include "../src/lz-regions-source/lzss.h"
unsigned sm_test_encode(const unsigned char *p,unsigned n,unsigned char *out,unsigned capacity,struct lz_work *stats) {
    return bounded_lzss_encode(p,n,out,capacity,4,stats);
}
unsigned sm_test_decode(const unsigned char *p,unsigned n,unsigned char *out,unsigned capacity) {
    return bounded_lzss_decode(p,n,out,capacity);
}

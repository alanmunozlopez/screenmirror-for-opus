#ifndef SCREENMIRROR_BOUNDED_LZSS_H
#define SCREENMIRROR_BOUNDED_LZSS_H
/* ScreenMirror LZSS4: custom lossless format, not the LZ4 codec.
 * See analysis/analyze-lz-corpus.py for an independent model.
 * Workspace is static, single caller on main; no allocation or libc. */
/* Experimental freestanding codec used by ScreenMirror 0.10.
 * Input<=65534; history<=4096; 1..16 candidates; match3..18.
 * 8192 byte comparisons per bucket of1024 input positions at search start.
 * Workspace16KiB (two4096-entry uint16 arrays), single caller/main-thread only.
 * Output cap makes raw fallback explicit; 0xffffffff means refused/full.
 */
struct lz_work {unsigned input_bytes,candidate_checks,byte_comparisons,max_bucket,skipped;};
static unsigned short lz_head[4096],lz_prev[4096];
static unsigned lz_hash(const unsigned char *src,unsigned i) {
    return (((unsigned)src[i]*251u+src[i+1])*251u+src[i+2])&4095u;
}
static unsigned bounded_lzss_encode(const unsigned char *src,unsigned n,unsigned char *dst,unsigned capacity,
                            unsigned lookups,struct lz_work *stats) {
    unsigned p=0,out=0,i,bucket=0xffffffffu,spent=0;
    struct lz_work work={0,0,0,0,0};
    if(n>65534u || lookups<1 || lookups>16)return 0xffffffffu;
    for(i=0;i<4096;i++)lz_head[i]=0;
    while(p<n) {
        unsigned flag_at=out,flags=0,bit;
        if(out==capacity)goto full;
        dst[out++]=0;
        for(bit=0;bit<8 && p<n;bit++) {
            unsigned best=0,distance=0,limit=n-p,q,search,step;
            if(limit>18)limit=18;
            if((p>>10)!=bucket) {bucket=p>>10;spent=0;}
            if(limit>=3 && spent<8192) {
                q=lz_head[lz_hash(src,p)];
                for(search=0;search<lookups && q && spent<8192;search++) {
                    unsigned position=q-1,k=0;
                    if(position>=p || p-position>4096)break;
                    work.candidate_checks++;
                    while(k<limit && spent<8192) {
                        spent++;work.byte_comparisons++;
                        if(src[position+k]!=src[p+k])break;
                        k++;
                    }
                    if(k>best) {best=k;distance=p-position;}
                    if(best==limit)break;
                    q=lz_prev[position&4095];
                }
            } else if(limit>=3)work.skipped++;
            if(spent>work.max_bucket)work.max_bucket=spent;
            if(best>=3) {
                if(capacity-out<2)goto full;
                dst[out++]=(unsigned char)(distance-1);
                dst[out++]=(unsigned char)((((distance-1)>>8)<<4)|(best-3));step=best;
            } else {
                if(out==capacity)goto full;
                flags|=1u<<bit;dst[out++]=src[p];step=1;
            }
            for(i=p;i<p+step;i++)if(i+2<n) {
                unsigned h=lz_hash(src,i);lz_prev[i&4095]=lz_head[h];lz_head[h]=(unsigned short)(i+1);
            }
            p+=step;
        }
        dst[flag_at]=(unsigned char)flags;
    }
    work.input_bytes=n;if(stats)*stats=work;return out;
full:
    work.input_bytes=p;if(stats)*stats=work;return 0xffffffffu;
}


#ifdef SCREENMIRROR_LZ_TEST_DECODER
static unsigned bounded_lzss_decode(const unsigned char *src,unsigned n,unsigned char *dst,unsigned capacity) {
    unsigned p=0,out=0;
    while(p<n) {
        unsigned flags=src[p++],bit;
        if(p==n)return 0xffffffffu; /* Empty trailing flag group is not canonical. */
        for(bit=0;bit<8;bit++) {
            if(p==n) {if(flags>>bit)return 0xffffffffu;break;}
            if(flags&(1u<<bit)) {
                if(out==capacity)return 0xffffffffu;
                dst[out++]=src[p++];
            } else {
                unsigned distance,length,i;
                if(n-p<2)return 0xffffffffu;
                distance=((((unsigned)src[p+1]>>4)<<8)|src[p])+1;
                length=(src[p+1]&15u)+3;p+=2;
                if(distance>out || distance>4096 || length>capacity-out)return 0xffffffffu;
                for(i=0;i<length;i++) {dst[out]=dst[out-distance];out++;}
            }
        }
    }
    return out==capacity ? out:0xffffffffu;
}

#endif
#endif

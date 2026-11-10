"""Independent Python LZSS model extracted from the original corpus analyzer.

Only pure codec functions are retained; no captures or device APIs are used.
"""

def lzss(data,lookups=16,budget=8192):
    """4096 window, flags/8 tokens, 12-bit distance-1, length nibble+3.

    budget limits byte comparisons for searches starting in each 1024 input
    bytes, including unsuccessful comparisons. Hash insertion is once/input byte.
    The prototype emits a full candidate; the proposed C encoder aborts to raw if
    output reaches the original length. That does not change any selected cost.
    """
    n=len(data);head=[-1]*4096;previous=[-1]*4096;out=bytearray();p=0
    probes=comparisons=0;bucket=-1;spent=0;maximum_spent=0;exhaustions=0
    def insert(i):
        if i+2<n:
            h=((data[i]*251+data[i+1])*251+data[i+2])&4095
            previous[i&4095]=head[h];head[h]=i
    while p<n:
        flag_at=len(out);out.append(0);flags=0
        for bit in range(8):
            if p>=n:break
            if p//1024!=bucket:bucket=p//1024;spent=0
            best=distance=0;limit=min(18,n-p)
            if limit>=3 and spent<budget:
                h=((data[p]*251+data[p+1])*251+data[p+2])&4095;q=head[h]
                for _ in range(lookups):
                    if q<0 or q>=p or p-q>4096 or spent>=budget:break
                    probes+=1;k=0
                    while k<limit and spent<budget:
                        comparisons+=1;spent+=1
                        if data[q+k]!=data[p+k]:break
                        k+=1
                    if k>best:best=k;distance=p-q
                    if best==limit:break
                    q=previous[q&4095]
            elif limit>=3:exhaustions+=1
            maximum_spent=max(maximum_spent,spent)
            if best>=3:
                out.extend(((distance-1)&255,(((distance-1)>>8)<<4)|(best-3)));step=best
            else:flags|=1<<bit;out.append(data[p]);step=1
            for i in range(p,p+step):insert(i)
            p+=step
        out[flag_at]=flags
    assert maximum_spent<=budget
    return bytes(out),{'input_bytes':n,'candidate_checks':probes,'byte_comparisons':comparisons,
                       'max_comparisons_per_input_kb':maximum_spent,'budget_skipped_searches':exhaustions}


def unlzss(data,maximum):
    p=0;out=bytearray()
    while p<len(data):
        flags=data[p];p+=1
        if p==len(data):raise ValueError('Empty flag group')
        for bit in range(8):
            if p==len(data):
                if flags>>bit:raise ValueError('Nonzero unused flag bits')
                break
            if flags&(1<<bit):out.append(data[p]);p+=1
            else:
                if p+2>len(data):raise ValueError('Truncated match')
                a,b=data[p:p+2];p+=2;distance=((b>>4)<<8|a)+1;length=(b&15)+3
                if distance>len(out) or distance>4096:raise ValueError('Invalid match distance')
                for _ in range(length):out.append(out[-distance])
            if len(out)>maximum:raise ValueError('Expansion limit')
    return bytes(out)

from pathlib import Path
import pgpy
from cryptography.hazmat.primitives import hashes
root=Path('.work/bfin-native/downloads')
# Split the concatenated GNU keyring at top-level public-key packets. PGPy's
# whole-keyring parser cannot handle every historical key in this collection.
data=(root/'gnu-keyring.gpg').read_bytes(); pos=0; starts=[]; public_packets=[]
while pos<len(data):
    start=pos; head=data[pos];pos+=1
    assert head&128
    if head&64:
        tag=head&63; length=data[pos];pos+=1
        if length<192: n=length
        elif length<224: n=((length-192)<<8)+data[pos]+192;pos+=1
        elif length==255: n=int.from_bytes(data[pos:pos+4],'big');pos+=4
        else: raise ValueError('partial length in keyring')
    else:
        tag=(head>>2)&15; size=1<<(head&3)
        assert (head&3)!=3
        n=int.from_bytes(data[pos:pos+size],'big');pos+=size
    if tag==6:starts.append(start)
    if tag in (6,14):public_packets.append(data[start:pos+n])
    pos+=n
assert pos==len(data)
starts.append(len(data))
keys=[]
for packet in public_packets:
    try:
        key,_=pgpy.PGPKey.from_blob(packet);keys.append(key)
    except Exception:pass
for name in ('binutils-2.38.tar.xz','gcc-13.3.0.tar.xz'):
    sig=pgpy.PGPSignature.from_blob((root/(name+'.sig')).read_bytes())
    matches=[k for k in keys if k.fingerprint.keyid==sig.signer or sig.signer in k.subkeys]
    assert matches, sig.signer
    assert any(k._key.verify(sig.hashdata((root/name).read_bytes()),sig.__sig__,getattr(hashes,sig.hash_algorithm.name)()) for k in matches),name
    print(name,'signature verified; GNU keyring signer',sig.signer)

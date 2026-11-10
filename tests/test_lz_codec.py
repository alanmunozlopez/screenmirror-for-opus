"""Actual freestanding C codec vs independent Python model, bounded writes/work."""
import ctypes,importlib.util,pathlib,random,subprocess,sys,unittest
R=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('codec_model',R/'tests/models/lz_model.py')
model=importlib.util.module_from_spec(spec);sys.modules[spec.name]=model;spec.loader.exec_module(model)
FAIL=0xffffffff
class Work(ctypes.Structure):
 _fields_=[(name,ctypes.c_uint) for name in ('input_bytes','candidate_checks','byte_comparisons','max_bucket','skipped')]
class Codec(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  lib=R/'build/lz-codec-tests.dylib'
  subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-shared','-fPIC',str(R/'tests/lz_codec_bridge.c'),'-o',str(lib)],check=True)
  cls.lib=ctypes.CDLL(str(lib));p=ctypes.POINTER(ctypes.c_ubyte)
  cls.lib.sm_test_encode.argtypes=[p,ctypes.c_uint,p,ctypes.c_uint,ctypes.POINTER(Work)];cls.lib.sm_test_encode.restype=ctypes.c_uint
  cls.lib.sm_test_decode.argtypes=[p,ctypes.c_uint,p,ctypes.c_uint];cls.lib.sm_test_decode.restype=ctypes.c_uint
 def ccall(self,data,capacity,decode=False):
  src=(ctypes.c_ubyte*max(1,len(data))).from_buffer_copy(data or b'\0')
  guarded=(ctypes.c_ubyte*(capacity+16))(*([165]*(capacity+16)))
  dst=ctypes.cast(ctypes.byref(guarded,8),ctypes.POINTER(ctypes.c_ubyte));stats=Work()
  if decode:n=self.lib.sm_test_decode(src,len(data),dst,capacity)
  else:n=self.lib.sm_test_encode(src,len(data),dst,capacity,ctypes.byref(stats))
  self.assertEqual(bytes(guarded[:8]),bytes([165])*8);self.assertEqual(bytes(guarded[8+capacity:]),bytes([165])*8)
  out=bytes(guarded[8:8+n]) if n!=FAIL else b''
  return n,out,stats
 def test_exact_model_and_roundtrips(self):
  rng=random.Random(1003154)
  cases=[b'',b'a',b'ab',b'abc',b'a'*10000,bytes(range(256))*24,(b'abcab'*900)+b'd']
  for n in (2,3,4,7,8,9,18,19,32,33,130,131,1023,1024,1025,4095,4096,4097,10000,51200,65534):
   cases.extend((bytes(rng.randrange(256) for _ in range(n)),bytes(rng.randrange(4) for _ in range(n))))
  summary=[]
  for data in cases:
   expected,work=model.lzss(data,4)
   n,encoded,stats=self.ccall(data,len(expected))
   self.assertEqual(encoded,expected);self.assertEqual(n,len(expected))
   self.assertLessEqual(stats.max_bucket,8192)
   self.assertEqual(stats.input_bytes,len(data));self.assertEqual(stats.byte_comparisons,work['byte_comparisons'])
   self.assertEqual(stats.candidate_checks,work['candidate_checks']);self.assertEqual(stats.skipped,work['budget_skipped_searches'])
   decoded_n,decoded,_=self.ccall(encoded,len(data),True);self.assertEqual(decoded,data);self.assertEqual(decoded_n,len(data))
   if encoded:
    too_small,_,_=self.ccall(data,len(encoded)-1);self.assertEqual(too_small,FAIL)
   summary.append({'input':len(data),'output':len(encoded),'comparisons':stats.byte_comparisons,'max_bucket':stats.max_bucket})
  import json
  (R/'build/lz-codec-tests.json').write_text(json.dumps({'cases':len(cases),'max_bucket':max(x['max_bucket'] for x in summary),'results':summary},indent=2)+'\n')
 def test_malformed_and_exact_lengths(self):
  for encoded,maximum in ((b'\0',0),(b'\0\0\0',32),(b'\xffa',1),(b'\x01a\0',1),(b'\x01a',0),(b'\x01a',2),(b'\x01a\0\x00',32)):
   n,_,_=self.ccall(encoded,maximum,True);self.assertEqual(n,FAIL,(encoded,maximum))
 def test_input_limit(self):
  n,_,_=self.ccall(bytes(65535),1);self.assertEqual(n,FAIL)
if __name__=='__main__':unittest.main()

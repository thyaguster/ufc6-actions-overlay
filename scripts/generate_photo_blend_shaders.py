from pathlib import Path
import struct
# Small SPIR-V 1.0 shaders. BufferBlock works without optional device features.
def inst(op,*args):return [(len(args)+1)<<16|op,*args]
def string(s):
 b=s.encode()+b'\0';b+=b'\0'*((-len(b))%4);return list(struct.unpack('<'+'I'*(len(b)//4),b))
def emit(name,bound,body):
 words=[0x07230203,0x00010000,0,bound,0]+body
 return 'static const uint32_t '+name+'[]={\n'+','.join(hex(x)+'u' for x in words)+'\n};\n'
v=[]
v+=inst(17,1)+inst(14,0,1)+inst(15,0,20,*string('main'),10,11)
v+=inst(71,10,30,0)+inst(71,11,11,0)
v+=inst(19,1)+inst(33,2,1)+inst(22,3,32)+inst(23,4,3,2)+inst(23,5,3,4)+inst(32,6,1,4)+inst(32,7,3,5)
v+=inst(43,3,8,0)+inst(43,3,9,0x3f800000)+inst(59,6,10,1)+inst(59,7,11,3)
v+=inst(54,1,20,0,2)+inst(248,21)+inst(61,4,22,10)+inst(81,3,23,22,0)+inst(81,3,24,22,1)+inst(80,5,25,23,24,8,9)+inst(62,11,25)+inst(253)+inst(56)
f=[]
f+=inst(17,1)+inst(14,0,1)+inst(15,4,30,*string('main'),15,16)+inst(16,30,7)
f+=inst(71,15,11,15)+inst(71,16,30,0)+inst(71,8,6,16)+inst(71,9,3)+inst(72,9,0,35,0)+inst(72,9,0,24)+inst(71,12,34,0)+inst(71,12,33,0)
f+=inst(71,17,2)+inst(72,17,0,35,0)+inst(72,17,1,35,4)+inst(72,17,2,35,8)
f+=inst(19,1)+inst(33,2,1)+inst(22,3,32)+inst(23,4,3,4)+inst(21,5,32,0)+inst(43,5,6,0)+inst(43,5,7,1)+inst(29,8,4)+inst(30,9,8)+inst(32,10,2,9)+inst(32,11,2,4)+inst(59,10,12,2)+inst(32,13,1,4)+inst(32,14,3,4)+inst(59,13,15,1)+inst(59,14,16,3)
f+=inst(30,17,5,5,5)+inst(32,18,9,17)+inst(32,19,9,5)+inst(59,18,20,9)+inst(43,5,21,2)
f+=inst(54,1,30,0,2)+inst(248,31)+inst(61,4,32,15)+inst(81,3,33,32,0)+inst(81,3,34,32,1)+inst(109,5,35,33)+inst(109,5,36,34)
f+=inst(65,19,37,20,6)+inst(65,19,38,20,7)+inst(65,19,39,20,21)+inst(61,5,40,37)+inst(61,5,41,38)+inst(61,5,42,39)+inst(130,5,43,35,40)+inst(130,5,44,36,41)+inst(132,5,45,44,42)+inst(128,5,46,45,43)+inst(65,11,47,12,6,46)+inst(61,4,48,47)+inst(62,16,48)+inst(253)+inst(56)
Path('src/photo_blend_shaders.h').write_text('/* SPIR-V 1.0: vertex XY; fragment reads straight-alpha RGBA from a readonly buffer. */\n'+emit('photo_blend_vert',26,v)+emit('photo_blend_frag',49,f))

"""Regenerate committed SPIR-V using Khronos glslang (Vulkan 1.0 baseline)."""
from pathlib import Path
import argparse,subprocess,struct,tempfile
p=argparse.ArgumentParser();p.add_argument('--compiler',default='glslangValidator');a=p.parse_args()
root=Path(__file__).resolve().parent.parent
header='/* Generated from src/shaders/photo_effects.*; Vulkan 1.0, no optional features. */\n'
with tempfile.TemporaryDirectory() as tmp:
 for stage in ('vert','frag'):
  out=Path(tmp)/(stage+'.spv');subprocess.run([a.compiler,'-V','--target-env','vulkan1.0',str(root/'src/shaders'/('photo_effects.'+stage)),'-o',str(out)],check=True)
  b=out.read_bytes();words=struct.unpack('<'+'I'*(len(b)//4),b)
  header+='static const uint32_t photo_effects_'+stage+'[]={\n'+','.join(hex(x)+'u' for x in words)+'\n};\n'
(root/'src/photo_effects_shaders.h').write_text(header,encoding='utf-8')

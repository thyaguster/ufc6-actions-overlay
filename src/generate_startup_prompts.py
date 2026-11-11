from pathlib import Path
from PIL import Image
import argparse
parser=argparse.ArgumentParser(description="Generate embedded text masks from English, 16:9 game-window captures; images are not bundled.")
parser.add_argument('--network',type=Path,required=True)
parser.add_argument('--roster',type=Path,required=True)
parser.add_argument('--title',type=Path,required=True)
parser.add_argument('--output',type=Path,default=Path(__file__).with_name('startup_prompt_masks.h'))
args=parser.parse_args()
paths=[args.network,args.roster,args.title]
W,H=640,90
out=['/* Binary text signatures for the English startup warning and title text. No game backgrounds or button artwork. */','#define PROMPT_GRID_W 640','#define PROMPT_GRID_H 90','static const unsigned char startup_prompt_masks[3][7200]={']
for n,p in enumerate(paths):
 im=Image.open(p).convert('RGB');px=im.load();w,h=im.size;bits=[]
 for y in range(H):
  for x in range(W):
   sx=((x+160)*w+w//2)//960;sy=((y+325)*h+h//2)//540
   v=max(min(px[min(w-1,sx+dx),min(h-1,sy+dy)]) for dx in (0,1) for dy in (0,1));bits.append(v>=160)
 packed=bytes(sum(int(bits[i+j])<<j for j in range(8)) for i in range(0,len(bits),8));print(n,'bright',sum(bits));out.append('{'+','.join(str(x) for x in packed)+'},')
 mask=Image.new('L',(W,H));mask.putdata([255*int(b) for b in bits]);# Raw captures and preview images are not emitted.
out.append('};\n');args.output.write_text('\n'.join(out))

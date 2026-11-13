"""Check actual photo-pass captures from the PHOTO_EFFECTS_GPU_TEST Vulkan fixture."""
import argparse, math
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
p=argparse.ArgumentParser();p.add_argument('folder',type=Path);a=p.parse_args()
images={n:Image.open(a.folder/f'effect-{n}.png').convert('RGB') for n in range(16,71)}
w,h=images[16].size
base=images[16].getpixel((w//2,h//2));c=[v/255 for v in base];l=sum(v*k for v,k in zip(c,(.2126,.7152,.0722)))
def expect(n,rgb):
 actual=images[n].getpixel((w//2,h//2));expected=[round(max(0,min(1,v))*255) for v in rgb]
 assert all(abs(x-y)<=2 for x,y in zip(actual,expected)),(n,actual,expected)
expect(17,[(l+(v-l)*1.35-.5)*1.12+.5 for v in c])
f=[(v-.5)*1.12+.5+sh*(1-l)+hi*l for v,sh,hi in zip(c,(-.025,.015,.045),(.045,.012,-.025))];expect(18,[l+(v-l)*.85 for v in f])
expect(19,[v*k for v,k in zip(c,(1.10,1.02,.90))]);expect(20,[v*k for v,k in zip(c,(.90,1.02,1.10))]);expect(21,[l]*3)
expect(22,[sum(x*k for x,k in zip(c,row)) for row in ((.393,.769,.189),(.349,.686,.168),(.272,.534,.131))]);expect(23,[v*2 for v in c])
assert images[24].getpixel((w//2,2))==(0,0,0);expect(24,c)
for n in range(25,29):
 bbox=ImageChops.difference(images[n],images[16]).getbbox();assert bbox,(n,'missing watermark');x,y,right,bottom=bbox
 assert (x>w//2)==(n in (26,28)),(n,bbox)
 assert (y>h//2)==(n in (27,28)),(n,bbox)
 assert max(ImageStat.Stat(images[n].crop(bbox)).extrema[0])>180
for n in (29,35,37):assert ImageChops.difference(images[n],images[16]).getbbox() is None,(n,'neutral/exit modified image')
assert sum(images[30].getpixel((8,8)))<sum(base);expect(30,c)
assert ImageStat.Stat(images[31]).stddev[0]>2
expect(32,[l]*3);expect(33,[(v-.5)*1.6+.5 for v in c]);expect(34,[v+k*.12 for v,k in zip(c,(1,.15,-1))])
bbox=ImageChops.difference(images[36],images[16]).getbbox();assert bbox and bbox[0]>w//2 and bbox[1]>h//2
assert images[36].getextrema()[0][1]<150
assert ImageChops.difference(images[38],images[21]).getbbox(),'Filtered overlay screenshot is missing the controls panel'
assert images[38].getpixel((w-2,h-2))==images[21].getpixel((w-2,h-2)),'Panel changed filtered background'
assert max(ImageStat.Stat(images[38]).extrema[0])>180,'Overlay artwork was graded with the scene'
for n in (39,40,41):
 assert ImageChops.difference(images[n],images[21]).getbbox(),'Active-filter page entry lost its menu'
 assert images[n].getpixel((w-2,h-2))==images[21].getpixel((w-2,h-2)),'Filters page changed background grading'
for n in range(42,58):
 bbox=ImageChops.difference(images[n],images[16]).getbbox();assert bbox and bbox[0]>w//2 and bbox[1]>h//2,(n,'styled watermark missing/misplaced')
 assert max(images[n].getextrema()[0])>180
assert len({images[n].tobytes() for n in (42,46,50,54)})==4,'Watermark styles did not update live'
assert len({images[n].tobytes() for n in (42,43,44,45)})>=3,'Font selection did not update the cached raster'
assert images[58].tobytes()!=images[59].tobytes(),'Editor pages did not change'
assert images[60].tobytes()!=images[59].tobytes(),'Hidden panel retained its editor raster'
assert images[61].tobytes()==images[16].tobytes(),'Photo Mode exit retained effects/watermark'
palette=(0xf4f6fa,0x141820,0xb8c4d4,0xf5c56b,0xff7d85,0x87c8ff,0x8ae5c1,0xcfb0ff)
for n in range(62,71):
 rgb=palette[1 if n==70 else n-62];target=((rgb>>16)&255,(rgb>>8)&255,rgb&255)
 region=images[n].crop((w//2,h//2,w,h));raw=region.tobytes();closest=min(abs(raw[i]-target[0])+abs(raw[i+1]-target[1])+abs(raw[i+2]-target[2]) for i in range(0,len(raw),3))
 assert closest<=6,(n,target,closest,'watermark color not rendered')
print('PASS: active-filter tall-page entry/resize, real GPU presets, exposure/contrast/saturation/warmth, vignette/grain/bars, watermark corners/opacity/size, neutral bypass and exit restoration; clean startup probes validated by the native fixture.')

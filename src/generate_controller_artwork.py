"""Compile bundled CC0 SVG prompt paths into resolution-independent GDI data."""
from pathlib import Path
import re,xml.etree.ElementTree as ET,json,argparse
ROOT=Path(__file__).resolve().parent
ASSETS=ROOT/'assets/controller-prompts'
def compile_path(data):
    tokens=re.findall(r'[A-Za-z]|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?',data)
    index=0;command=None;point=(0.,0.);first=None;ops=[];bounds=[]
    def emit(op,*points):
        ops.append((op,[round(c*1000) for p in points for c in p]));bounds.extend(points)
    def pair():
        nonlocal index
        p=(float(tokens[index]),float(tokens[index+1]));index+=2;return p
    while index<len(tokens):
        if tokens[index].isalpha():command=tokens[index];index+=1
        assert command in ('M','L','Q','Z'),command
        if command=='Z':emit('Z');point=first;first=None;command=None;continue
        if command=='M':
            if first is not None:emit('Z')
            point=pair();first=point;emit('M',point);command='L'
        elif command=='L':point=pair();emit('L',point)
        else:
            control,end=pair(),pair();a=tuple(point[i]+2*(control[i]-point[i])/3 for i in range(2));b=tuple(end[i]+2*(control[i]-end[i])/3 for i in range(2));emit('C',a,b,end);point=end
    if first is not None:emit('Z')
    return ops,bounds
def generate():
    manifest=json.loads((ASSETS/'manifest.json').read_text());out=['/* Generated from Kenney Input Prompts 1.5A; CC0. See assets/controller-prompts/License.txt. */']
    names=[]
    for platform,files in enumerate(manifest['platforms']):
        platform_names=[]
        for number,file in enumerate(files):
            root=ET.fromstring((ASSETS/file).read_bytes());assert not any('transform' in n.attrib for n in root.iter())
            paths=list(root.iter('{http://www.w3.org/2000/svg}path'));bounds=[];identifier=f'controller_asset_{platform}_{number}';path_defs=[]
            for part,path in enumerate(paths):
                assert path.attrib.get('stroke') in ('none',None)
                ops,bb=compile_path(path.attrib['d']);bounds.extend(bb);name=f'{identifier}_part_{part}'
                out.append(f'static const ControllerArtCommand {name}[]={{')
                for op,coords in ops:out.append('{'+str({'M':0,'L':1,'C':2,'Z':3}[op])+', {'+','.join(map(str,coords+[0]*(6-len(coords))))+'}},')
                out.append('};');fill=int(path.attrib['fill'][1:],16);path_defs.append((name,len(ops),fill))
            out.append(f'static const ControllerArtPath {identifier}_paths[]={{')
            for name,count,fill in path_defs:out.append(f'{{{name},{count},0x{fill:06x}u}},')
            out.append('};');xmin=min(p[0] for p in bounds);ymin=min(p[1] for p in bounds);xmax=max(p[0] for p in bounds);ymax=max(p[1] for p in bounds)
            out.append(f'static const ControllerArtAsset {identifier}={{{identifier}_paths,{len(path_defs)},'+','.join(str(round(c*1000)) for c in [xmin,ymin,xmax,ymax])+'};');platform_names.append('&'+identifier)
        names.append(platform_names)
    out.append('static const ControllerArtAsset *controller_art_assets[2][14]={');out.extend('{'+','.join(n)+'},' for n in names);out.append('};\n');return '\n'.join(out)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args();path=ROOT/'controller_artwork_data.h';text=generate()
    if args.check:assert path.read_text()==text,'Regenerate controller_artwork_data.h'
    else:path.write_text(text)

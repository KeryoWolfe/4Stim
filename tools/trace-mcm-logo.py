#!/usr/bin/env python3
# Traces the MCM page's logo picture into the vector layers of
# Interface-src/logo/FourStimLogo.as (MCM's image control takes vector art only).
#   python3 tools/trace-mcm-logo.py <picture.png> <work folder>
# Needs: pip install numpy pillow scipy vtracer svgpathtools cairosvg
# Writes into the work folder: paths.as.txt (the SILVER / FOUR / GEAR arrays to
# paste into FourStimLogo.as), meta.json (each layer's gradient box and stops,
# for the layer(...) calls) and preview.png (the layers without Flash's bevels).
# The layers: the coloured parts (saturation >= 8: the plate is pure grey) as
# the "4" and the gear (the two largest), the rest as the silver.
import os, shutil, sys
src, work = os.path.abspath(sys.argv[1]), sys.argv[2]
os.makedirs(work, exist_ok=True)
shutil.copy(src, os.path.join(work, 'src.png'))
os.chdir(work)
# ---- masks ----
import numpy as np
from PIL import Image
from scipy import ndimage as nd
im=np.asarray(Image.open('src.png').convert('RGBA')).astype(int)
H,W=im.shape[:2]
r,g,b,a=im[...,0],im[...,1],im[...,2],im[...,3]
A=a>=128
sat=np.maximum(np.maximum(r,g),b)-np.minimum(np.minimum(r,g),b)
col=A&(sat>=8)
col=nd.binary_opening(col,iterations=1)
sil=A&~col
lab,n=nd.label(col)
sizes=nd.sum(col,lab,range(1,n+1))
print(H,W,n,sorted(sizes)[-6:])
# components: split colored by x
xs=np.arange(W)[None,:].repeat(H,0)
order=np.argsort(sizes)[-2:]
four=lab==(order[0]+1); gear=lab==(order[1]+1)
# The bevels' pale edges aren't saturated: close them back in.
four=nd.binary_closing(four,iterations=2)|four
gear=nd.binary_closing(gear,iterations=2)|gear
sil=sil&~nd.binary_dilation(nd.binary_fill_holes(gear),iterations=6)
# keep only big comps
def big(m,minsz=500):
    l,k=nd.label(m); s=nd.sum(m,l,range(1,k+1)); keep=np.isin(l,[i+1 for i,v in enumerate(s) if v>=minsz]); return keep
four=big(four); gear=big(gear)
silf=big(sil|(nd.binary_dilation(sil,iterations=4)&four))
for nm,m in [('four',four),('gear',gear),('silver',silf)]:
    Image.fromarray(np.where(m,0,255).astype(np.uint8)).save(nm+'_mask.png')
np.save('img.npy',im)
for nm,m in [('four',four),('gear',gear),('silver',sil)]:
    ys,xs2=np.nonzero(m); print(nm,xs2.min(),ys.min(),xs2.max(),ys.max())
# ---- trace ----
import vtracer, re, math
from PIL import Image
from svgpathtools import parse_path, Line, CubicBezier, QuadraticBezier
from svgpathtools import parse_path, Line, CubicBezier, QuadraticBezier
im=np.load('img.npy'); H,W=im.shape[:2]
OUTW=640.0; SC=OUTW/W
layers={}
for nm in ['silver','four','gear']:
    vtracer.convert_image_to_svg_py(nm+'_mask.png',nm+'.svg',colormode='binary',filter_speckle=8,corner_threshold=60,length_threshold=4.0,splice_threshold=45,path_precision=2,mode='spline')
    svg=open(nm+'.svg').read()
    cmds=[]
    for tag in re.findall(r'<path[^>]*>',svg):
        d=re.search(r' d="([^"]+)"',tag).group(1)
        t=re.search(r'translate\(([-\d.]+),([-\d.]+)\)',tag)
        tx,ty=(float(t.group(1)),float(t.group(2))) if t else (0.0,0.0)
        p=parse_path(d)
        cur=None
        for seg in p:
            s0=seg.start+complex(tx,ty)
            if cur is None or abs(s0-cur)>1e-6:
                cmds.append((0,s0)); 
            if isinstance(seg,Line):
                cmds.append((1,seg.end+complex(tx,ty)))
            elif isinstance(seg,CubicBezier):
                for half in seg.split(0.5):
                    P0,P1,P2,P3=[z+complex(tx,ty) for z in (half.start,half.control1,half.control2,half.end)]
                    Q=(3*(P1+P2)-P0-P3)/4
                    cmds.append((2,Q,P3))
            elif isinstance(seg,QuadraticBezier):
                cmds.append((2,seg.control+complex(tx,ty),seg.end+complex(tx,ty)))
            cur=seg.end+complex(tx,ty)
    layers[nm]=cmds
    print(nm,len(cmds),svg.count('<path'))
# gradient stops along the diagonal (bottom-left -> top-right), from the image
def stops(mask_name,n=6,exclude_dark=False):
    m=np.asarray(Image.open(mask_name+'_mask.png'))<128
    r,g,b,a=[im[...,i] for i in range(4)]
    ok=m&(a>=250)
    if exclude_dark: ok&=(r+g+b)/3>150
    ys,xs=np.nonzero(m); x0,x1,y0,y1=xs.min(),xs.max(),ys.min(),ys.max()
    yy,xx=np.nonzero(ok)
    # t: 0 at bottom-left, 1 at top-right
    t=((xx-x0)/(x1-x0)+(y1-yy)/(y1-y0))/2
    out=[]
    for k in range(n):
        lo,hi=k/n,(k+1)/n; sel=(t>=lo)&(t<hi)
        if sel.sum()<20: continue
        c=[int(np.median(ch[yy[sel],xx[sel]])) for ch in (r,g,b)]
        out.append(((lo+hi)/2,(c[0]<<16)|(c[1]<<8)|c[2]))
    return (x0*SC,y0*SC,(x1-x0)*SC,(y1-y0)*SC),out
grads={'silver':stops('silver',8,True),'four':stops('four',6),'gear':stops('gear',6)}
for k,v in grads.items(): print(k,[ (round(t,2),hex(c)) for t,c in v[1]])
def fmt(x): 
    s=('%.1f'%x).rstrip('0').rstrip('.'); return '0' if s=='-0' else s
def asarr(cmds):
    out=[]
    for c in cmds:
        if c[0]<2: out+= [str(c[0]),fmt(c[1].real*SC),fmt(c[1].imag*SC)]
        else: out+=['2',fmt(c[1].real*SC),fmt(c[1].imag*SC),fmt(c[2].real*SC),fmt(c[2].imag*SC)]
    lines=[];line=[]
    for i,x in enumerate(out):
        line.append(x)
        if len(line)>=24: lines.append('\t\t\t'+', '.join(line)); line=[]
    if line: lines.append('\t\t\t'+', '.join(line))
    return ',\n'.join(lines)
import json
json.dump({'H':H*SC,'W':OUTW,'grads':grads},open('meta.json','w'))
with open('paths.as.txt','w') as f:
    for nm in ['silver','four','gear']:
        f.write('\t\tprivate static const %s:Array = [\n%s\n\t\t];\n\n'%(nm.upper(),asarr(layers[nm])))
# SVG preview (no filters)
def svgd(cmds):
    o=[]
    for c in cmds:
        if c[0]==0: o.append('M%s %s'%(fmt(c[1].real*SC),fmt(c[1].imag*SC)))
        elif c[0]==1: o.append('L%s %s'%(fmt(c[1].real*SC),fmt(c[1].imag*SC)))
        else: o.append('Q%s %s %s %s'%(fmt(c[1].real*SC),fmt(c[1].imag*SC),fmt(c[2].real*SC),fmt(c[2].imag*SC)))
    return ' '.join(o)
defs=[];body=[]
for nm in ['silver','four','gear']:
    (x,y,w,h),st=grads[nm]
    defs.append('<linearGradient id="%s" gradientUnits="userSpaceOnUse" x1="%s" y1="%s" x2="%s" y2="%s">%s</linearGradient>'%(nm,x,y+h,x+w,y,''.join('<stop offset="%s" stop-color="#%06x"/>'%(t,c) for t,c in st)))
    body.append('<path fill-rule="evenodd" fill="url(#%s)" d="%s"/>'%(nm,svgd(layers[nm])))
open('preview.svg','w').write('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d"><rect width="100%%" height="100%%" fill="#1d3b1d"/><defs>%s</defs>%s</svg>'%(OUTW,H*SC+1,''.join(defs),''.join(body)))
import cairosvg; cairosvg.svg2png(url='preview.svg',write_to='preview.png',scale=1.5)

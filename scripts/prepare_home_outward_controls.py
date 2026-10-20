"""Derive Home from its immutable 466px asset and canonical gears wallpaper."""
from pathlib import Path
from PIL import Image, ImageDraw
import hashlib, json
try:
    from .png_to_rgb565a8 import decode
except ImportError:
    from png_to_rgb565a8 import decode
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'shared/references/home-reference-466-source-derivative.png'
WALL=ROOT/'shared/assets/settings-gears-wallpaper-466.png'
OUT=ROOT/'shared/assets/home-controls-outward-10-466.png'
GROUPS=[((239,102),(0,-10)),((158,148),(-6,-8)),((109,228),(-10,0)),((364,228),(10,0)),((155,310),(-8,6)),((321,310),(6,8)),((236,362),(0,10))]
# These are the measured bounds of the approved status strip from the reference
# manifest (145,203,184,57). The former 224 px rounded mask also copied the old
# weather bubble back over its newly shifted position.
BANNER=(145,203,184,57)
LIVE_TIME=(211,219,56,22)
def compose():
    source=Image.open(SOURCE).convert('RGBA'); wall=Image.open(WALL).convert('RGBA')
    if source.size!=(466,466) or wall.size!=(466,466): raise ValueError('Expected 466x466 inputs')
    out=wall.copy(); src=source.load(); bg=wall.load(); dst=out.load()
    for (cx,cy),(dx,dy) in GROUPS:
        for y in range(max(0,cy-68),min(466,cy+69)):
            for x in range(max(0,cx-68),min(466,cx+69)):
                dist=(x-cx)**2+(y-cy)**2
                if dist>68**2: continue
                r,g,b,a=src[x,y]; peak=max(r,g,b)
                if peak<=8: continue
                # Attribute overlapping glows to the nearest icon center.
                if any((x-ox)**2+(y-oy)**2 < dist for (ox,oy),_ in GROUPS if (ox,oy)!=(cx,cy)): continue
                tx,ty=x+dx,y+dy
                if not (0<=tx<466 and 0<=ty<466): continue
                br,bg_,bb,_=bg[tx,ty]
                # Screen-composite the unchanged artwork pixels over the gears.
                coverage=min(255,peak)
                sr=255-(255-br)*(255-r)//255
                sg=255-(255-bg_)*(255-g)//255
                sb=255-(255-bb)*(255-b)//255
                dst[tx,ty]=(br+(sr-br)*coverage//255,bg_+(sg-bg_)*coverage//255,bb+(sb-bb)*coverage//255,255)
    # Restore only the approved banner bounds from the pre-UI17 image. Its
    # opaque black matte must become transparent over the gears. Use this
    # source slice rather than the isolated banner overlay, which would draw
    # the already-baked status icons a second time.
    banner=source.crop((BANNER[0],BANNER[1],BANNER[0]+BANNER[2],BANNER[1]+BANNER[3]))
    shape=Image.new('L',banner.size,0)
    ImageDraw.Draw(shape).rounded_rectangle((0,0,BANNER[2]-1,BANNER[3]-1),radius=28,fill=255)
    banner_pixels=banner.load(); shape_pixels=shape.load()
    for y in range(BANNER[3]):
        for x in range(BANNER[2]):
            r,g,b,_=banner_pixels[x,y]
            alpha=min(224,max(0,max(r,g,b)-24)*4)*shape_pixels[x,y]//255
            if alpha:
                banner_pixels[x,y]=(min(255,(r*255+alpha//2)//alpha),min(255,(g*255+alpha//2)//alpha),min(255,(b*255+alpha//2)//alpha),alpha)
            else:
                banner_pixels[x,y]=(0,0,0,0)
    out.alpha_composite(banner,(BANNER[0],BANNER[1]))
    # Remove only the baked clock glyphs. Interpolate the pill pixels at the
    # two edges of the declared live-time mask, matching the browser path.
    pixels=out.load(); x,y,width,height=LIVE_TIME
    for row in range(y,y+height):
        left=pixels[x-1,row]; right=pixels[x+width,row]
        for col in range(width):
            ratio=(col+1)/(width+1)
            pixels[x+col,row]=tuple(round(left[channel]*(1-ratio)+right[channel]*ratio) for channel in range(3))+(255,)
    # The separate voltage placeholder is content, not one of the seven moved controls.
    for y in range(139,157):
        for x in range(313,328):
            r,g,b,a=src[x,y]; peak=max(r,g,b)
            if peak <= 8: continue
            br,bg_,bb,_=bg[x,y]
            sr=255-(255-br)*(255-r)//255; sg=255-(255-bg_)*(255-g)//255; sb=255-(255-bb)*(255-b)//255
            dst[x,y]=(br+(sr-br)*peak//255,bg_+(sg-bg_)*peak//255,bb+(sb-bb)*peak//255,255)
    return out
def main():
    import argparse
    ap=argparse.ArgumentParser(); ap.add_argument('--check',action='store_true'); args=ap.parse_args()
    result=compose()
    if args.check:
        if Image.open(OUT).convert('RGBA').tobytes()!=result.tobytes(): raise SystemExit('Home composition mismatch')
        _,_,data=decode(OUT)
        if OUT.with_suffix('.bin').read_bytes()!=data: raise SystemExit('Home RGB565A8 mismatch')
        print('Home gears and outward controls verified'); return
    result.save(OUT); _,_,data=decode(OUT); OUT.with_suffix('.bin').write_bytes(data)
    print(hashlib.sha256(OUT.read_bytes()).hexdigest(),hashlib.sha256(OUT.with_suffix('.bin').read_bytes()).hexdigest())
if __name__=='__main__':main()

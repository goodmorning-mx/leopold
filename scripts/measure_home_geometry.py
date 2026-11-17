"""Record Home button, icon and clearance geometry from shared contracts/assets."""
from __future__ import annotations
import json
import math
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
UI = json.loads((ROOT/'shared/firmware-ui.json').read_text())
INTERACTION = json.loads((ROOT/'shared/interaction.json').read_text())['behaviour']['home_button_bounds']
RENDERING = json.loads((ROOT/'shared/rendering.json').read_text())['home']
ASSETS = json.loads((ROOT/'shared/asset-manifest.json').read_text())['assets']
HOME = next(screen for screen in UI['screens'] if screen['id']=='sky-home')
COMPONENTS = {item['id']: item for item in HOME['components'] if item['type']=='button'}
PILL = (137,209,192,48)
USB_SEGMENTS = RENDERING['usb_icon']['segments']

def visible_bbox(key: str):
    meta=ASSETS[key]; raw=(ROOT/'shared/assets'/meta['file']).read_bytes(); n=meta['width']*meta['height']
    alpha=Image.frombytes('L',(meta['width'],meta['height']),raw[n*2:])
    return alpha.point(lambda v: 255 if v >= 32 else 0).getbbox()

results=[]
for item in INTERACTION:
    c=COMPONENTS[item['id']]; cx,cy=item['final_center']; bx,by=item['move_from_center']
    nx,ny=item['normalized_center_after_existing_10px_shift']
    radius=math.hypot(bx-233,by-233)
    target=(233+(bx-233)*15/radius,233+(by-233)*15/radius)
    x,y,w,h=c['x'],c['y'],c['width'],c['height']
    hit_corners=[(x+dx,y+dy) for dx in (0,w) for dy in (0,h)]
    edge=233-max(math.hypot(px-233,py-233) for px,py in hit_corners)
    if item['id']=='open-usb':
        xs=[point for seg in USB_SEGMENTS for point in (seg[0],seg[2])]
        ys=[point for seg in USB_SEGMENTS for point in (seg[1],seg[3])]
        stroke=RENDERING['usb_icon']['stroke']
        icon=[round(x+(min(xs)-stroke/2),2),round(y+(min(ys)-stroke/2),2),round(x+(max(xs)+stroke/2),2),round(y+(max(ys)+stroke/2),2)]
        visible=[round(icon[0],2),round(icon[1],2),round(icon[2],2),round(icon[3],2)]
        method='shared LVGL USB line segments plus stroke radius'
    elif item['id']=='open-settings':
        # LVGL Montserrat 32 settings glyph is font-rasterized; report its
        # declared centered glyph box, and mark exact pixels capture-required.
        visible=[cx-16,cy-16,cx+16,cy+16]
        icon=visible; method='LVGL Montserrat 32 centered glyph box; exact alpha pixels pending native canvas export'
    else:
        key=item['id']; meta=ASSETS[key]; sx=RENDERING['icon_scales'][key]/100
        l,t,r,b=visible_bbox(key)
        iw,ih=meta['width'],meta['height']
        visible=[round(cx+(l-iw/2)*sx,2),round(cy+(t-ih/2)*sx,2),round(cx+(r-iw/2)*sx,2),round(cy+(b-ih/2)*sx,2)]
        icon=visible;method=f"manifest asset alpha >= 32, LVGL scale {RENDERING['icon_scales'][key]}%"
    pill_gap=max(0,max(PILL[0]-(x+w), x-(PILL[0]+PILL[2]), PILL[1]-(y+h), y-(PILL[1]+PILL[3])))
    results.append({
      'id':item['id'],'reviewed_baseline_center':item['move_from_center'],'previous_center_after_10px_move':item['original_center'],
      'normalized_center_after_existing_10px_shift':item['normalized_center_after_existing_10px_shift'],
      'symmetry_normalization_offset_px':item['symmetry_normalization_offset_px'],
      'target_center_after_15px_radial_move_px':[round(target[0],3),round(target[1],3)],
      'final_center':[cx,cy],
      'symmetry_correction_from_radial_target_px':[round(cx-target[0],3),round(cy-target[1],3)],
      'final_displacement_from_reviewed_baseline_px':round(math.hypot(cx-bx,cy-by),3),
      'final_radial_change_from_reviewed_baseline_px':round(math.hypot(cx-233,cy-233)-math.hypot(bx-233,by-233),3),
      'remaining_radial_move_after_normalization_px':round(math.hypot(cx-233,cy-233)-math.hypot(nx-233,ny-233),3),
      'control_and_hit_bounds':[x,y,w,h],'visible_icon_bounds_native_px':visible,
      'visible_bounds_method':method,'circular_edge_clearance_px':round(edge,2),
      'home_pill_bounds':list(PILL),'axis_aligned_clearance_to_pill_px':pill_gap,
      'pressed_glow_edge_clearance_px':round(edge-RENDERING['glass_button']['pressed_shadow_width'],2)
    })
report={'framebuffer':[466,466],'center':[233,233],'pill':{'bounds':list(PILL),'center':[233,233]},
        'source_contracts':{'geometry':'shared/interaction.json','components':'shared/firmware-ui.json','art':'shared/asset-manifest.json','scales':'shared/rendering.json'},
        'controls':results}
out=ROOT/'docs/evidence/home-geometry-2026-10-22.json';out.write_text(json.dumps(report,indent=2)+'\n');print(out)

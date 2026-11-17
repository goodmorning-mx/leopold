"""Behavioral contracts for the actual shared C++ model, not a layout copy."""
from __future__ import annotations
import json,os,shutil,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
HOST=r'''
#include "display_menu.h"
#include <cstdlib>
#include <iostream>
int main(int argc,char** argv){
 if(argc!=20)return 2;
 using namespace leopold::display_menu;
 State s;s.expanded=rowIndex(argv[1]);s.scroll=atoi(argv[2]);s.background=atoi(argv[3]);s.backgroundGalleryOffset=atoi(argv[4]);
 s.dark=atoi(argv[5]);s.accent=clamp(atoi(argv[6]),0,6);s.metric=atoi(argv[7]);s.spanish=atoi(argv[8]);s.country=clamp(atoi(argv[9]),0,2);s.keypad=atoi(argv[10]);
 copy(s.location,sizeof(s.location),argv[11]);copy(s.postal,sizeof(s.postal),argv[12]);copy(s.status,sizeof(s.status),argv[13]);
 s.clock24=atoi(argv[14]);s.clockFace=atoi(argv[15]);s.clockBackground=atoi(argv[16]);s.clockWeather=atoi(argv[17]);bool clock=atoi(argv[18]);
 Effect effect=act(s,argv[19],clock);
 leopold::SduiScreen screen;if(clock)buildClock(screen,s);else build(screen,s);
 char output[8192];if(!json(output,sizeof(output),screen,s,effect))return 3;
 std::cout<<output;
}
'''

def args(**overrides):
 d=dict(expanded='',scroll=0,background=3,gallery=3,dark=1,accent=4,metric=1,spanish=0,country=0,keypad=0,location='',postal='',status='',clock24=1,face=0,clockBackground=0,clockWeather=0,clock=0,action='')
 d.update(overrides);return list(d.values())

class DisplayMenuTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  compiler=shutil.which('c++') or shutil.which('clang++')
  if not compiler:raise unittest.SkipTest('Host C++ compiler unavailable')
  cls.temp=tempfile.TemporaryDirectory(prefix='leopold-display-');p=Path(cls.temp.name)
  (p/'main.cpp').write_text(HOST);cls.binary=p/'menu'
  subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-DLEOPOLD_WASM','-I'+str(ROOT/'shared'),'-I'+str(ROOT/'device/include'),str(p/'main.cpp'),'-o',str(cls.binary)],check=True,capture_output=True,text=True)
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def model(self,**kw):return json.loads(subprocess.check_output([str(self.binary),*map(str,args(**kw))]))
 def test_complete_rows_and_fixed_header(self):
  m=self.model();rows=[c for c in m['screen']['components'] if c['id'].startswith('display-row-')]
  self.assertEqual([r['id'] for r in rows],['display-row-background','display-row-appearance','display-row-clock','display-row-units','display-row-language','display-row-weather-location'])
  self.assertEqual(len({c['id'] for c in m['screen']['components']}),8)
  self.assertEqual(m['screen']['components'][0]['text'],'DISPLAY')
  self.assertEqual(m['screen']['components'][1]['y'],87)
 def test_expanded_controls_and_scroll_clamp(self):
  expected={'background':['display-background-3','display-background-0','display-background-1'],'appearance':['display-appearance-light','display-appearance-dark','display-accent-blue'],'units':['display-units-imperial','display-units-metric'],'language':['display-language-en','display-language-es'],'weather-location':['display-country-US','display-country-MX']}
  for row,ids in expected.items():
   with self.subTest(row=row):
    m=self.model(expanded=row,scroll=9999);components={c['id']:c for c in m['screen']['components']}
    self.assertTrue(set(ids)<=components.keys());self.assertEqual(components['menu-back']['y'],87)
    self.assertEqual(components['display-row-background']['y'],130-m['maxScroll'])
 def test_background_gallery_wraps_in_both_directions(self):
  for offset in range(4):
   m=self.model(expanded='background',gallery=offset)
   self.assertEqual([c['id'] for c in m['screen']['components'] if c['id'].startswith('display-background-')],
                    [f'display-background-{(offset+i)%4}' for i in range(3)])
 def test_languages_palette_and_selected_preferences(self):
  for dark in (0,1):
   for accent in range(7):
    m=self.model(expanded='appearance',dark=dark,accent=accent,spanish=1)
    c={c['id']:c for c in m['screen']['components']}
    self.assertEqual(c['submenu-title']['text'],'PANTALLA')
    self.assertEqual(sum(c['label'].startswith('[x] ') for c in c.values() if 'label'in c),1)
    self.assertNotEqual(c['display-appearance-light']['color'],c['display-appearance-dark']['color'])
 def test_shared_actions_toggle_preferences_and_no_unknown_effect(self):
  for action,key,value in [('display-units-imperial','measurementSystem','imperial'),('display-language-es','spanishLanguage',True),('display-appearance-light','appearanceDark',False),('display-accent-violet','accentColor','violet'),('display-background-2','backgroundArt',2),('display-background-3','backgroundArt',3)]:
   with self.subTest(action=action):
    m=self.model(action=action);self.assertEqual(m['effect'],3);self.assertEqual(m['state'][key],value)
  self.assertEqual(self.model(action='display-row-clock')['effect'],2)
  self.assertEqual(self.model(action='menu-back')['effect'],1)
  self.assertEqual(self.model(action='unknown')['effect'],0)
 def test_postal_validation_preserves_last_verified_location(self):
  for country,postal,valid in [(1,'12345',True),(1,'123456789',True),(1,'12',False),(2,'12345',True),(2,'123456789',False),(2,'abcde',False)]:
   m=self.model(expanded='weather-location',country=country,keypad=1,location='US:90210',postal=postal,action='display-postal-apply')
   self.assertEqual(m['effect'],4 if valid else 0)
   if not valid:self.assertIn('not valid',m['state']['postalStatus'])
  m=self.model(expanded='weather-location',country=1,keypad=1,postal='12345',action='display-postal-cancel')
  self.assertEqual(m['state']['postalDraft'],'');self.assertFalse(m['state']['keypadOpen'])
 def test_keypad_bounded_and_component_budget(self):
  m=self.model(expanded='weather-location',country=1,keypad=1,postal='123456789',action='postal-key-0')
  self.assertEqual(m['state']['postalDraft'],'123456789')
  self.assertLessEqual(len(m['screen']['components']),32)
  self.assertEqual(sum(c['id'].startswith('postal-key-') for c in m['screen']['components']),12)
  m=self.model(expanded='weather-location',country=1,keypad=1,postal='12345',action='postal-key-delete');self.assertEqual(m['state']['postalDraft'],'1234')
 def test_clock_same_builder_has_all_controls(self):
  m=self.model(clock=1,action='clock-format-12');self.assertEqual(m['state']['clock24'],False)
  ids={c['id']for c in m['screen']['components']}
  self.assertTrue({'clock-format-12','clock-format-24','clock-face-4','clock-background-2','clock-weather-yes','clock-weather-no'}<=ids)
 def test_json_escapes_dynamic_text(self):
  m=self.model(expanded='weather-location',country=1,status='Quoted "city"\\path\nEspañol');self.assertIn('Quoted "city"',m['state']['postalStatus'])
 def test_native_and_actual_wasm_models_match(self):
  node=Path(shutil.which('node') or ROOT.parent/'Toolchains/node/bin/node');wasm=ROOT/'frontend/public/wasm/lvgl.wasm';js=wasm.with_suffix('.js')
  if not node.exists() or not wasm.exists():
   if os.environ.get('LEOPOLD_REQUIRE_DISPLAY_WASM')=='1':self.fail('Rebuilt WASM and installed Node are required by display-check')
   self.skipTest('Rebuilt WASM and installed Node required; native-only environment')
  cases=[]
  for dark in (0,1):
   for accent in range(7):
    for row in ('','background','appearance','units','language','weather-location'):
     cases.append(args(expanded=row,dark=dark,accent=accent,spanish=accent%2,scroll=500,country=1,keypad=row=='weather-location'))
  for action in ('display-row-clock','display-units-imperial','display-language-es','display-country-MX','postal-key-1','display-postal-apply','menu-back'):
   cases.append(args(expanded='weather-location',country=1,keypad=1,postal='12345',action=action))
  cases.append(args(clock=1,action='clock-format-12'))
  cases.append(args(expanded='weather-location',country=1,status='Quoted \"city\"\\path\nEspañol'))
  request=Path(self.temp.name)/'cases.json';request.write_text(json.dumps(cases))
  runner=Path(self.temp.name)/'compare.mjs';runner.write_text('''import fs from 'node:fs';import {pathToFileURL} from 'node:url';
const [js,wasm,requests]=process.argv.slice(2);const factory=(await import(pathToFileURL(js))).default;
const module=await factory({instantiateWasm(imports,receive){WebAssembly.instantiate(fs.readFileSync(wasm),imports).then(result=>receive(result.instance));return {};}});module._lvgl_wasm_init();
const cases=JSON.parse(fs.readFileSync(requests,'utf8'));const out=cases.map(values=>JSON.parse(module.ccall('lvgl_wasm_display_model','string',values.map(v=>typeof v==='string'?'string':'number'),values)));
console.log(JSON.stringify(out));''')
  output=subprocess.run([str(node),str(runner),str(js),str(wasm),str(request)],capture_output=True,text=True)
  self.assertEqual(output.returncode,0,output.stderr)
  actual=json.loads(output.stdout)
  for case,result in zip(cases,actual):
   with self.subTest(case=case):self.assertEqual(json.loads(subprocess.check_output([str(self.binary),*map(lambda v:str(int(v)) if isinstance(v,bool) else str(v),case)])),result)

if __name__=='__main__':unittest.main()

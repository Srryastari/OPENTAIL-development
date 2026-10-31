#!/usr/bin/env python3
"""All 140 IDs: real Java/JNI frames vs independent exact CORE source build.

Host renderer timings are NOT Android display FPS or radio measurements.
Usage: JAVA_HOME=... python3 android/qa/test_full_offline_preview.py --core-dir pinned-effects50
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
CPP=ROOT/'android/app/src/main/cpp'
JAVA=ROOT/'android/app/src/main/java/com/opentail/android'
PIN='1767b144e193ed6978816899ebb5eba237ee6e2d'
CONTINUITY_PIN='7c0fcc450a7619a57603aabf833dea855a18bcd8'
CONTINUITY_FILES={'effects_1d.c','effects_1d.h','ot_auto_effects.c','ot_auto_effects.h'}
SOURCES=['wled_fx_compat.c','effects_1d.c','wled_1d.c','palette_1d.c','particle_system_1d.c','ot_auto_effects.c']

ORACLE=r'''
#include "effects_1d.h"
#include "palette_1d.h"
#include <stdlib.h>
typedef struct {ot_effects_1d_state_t s;ot_wled_palette_random_t pal;uint32_t seed;
 uint16_t id;ot_wled_rgb_t px[30];} oracle;
static uint8_t draw(void *ctx){uint32_t *s=ctx;*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return (uint8_t)*s;}
size_t ids(uint16_t *p){return ot_effects_1d_supported_ids(p,140);}
int flags(int id){ot_wled_fx_metadata_t m={0},w={0};if(!ot_effects_1d_metadata(id,&m))return -1;
 return m.supports_speed|m.supports_intensity<<1|m.supports_foreground<<2|
 m.supports_background<<3|m.supports_palette<<4|ot_wled_fx_metadata(id,&w)<<5;}
int palette_ok(int id,int pal){return ot_palette_1d_supported(id,pal);}
void *make(int id){oracle *p=calloc(1,sizeof(*p));p->id=id;p->seed=0x5eed1234;
 ot_effects_1d_reset(&p->s,0x5eed1234);ot_wled_palette_global_init(&p->pal,draw,&p->seed);return p;}
void drop(oracle *p){ot_wled_fx_reset(&p->s.wled);free(p);}
int frame(oracle *p,uint32_t ms,int speed,int intensity,int bri,int palette,
 uint32_t fg,uint32_t bg,float hz,float magnitude,uint32_t *out){
 ot_wled_palette16_t custom;const ot_wled_palette16_t *resolved=NULL;
 const uint8_t stops[]={0,12,50,100,255,200,3,22};
 if(!ot_wled_palette_global_tick(&p->pal,ms,5,16,750,true,draw,&p->seed))return 0;
 if(palette==1)resolved=&p->pal.current;
 else if(palette>72){if(!ot_wled_palette_from_gradient_stops(stops,2,&custom))return 0;resolved=&custom;}
 ot_wled_audio_peak_t audio={hz,magnitude};
 ot_wled_fx_config_t cfg={.effect_id=p->id,.speed=speed,.intensity=intensity,
 .brightness=bri,.palette_id=palette,.frame_time_ms=16,
 .foreground={(uint8_t)(fg>>16),(uint8_t)(fg>>8),(uint8_t)fg},
 .background={(uint8_t)(bg>>16),(uint8_t)(bg>>8),(uint8_t)bg},
 .audio_peak=&audio,.resolved_palette=resolved};
 if(!ot_effects_1d_render(&cfg,&p->s,ms,p->px,30))return 0;
 for(int i=0;i<30;i++)out[i]=0xff000000u|(uint32_t)p->px[i].r<<16|(uint32_t)p->px[i].g<<8|p->px[i].b;
 return 1;
}
'''

DRIVER=r'''package com.opentail.android;
import java.io.*;import java.util.*;
public class FullPreviewHarness {
 public static void main(String[] args)throws Exception{
  BufferedReader in=new BufferedReader(new InputStreamReader(System.in));
  DataOutputStream out=new DataOutputStream(new BufferedOutputStream(System.out));
  OfflineCoreRenderer r=null;int[] colors=new int[30];Map<Integer,List<Long>> durations=new TreeMap<>();
  byte[] custom={0,12,50,100,(byte)255,(byte)200,3,22};String line;
  try{while((line=in.readLine())!=null){
   String[] f=line.split(",");int reset=Integer.parseInt(f[0]),id=Integer.parseInt(f[1]);
   if(reset!=0){if(r!=null)r.close();r=new OfflineCoreRenderer(id,30);if(!r.available())throw new AssertionError("missing "+id);}
   long ms=Long.parseLong(f[2]);int speed=Integer.parseInt(f[3]),intensity=Integer.parseInt(f[4]),bri=Integer.parseInt(f[5]),pal=Integer.parseInt(f[6]);
   int fg=(int)Long.parseLong(f[7]),bg=(int)Long.parseLong(f[8]);
   long start=System.nanoTime();
   boolean ok=r.render(ms,16,speed,intensity,bri,pal,fg,bg,pal>72?custom:null,440,2048,true,colors);
   long ns=System.nanoTime()-start;
   if(!ok)throw new AssertionError("rejected "+line);
   durations.computeIfAbsent(id,k->new ArrayList<>()).add(ns);
   for(int c:colors)out.writeInt(c);
  }}finally{if(r!=null){r.close();r.close();}out.flush();}
  System.err.print("{");boolean first=true;
  for(Map.Entry<Integer,List<Long>> e:durations.entrySet()){
   List<Long> v=e.getValue();Collections.sort(v);long sum=0;for(long n:v)sum+=n;
   if(!first)System.err.print(",");first=false;
   System.err.print("\""+e.getKey()+"\":{\"frames\":"+v.size()+",\"mean_ns\":"+(sum/v.size())+
    ",\"p50_ns\":"+v.get(v.size()/2)+",\"p95_ns\":"+v.get(v.size()*95/100)+",\"p99_ns\":"+v.get(v.size()*99/100)+"}");
  }System.err.println("}");
  try(OfflineCoreRenderer invalid=new OfflineCoreRenderer(65535,30)){
   if(invalid.available())throw new AssertionError("unknown effect accepted");
  }
  try(OfflineCoreRenderer audio=new OfflineCoreRenderer(185,30)){
   if(audio.render(0,16,128,128,160,0,-1,0xff000000,null,0,0,false,colors))throw new AssertionError("fabricated audio");
   if(audio.render(0,16,128,128,160,0,-1,0xff000000,null,Float.NaN,2,true,colors))throw new AssertionError("NaN audio");
   if(audio.render(0,16,128,128,160,0,-1,0xff000000,null,440,2000,true,new int[29]))throw new AssertionError("bad buffer");
  }
  try(OfflineCoreRenderer palette=new OfflineCoreRenderer(65,30)){
   if(palette.render(0,16,128,128,160,200,-1,0xff000000,null,0,0,false,colors))throw new AssertionError("invented custom");
  }
  try(OfflineCoreRenderer speed=new OfflineCoreRenderer(9,30)){
   if(!speed.render(1000,16,0,128,160,0,-1,0xff000000,null,0,0,false,colors))throw new AssertionError("slow speed");
   int[] slow=colors.clone();
   if(!speed.render(1000,16,255,128,160,0,-1,0xff000000,null,0,0,false,colors))throw new AssertionError("fast speed");
   if(Arrays.equals(slow,colors))throw new AssertionError("Speed edit failed to update same-instance preview");
   if(!speed.render(1000,16,255,128,0,0,-1,0xff000000,null,0,0,false,colors))throw new AssertionError("brightness edit");
   for(int c:colors)if(c!=0xff000000)throw new AssertionError("absolute off");
  }
 }
}
'''

def run(args,**kw):
    return subprocess.check_output([str(x) for x in args],cwd=ROOT,**kw)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--core-dir',type=Path,default=ROOT/'pinned-effects50')
    parser.add_argument('--continuity-dir',type=Path,default=ROOT/'pinned-effects80')
    args=parser.parse_args();core=args.core_dir.resolve()
    continuity=args.continuity_dir.resolve()
    assert run(['git','-C',core,'rev-parse','HEAD'],text=True).strip()==PIN
    assert run(['git','-C',continuity,'rev-parse','HEAD'],text=True).strip()==CONTINUITY_PIN
    manifest=json.loads((CPP/'core/SOURCE.json').read_text())
    assert manifest['source_commit']==PIN
    assert manifest['seamless_loop_source_commit']==CONTINUITY_PIN
    assert set(manifest['seamless_loop_files'])==CONTINUITY_FILES
    for name,digest in manifest['files'].items():
        original_dir=continuity if name in CONTINUITY_FILES else core
        local=(CPP/'core'/name).read_bytes()
        original=(original_dir/'core-firmware/main'/name).read_bytes()
        assert local==original and hashlib.sha256(local).hexdigest()==digest,name
    catalog=[(int(i),n) for i,n in re.findall(r'new Effect\((\d+),\s*"([^"]+)"\)',(JAVA/'NativeEffectCatalog.java').read_text())]
    assert len(catalog)==140 and len(set(i for i,_ in catalog))==140
    jdk=Path(os.environ['JAVA_HOME'])
    with tempfile.TemporaryDirectory(prefix='all-effects-') as tmp:
        t=Path(tmp);(t/'oracle.c').write_text(ORACLE)
        original=core/'core-firmware/main'
        corrected=continuity/'core-firmware/main'
        # CORE oracle and copied JNI independently compile identical corrected
        # proprietary algorithms; unchanged WLED/palette/particle source stays
        # byte-pinned to Effects #50, no Android-specific frame substitutions.
        oracle_sources=[(corrected if f in CONTINUITY_FILES else original)/f
                        for f in SOURCES]
        run(['cc','-std=c11','-O2','-shared','-fPIC',
             '-I',corrected,'-I',original,t/'oracle.c',
             *oracle_sources,'-lm','-o',t/'oracle.so'])
        run(['cc','-std=c11','-O2','-shared','-fPIC','-I',CPP,'-I',CPP/'core',
             '-I',jdk/'include','-I',jdk/'include/linux',CPP/'preview.c',CPP/'preview_jni.c',
             *[CPP/'core'/f for f in SOURCES],'-lm','-o',t/'libopentail_preview.so'])
        (t/'FullPreviewHarness.java').write_text(DRIVER)
        run([jdk/'bin/javac','-d',t,JAVA/'OfflineCoreRenderer.java',t/'FullPreviewHarness.java'])
        lib=C.CDLL(str(t/'oracle.so'))
        lib.ids.argtypes=[C.POINTER(C.c_uint16)];lib.ids.restype=C.c_size_t
        native=(C.c_uint16*140)();assert lib.ids(native)==140
        assert set(native)==set(i for i,_ in catalog)
        lib.make.argtypes=[C.c_int];lib.make.restype=C.c_void_p
        lib.drop.argtypes=[C.c_void_p]
        lib.frame.argtypes=[C.c_void_p,C.c_uint32,*[C.c_int]*4,C.c_uint32,C.c_uint32,C.c_float,C.c_float,C.POINTER(C.c_uint32)]
        lib.frame.restype=C.c_int
        # Dedicated Startup/Shutdown animations are true one-shots: they
        # advance through different frames and hold their final image.
        # A loop-seam assertion would be wrong for these intentional events.
        # Use the independent pinned CORE oracle, not a substituted UI demo.
        one_shots=(32769,32770,32771,32773,32787)
        event_duration=350+(255-128)*14
        assert len(one_shots)==5
        for fx in one_shots:
            h=lib.make(fx);samples=[]
            for ms in (0,event_duration//2,event_duration,
                       event_duration+300,event_duration+1200):
                rgb=(C.c_uint32*30)()
                assert lib.frame(h,ms,128,255,160,0,
                                 0xffd735f4,0xff174a03,440,2048,rgb),(fx,ms)
                samples.append(tuple(rgb))
            lib.drop(h)
            assert samples[0]!=samples[1] and samples[1]!=samples[2], \
                ('event did not animate through its one-shot',fx)
            assert samples[2]==samples[3]==samples[4], \
                ('one-shot terminal frame changed after completion',fx)
        expected=bytearray();plan=[];rows=[];count=0
        frame=(C.c_uint32*30)()
        for effect,name in catalog:
            flags=lib.flags(effect)
            profiles=[(0,0,0,0,0xffd735f4,0xff174a03),
                      (0,128,160,0,0xffd735f4,0xff174a03),
                      (255,128,160,0,0xffd735f4,0xff174a03),
                      (128,255,255,0,0xff12bc56,0xffe540c9)]
            for pal in range(1,73):
                if lib.palette_ok(effect,pal):profiles.append((128,128,160,pal,0xffd735f4,0xff174a03))
            if flags&16:profiles.append((128,128,160,200,0xffd735f4,0xff174a03))
            hashes=[];start_count=count
            named_phone_dynamic=(1,26,38,91)
            for profile_index,profile in enumerate(profiles):
                speed,intensity,bri,pal,fg,bg=profile
                handle=lib.make(effect);h=hashlib.sha256()
                distinct_frames=set() if profile_index==1 and (
                    effect>=32768 or effect in named_phone_dynamic) else None
                frames=420 if pal==1 else 180 if profile_index<4 else 12
                for step in range(frames):
                    # Change all authoring parameters without resetting native state.
                    if step==80 and profile_index==3:
                        speed,intensity,bri,fg,bg=240,32,24,0xff336699,0xffcd2480
                    ms=step*16
                    assert lib.frame(handle,ms,speed,intensity,bri,pal,fg,bg,440,2048,frame),(effect,profile,step)
                    data=b''.join(int(x).to_bytes(4,'big') for x in frame)
                    if distinct_frames is not None:
                        distinct_frames.add(hashlib.sha256(data).digest())
                    if bri==0:assert all(x==0xff000000 for x in frame),(effect,'zero brightness')
                    expected.extend(data);h.update(data)
                    plan.append(','.join(map(str,[int(step==0),effect,ms,speed,intensity,bri,pal,fg,bg])))
                    count+=1
                if distinct_frames is not None:
                    assert len(distinct_frames)>1, (
                        effect,name,'does not animate across 180 offline frames')
                hashes.append(h.hexdigest());lib.drop(handle)
            rows.append({'id':effect,'name':name,'source_adapter':bool(flags&32),
                         'speed_supported':bool(flags&1),'intensity_supported':bool(flags&2),
                         'palette_supported':bool(flags&16),'profiles':len(profiles),
                         'frames':count-start_count,'speed_0_vs_255_trace_differs':hashes[1]!=hashes[2]})
        result=subprocess.run([str(jdk/'bin/java'),'-Djava.library.path='+str(t),'-cp',str(t),
                               'com.opentail.android.FullPreviewHarness'],
                              input=('\n'.join(plan)+'\n').encode(),capture_output=True,check=True)
        assert result.stdout==expected,'JNI frames differ from authoritative CORE'
        timings=json.loads(result.stderr)
        for row in rows:
            row['jni_timing']=timings[str(row['id'])]
            row['host_renderer_only_fps']=round(1e9/row['jni_timing']['mean_ns'],1)
        report={'core_commit':PIN,'seamless_loop_commit':CONTINUITY_PIN,'candidate_commit':run(['git','rev-parse','HEAD'],text=True).strip(),
                'tracked_android_dirty':bool(run(['git','diff','--name-only','HEAD','--','android'],text=True).strip()),
                'effects':len(rows),'source_adapters':sum(r['source_adapter'] for r in rows),
                'frames':count,'argb_bytes':len(expected),'parity':'byte-exact JNI vs independent pinned CORE build',
                'timing_scope':'Linux host JNI renderer only; excludes Android display/composition/microphone acquisition/BLE. Not measured phone FPS.',
                'audio_fixture':'test input 440 Hz, PCM magnitude 2048; app uses live Android microphone, never this fixture',
                'per_effect':rows}
        print(json.dumps(report,indent=2))

if __name__=='__main__':main()

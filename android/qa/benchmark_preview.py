#!/usr/bin/env python3
"""Deterministic source benchmark, NOT an Android/radio/display benchmark.

Runs the actual baseline/candidate CoreFrameCapture with a fake ATT/Handler,
and compares the Java offline subset byte-for-byte with immutable CORE C.
Requires JDK (JAVA_HOME), gcc, git. No external Python dependencies.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'android/app/src/main/java/com/opentail/android'
BASE = '619298f1a574a502a06d769b9c7e0a02925ee8dd'
CORE = '1767b144e193ed6978816899ebb5eba237ee6e2d'
JDK = Path(os.environ.get('JAVA_HOME', '/usr')) / 'bin'

def run(args, **kw):
    return subprocess.check_output([str(a) for a in args], cwd=ROOT, **kw)

def git_file(sha, path):
    return run(['git', 'show', f'{sha}:{path}'])

def put(root, name, text):
    target = root / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text)

STUBS = {
    'android/os/Looper.java': '''package android.os;
public class Looper { public static Looper getMainLooper(){return new Looper();} }''',
    'android/os/Build.java': '''package android.os;
public class Build { public static class VERSION {public static int SDK_INT=35;} }''',
    'android/bluetooth/BluetoothStatusCodes.java': '''package android.bluetooth;
public class BluetoothStatusCodes {public static final int SUCCESS=0;}''',
    'android/bluetooth/BluetoothGattCharacteristic.java': '''package android.bluetooth;
public class BluetoothGattCharacteristic {public static final int WRITE_TYPE_DEFAULT=2;
public void setWriteType(int n){} public void setValue(byte[] d){} }''',
    'android/bluetooth/BluetoothGatt.java': '''package android.bluetooth;
public class BluetoothGatt {public static final int GATT_SUCCESS=0;
public byte[] last; public int writes,reads; public boolean start=true;
public int writeCharacteristic(BluetoothGattCharacteristic c,byte[] d,int type){last=d;writes++;return start?0:1;}
public boolean writeCharacteristic(BluetoothGattCharacteristic c){writes++;return start;}
public boolean readCharacteristic(BluetoothGattCharacteristic c){reads++;return start;}}''',
    'android/os/Handler.java': '''package android.os;
import java.util.*;
public class Handler {
static class Task {Runnable r;long at;Task(Runnable r,long at){this.r=r;this.at=at;}}
public static long now;public static int peak;static List<Task> tasks=new ArrayList<>();
public Handler(Looper l){} public boolean postDelayed(Runnable r,long delay){tasks.add(new Task(r,now+delay));peak=Math.max(peak,tasks.size());return true;}
public void removeCallbacks(Runnable r){tasks.removeIf(t->t.r==r);}
public static int pending(){return tasks.size();}
public static void reset(){tasks.clear();now=0;peak=0;}
public static void advance(long dt){long end=now+dt;while(true){Task n=tasks.stream().filter(t->t.at<=end).min(Comparator.comparingLong(t->t.at)).orElse(null);if(n==null)break;tasks.remove(n);now=n.at;n.r.run();}now=end;}}
''',
    'com/opentail/android/ExactLedFrame.java': '''package com.opentail.android;
class ExactLedFrame {static final int MAX_PIXELS=65535;}''',
}

HARNESS = r'''package com.opentail.android;
import android.os.Handler;
import android.bluetooth.*;
import java.util.*;
public class CaptureHarness {
static void check(boolean v,String m){if(!v)throw new AssertionError(m);}
static class Fixture {
 BluetoothGatt g=new BluetoothGatt();CoreFrameCapture c;
 int callbacks;String error;byte[] result;
 Fixture(){Handler.reset();c=new CoreFrameCapture((o,s,r,e)->{callbacks++;error=e;result=r;});
 c.configure(g,new BluetoothGattCharacteristic(),new BluetoothGattCharacteristic());}
 void written(){Handler.advance(10);c.written(g,0);}
 void read(byte[] a){Handler.advance(10);c.read(g,a,0);}
}
static byte[] rgb(int count){byte[] p=new byte[count*3];for(int i=0;i<p.length;i++)p[i]=(byte)(i*37);return p;}
static void u16(byte[] a,int p,int n){a[p]=(byte)n;a[p+1]=(byte)(n>>8);}
static byte[] header(int count,byte[] rgb){byte[] a={1,1,1,0,0,0,7,0,0,0,0,0};u16(a,4,count);u16(a,10,BleConfigCodec.crc16(rgb));return a;}
static byte[] chunk(byte[] rgb,int offset,int n){byte[] a=new byte[11+3*n];a[0]=1;a[1]=2;a[2]=1;a[4]=7;u16(a,8,offset);a[10]=(byte)n;System.arraycopy(rgb,3*offset,a,11,3*n);return a;}
static Fixture transfer(int count,int corrupt){Fixture f=new Fixture();byte[] rgb=rgb(count);f.c.request(0);f.written();f.read(header(count,rgb));
 for(int o=0;o<count && f.c.busy();o+=56){int n=Math.min(56,count-o);f.written();byte[] a=chunk(rgb,o,n);
 if(corrupt==1)a[4]=8;if(corrupt==2)a[8]++;if(corrupt==3)a[11]^=1;if(corrupt==4)a=Arrays.copyOf(a,a.length-1);
 f.read(a);}
 return f;}
public static void main(String[] args){
 for(int n:new int[]{1,56,57,522,6000,65535}){Fixture f=transfer(n,0);
 check(f.callbacks==1 && f.error==null && Arrays.equals(rgb(n),f.result),"complete RGB/CRC "+n);
 System.out.println("{\"pixels\":"+n+",\"att_operations\":"+(f.g.writes+f.g.reads)+",\"simulated_ms\":"+Handler.now+",\"pending_timers\":"+Handler.pending()+",\"peak_timers\":"+Handler.peak+"}");
 byte[] retained=f.result.clone();f.c.request(0);check(Arrays.equals(retained,f.result),"buffer ownership");f.c.cancel();Handler.advance(9000);check(f.callbacks==1,"cancel callback");}
 for(int corrupt=1;corrupt<=4;corrupt++){Fixture f=transfer(57,corrupt);check(f.callbacks==1 && f.result==null && f.error!=null,"reject corrupt "+corrupt);}
 Fixture f=new Fixture();f.c.request(0);f.c.abortForStop();check(f.c.busy(),"must drain ATT");f.written();check(!f.c.busy() && f.g.reads==0 && f.callbacks==1,"yield after current write");
 f=new Fixture();f.c.request(0);f.written();f.c.abortForStop();f.read(header(30,rgb(30)));check(!f.c.busy() && f.g.writes==1,"yield after current read");
 f=new Fixture();f.c.request(0);f.written();byte[] pending=header(30,rgb(30));pending[2]=0;f.read(pending);f.c.abortForStop();Handler.advance(100);check(!f.c.busy() && f.g.reads==1,"cancel scheduled poll");
 f=new Fixture();f.c.request(0);Handler.advance(8000);check(!f.c.busy() && f.error.contains("timed out"),"write watchdog");
 f=new Fixture();f.c.request(0);f.written();Handler.advance(8000);check(!f.c.busy() && f.error.contains("timed out"),"read watchdog");
 f=new Fixture();f.g.start=false;f.c.request(0);check(!f.c.busy() && f.callbacks==1,"failed start");
 f=new Fixture();f.c.request(0);f.c.written(new BluetoothGatt(),0);check(f.g.reads==0,"wrong session");f.c.cancel();Handler.advance(9000);check(f.callbacks==0,"retired session");
 System.err.println("PASS: transport complete/corrupt/timeout/abort/poll/session/buffer regressions");
}}
'''

C_DRIVER = r'''
#include "wled_fx_compat.h"
#include <stdio.h>
int main(void){
 const int ids[]={0,1,3,9,12},ns[]={1,30,257},values[]={0,128,255},br[]={0,1,24,160,255};
 const uint64_t times[]={0,1,23,24,500,2000,5000,65535,65536,4294967294ULL,4294967297ULL};
 for(int e=0;e<5;e++)for(int n=0;n<3;n++)for(int s=0;s<3;s++)for(int in=0;in<3;in++)for(int b=0;b<5;b++){
 ot_wled_fx_config_t c={.effect_id=ids[e],.speed=values[s],.intensity=values[in],.brightness=br[b],.palette_id=0,.frame_time_ms=16,.foreground={215,53,244},.background={17,93,3}};
 ot_wled_fx_state_t state={0};ot_wled_rgb_t px[257];
 for(int t=0;t<11;t++){if(!ot_wled_fx_render(&c,&state,(uint32_t)times[t],px,ns[n]))return 2;
 for(int i=0;i<ns[n];i++){putchar(px[i].r);putchar(px[i].g);putchar(px[i].b);}}}
 return 0;
}
'''
JAVA_DRIVER = r'''package com.opentail.android;
import java.io.*;
public class OfflineHarness {
 public static void main(String[] args)throws Exception{
 int[] ids={0,1,3,9,12},ns={1,30,257},values={0,128,255},br={0,1,24,160,255};
 long[] times={0,1,23,24,500,2000,5000,65535,65536,4294967294L,4294967297L};
 BufferedOutputStream out=new BufferedOutputStream(System.out);
 for(int id:ids)for(int n:ns)for(int speed:values)for(int intensity:values)for(int brightness:br){
 OfflineFxSketch r=new OfflineFxSketch();int[] px=new int[n];
 for(long t:times){if(!r.render(id,0,t,0xffd735f4,0xff115d03,speed,intensity,brightness,px))throw new AssertionError("rejected");
 for(int color:px){if((color>>>24)!=255)throw new AssertionError("alpha");out.write(color>>16);out.write(color>>8);out.write(color);}}}
 out.flush();
 OfflineFxSketch r=new OfflineFxSketch();int[] px=new int[30];
 for(int i=0;i<100000;i++)r.render(9,0,i*17L,0xffd735f4,0xff115d03,128,128,160,px);
 long[] durations=new long[10000];
 for(int i=0;i<durations.length;i++){long start=System.nanoTime();r.render(9,0,i*17L,0xffd735f4,0xff115d03,128,128,160,px);durations[i]=System.nanoTime()-start;}
 java.util.Arrays.sort(durations);
 System.err.println("{\"host_render_p50_ns\":"+durations[5000]+",\"host_render_p95_ns\":"+durations[9500]+",\"host_render_p99_ns\":"+durations[9900]+",\"samples\":10000}");
 }
}
'''

QUEUE_HARNESS = r'''package com.opentail.android;
import android.bluetooth.*;
import android.os.*;
import java.util.*;
public class QueueHarness {
 BluetoothGatt gatt=new BluetoothGatt();BluetoothGattCharacteristic coreInfo;
 Handler ui=new Handler(Looper.getMainLooper());
 boolean stopped,writePending,readReplyPending,cfgBusy,fxCapsBusy,infoReadPending,
 triggerMapBusy,authPending,pairPending,challengePending,frameCallbackPending,
 infoReadQueued,connected=true;
 int queuedOutputRead=-1;BluetoothGatt queuedOutputOwner,previewCommandOwner;
 Runnable previewCommand;byte[] nextPacket,afterStopPacket,pendingPacket;
 int actions,outputReads;List<Integer> sent=new ArrayList<>();
 class Palette {boolean busy(){return false;}void abortForStop(){}}
 Palette palette=new Palette();
 CoreFrameCapture frameCapture=new CoreFrameCapture((o,s,r,e)->pumpQueue());
 QueueHarness(){Handler.reset();frameCapture.configure(gatt,new BluetoothGattCharacteristic(),new BluetoothGattCharacteristic());}
 boolean canCaptureFrame(){return true;}boolean canConfigure(){return true;}
 void clearQueuedOutputRead(){queuedOutputRead=-1;queuedOutputOwner=null;}
 void beginOutputRead(int i){outputReads++;cfgBusy=true;}
 void notifyStatus(String s){}
 void writeNow(){writePending=true;sent.add((int)pendingPacket[1]);}
 void ack(){writePending=false;pendingPacket=null;pumpQueue();}
 static void check(boolean b,String m){if(!b)throw new AssertionError(m);}
 // METHODS
 public static void main(String[] args){
 if(args.length>0){
  QueueHarness q=new QueueHarness();q.captureFrame(0);q.enqueueWrite(new byte[]{1,5});
  byte[] rgb=CaptureHarness.rgb(522);
  while(q.frameCapture.busy()){
   byte[] request=q.gatt.last;Handler.advance(10);q.frameCapture.written(q.gatt,0);
   if(!q.frameCapture.busy())break;
   byte[] response=request[1]==1?CaptureHarness.header(522,rgb):
    CaptureHarness.chunk(rgb,(request[7]&255)|((request[8]&255)<<8),request[9]&255);
   Handler.advance(10);q.frameCapture.read(q.gatt,response,0);
  }
  check(q.sent.equals(Arrays.asList(5)),"FX eventually dispatched");
  System.out.println(Handler.now);return;
 }
 QueueHarness q=new QueueHarness();q.captureFrame(0);
 QueueHarness first=q;
 check(q.deferBehindFrame(()->first.actions++),"user action admitted");
 check(!q.captureFrame(0),"no recapture before command");
 check(!q.deferBehindFrame(()->first.actions+=100),"second command cannot overwrite first");
 check(q.actions==0 && q.frameCapture.busy(),"must drain current ATT");
 q.frameCapture.written(q.gatt,0);
 check(q.actions==1 && !q.frameCapture.busy() && q.gatt.reads==0,"action after exactly one ATT callback");
 q=new QueueHarness();q.captureFrame(0);QueueHarness second=q;
 q.deferBehindFrame(()->second.actions++);
 q.enqueueWrite(new byte[]{1,3});q.enqueueWrite(new byte[]{1,5});
 q.frameCapture.written(q.gatt,0);
 check(q.sent.equals(Arrays.asList(3)) && q.actions==0,"STOP must be first");
 q.ack();check(q.sent.equals(Arrays.asList(3,5)) && q.actions==0,"existing STOP successor order");
 q.ack();check(q.actions==1,"deferred command drains after STOP/FX");
 q=new QueueHarness();q.captureFrame(0);q.enqueueWrite(new byte[]{1,5});
 check(q.sent.isEmpty(),"FX cannot overlap ATT");q.frameCapture.written(q.gatt,0);
 check(q.sent.equals(Arrays.asList(5)) && q.gatt.reads==0,"FX preempts remaining capture");
 q=new QueueHarness();q.captureFrame(0);QueueHarness old=q;q.deferBehindFrame(()->old.actions++);
 q.gatt=new BluetoothGatt();q.frameCapture.cancel();q.pumpQueue();check(q.actions==0,"no deferred action on another session");
 q=new QueueHarness();q.captureFrame(0);q.queuedOutputRead=0;q.queuedOutputOwner=q.gatt;
 q.frameCapture.abortForStop();q.frameCapture.written(q.gatt,0);check(q.outputReads==1 && q.cfgBusy,"output read after drain");
 System.err.println("PASS: actual extracted admission/queue methods: bounded command, STOP, FX, read, session and ATT serialization");
 }
}
'''

def method(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace+1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

def main():
    report = {'baseline': BASE, 'core': CORE,
              'candidate_commit': run(['git','rev-parse','HEAD'],text=True).strip(),
              'tracked_android_dirty': bool(run(['git','diff','--name-only','HEAD','--','android'],text=True).strip()),
              'scope': 'Host tests; fake ATT=10ms/operation. No phone/radio FPS claims.'}
    with tempfile.TemporaryDirectory(prefix='led-preview-') as tmp:
        root = Path(tmp)
        for name, value in STUBS.items():
            put(root, name, value)
        codec = (SOURCE / 'BleConfigCodec.java').read_text()
        start = codec.index('    static int crc16(')
        end = codec.index('    static byte[] encode', start)
        put(root, 'com/opentail/android/BleConfigCodec.java',
            'package com.opentail.android;class BleConfigCodec {\n'+codec[start:end]+'}')
        put(root, 'com/opentail/android/CaptureHarness.java', HARNESS)
        capture = root / 'com/opentail/android/CoreFrameCapture.java'
        for label, source in [('before', git_file(BASE, str((SOURCE/'CoreFrameCapture.java').relative_to(ROOT)))),
                              ('after', (SOURCE/'CoreFrameCapture.java').read_bytes())]:
            capture.write_bytes(source)
            classes = root / label
            run([JDK/'javac', '-d', classes, *root.rglob('*.java')])
            rows = run([JDK/'java', '-cp', classes, 'com.opentail.android.CaptureHarness'], text=True)
            report[label] = [json.loads(row) for row in rows.splitlines()]
            if label == 'after':
                assert all(r['pending_timers'] == 0 and r['peak_timers'] == 1
                           for r in report[label]), 'Snapshot timers must be bounded'
        # Compile the independent, exact pinned renderer, never a copied formula oracle.
        for name in ['wled_fx_compat.c', 'wled_fx_compat.h']:
            (root/name).write_bytes(git_file(CORE, 'core-firmware/main/'+name))
        put(root, 'driver.c', C_DRIVER)
        run(['gcc', '-O2', '-I', root, root/'driver.c', root/'wled_fx_compat.c', '-lm', '-o', root/'oracle'])
        expected = run([root/'oracle'])
        put(root, 'com/opentail/android/OfflineFxSketch.java', (SOURCE/'OfflineFxSketch.java').read_text())
        put(root, 'com/opentail/android/OfflineHarness.java', JAVA_DRIVER)
        run([JDK/'javac', '-d', root/'offline', *root.rglob('*.java')])
        result = subprocess.run([str(JDK/'java'), '-cp', str(root/'offline'),
                                 'com.opentail.android.OfflineHarness'], capture_output=True, check=True)
        assert expected == result.stdout, 'Offline RGB diverges from pinned CORE'
        report['differential'] = {'frames': 5*3*3*3*5*11, 'rgb_bytes': len(expected),
                                  'result': 'byte-exact PASS',
                                  'host_render': json.loads(result.stderr)}
        ble = (SOURCE/'BleDiscovery.java').read_text()
        methods = '\n'.join(method(ble, signature) for signature in [
            'private boolean deferBehindFrame(', 'private void clearPreviewCommand(',
            'boolean captureFrame(', 'private static boolean isStopPacket(',
            'private void enqueueWrite(', 'private void pumpQueue('])
        put(root, 'com/opentail/android/QueueHarness.java',
            QUEUE_HARNESS.replace('// METHODS', methods))
        run([JDK/'javac', '-d', root/'queue', *root.rglob('*.java')])
        run([JDK/'java', '-cp', root/'queue', 'com.opentail.android.QueueHarness'])
        report['queue_regressions'] = 'PASS (extracted production methods, fake ATT)'
        candidate_queue = QUEUE_HARNESS.replace('// METHODS', methods)
        baseline_ble = git_file(BASE, str((SOURCE/'BleDiscovery.java').relative_to(ROOT))).decode()
        for label, capture_source, enqueue in [
            ('before', git_file(BASE, str((SOURCE/'CoreFrameCapture.java').relative_to(ROOT))),
             method(baseline_ble, 'private void enqueueWrite(')),
            ('after', (SOURCE/'CoreFrameCapture.java').read_bytes(),
             method(ble, 'private void enqueueWrite('))]:
            capture.write_bytes(capture_source)
            put(root, 'com/opentail/android/QueueHarness.java', candidate_queue.replace(
                method(ble, 'private void enqueueWrite('), enqueue))
            run([JDK/'javac', '-d', root/'latency', *root.rglob('*.java')])
            latency = run([JDK/'java', '-cp', root/'latency',
                           'com.opentail.android.QueueHarness', 'latency'], text=True)
            report[label+'_fx_dispatch_simulated_ms'] = int(latency)
        assert report['after_fx_dispatch_simulated_ms'] == 10
        # Protect all UI outside the scoped preview and its state revision.
        preview_path = 'android/app/src/main/java/com/opentail/android/ComposeMainActivity.kt'
        original = git_file(BASE, preview_path).decode()
        current = (ROOT/preview_path).read_text()
        def outside_preview(text):
            begin = text.index('    @Composable\n    private fun EffectsPreview(')
            end = text.index('    @Composable\n    private fun ShowModesPage(', begin)
            return text[:begin]+text[end:]
        assert outside_preview(current).replace('private var frameRevision by mutableLongStateOf(0L)',
                                                'private var frameRevision = 0L') == outside_preview(original)
        editor_path = 'android/app/src/main/java/com/opentail/android/ApprovedWledEffectsEditor.kt'
        assert (ROOT/editor_path).read_bytes() == git_file(BASE, editor_path)
        for token in ['val columns = 30', 'val gap=3.dp.toPx()',
                      'val capsuleWidth=(size.width-gap*(columns-1))/columns',
                      'val capsuleHeight = 24.dp.toPx()',
                      'topLeft = Offset(col * pitch, 0f)',
                      'cornerRadius = androidx.compose.ui.geometry.CornerRadius(capsuleWidth / 2f)']:
            assert token in original and token in current
        report['ui_source_invariants'] = 'PASS: other UI/editor unchanged; capsule geometry preserved'
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()

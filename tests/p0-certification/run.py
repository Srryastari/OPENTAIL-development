#!/usr/bin/env python3
"""Run against explicit firmware/Android trees; retain logs and exact source IDs."""
import argparse, json, pathlib, shutil, subprocess, sys
p=argparse.ArgumentParser(); p.add_argument('--firmware', required=True); p.add_argument('--android', required=True); p.add_argument('--output', required=True)
a=p.parse_args(); fw=pathlib.Path(a.firmware).resolve(); android=pathlib.Path(a.android).resolve(); out=pathlib.Path(a.output).resolve(); out.mkdir(parents=True,exist_ok=True)
suite=pathlib.Path(__file__).resolve().parent
report={'scope':'host software only; no BLE radio, RTOS, NVS hardware, RMT or optical certification','checks':[]}
for label,root in [('firmware',fw),('android',android)]:
    report[label]={'sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'status':subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True)}
def run(name,cmd,cwd):
    with (out/(name+'.log')).open('w') as log:
        result=subprocess.run(cmd,cwd=cwd,stdout=log,stderr=subprocess.STDOUT)
    report['checks'].append({'name':name,'result':'PASS' if result.returncode==0 else 'FAIL','exit_code':result.returncode})
    return result.returncode==0
sources=['config_journal','panel_zone','panel_zone_pair_store','trigger_manager','output_manager','core_logic','arbitration_logic']
# The harness includes the baseline fake adapter by relative path, so keep suite
# in the firmware worktree; refuse an accidental different source tree.
if suite.parent.parent != fw: p.error('suite must reside in the selected firmware worktree')
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-I'+str(fw/'core-firmware/main')]+[str(fw/'core-firmware/main'/ (s+'.c')) for s in sources]+[str(suite/'test_integration.c'),'-o',str(out/'integration')]
if run('compile-integration',cmd,fw): run('integration',[str(out/'integration'),str(out)],fw)
run('firmware-host',['sh','core-firmware/tests/run-host-tests.sh'],fw)
if shutil.which('java'):
    run('android-wire',['python3',str(suite/'wire_test.py'),str(android),str(out)],fw)
else:
    report['checks'].append({'name':'android-wire','result':'BLOCKED','reason':'Java runtime/compiler unavailable'})
report['certified']=False
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
sys.exit(1 if any(c['result']!='PASS' for c in report['checks']) else 0)

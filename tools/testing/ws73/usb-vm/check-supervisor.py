#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
import argparse,copy,datetime,hashlib,importlib.util,json,pathlib,re,sys
sys.dont_write_bytecode=True
if not __debug__:raise RuntimeError("Run without Python optimization")
parser=argparse.ArgumentParser(description="Verify rejection controls against a fresh full-speed PASS record")
parser.add_argument("record",type=pathlib.Path)
args=parser.parse_args()
root=pathlib.Path(__file__).resolve().parents[4].parent/'.dev/ws73-upstream'
runner=pathlib.Path(__file__).with_name('run.py')
spec=importlib.util.spec_from_file_location('ws73_virtual',runner);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
p=args.record.resolve()
proof=json.loads((p/'manifest.json').read_text())
assert proof['status']=='PASS' and proof['speed']=='full'
for name in ['kernel.log','fixture.log']:
 assert hashlib.sha256((p/name).read_bytes()).hexdigest()==proof['materials'][name]
k=(p/'kernel.log').read_text();s=(p/'fixture.log').read_text();m.validate(k,s)
builddir=pathlib.Path(proof['build_record'])
build=json.loads((builddir/'manifest.json').read_text())
m.validate_build(builddir,build,proof['compiled_source'])
artifact_trials={}
for name in ['stale_source','failed_build','missing_artifact','extra_artifact','wrong_hash']:
 changed=copy.deepcopy(build)
 if name=='stale_source':changed['source']='0'*40
 elif name=='failed_build':changed['status']='FAIL'
 elif name=='missing_artifact':del changed['artifacts']['drivers/misc/ws73/usb.o']
 elif name=='extra_artifact':changed['artifacts']['unexpected']='0'*64
 else:changed['artifacts']['.config']='0'*64
 try:m.validate_build(builddir,changed,proof['compiled_source'])
 except (AssertionError,ValueError):artifact_trials[name]='REJECTED'
 else:raise AssertionError('accepted '+name)

trials={
 'missing_overall':(k,s.replace('WS73_VIRTUAL_USB_PASS','')),
 'duplicate_overall':(k,s+'\nWS73_VIRTUAL_USB_PASS\n'),
 'missing_case':(k,s.replace('WS73_TEST_CASE 10 PASS','')),
 'short_tx':(k,s.replace('actual=128 verified','actual=127 verified',1)),
 'extra_tx':(k,s.replace('WS73_TEST_CASE 0 PASS','WS73_TEST_OUT step=3 actual=128 verified\nWS73_TEST_CASE 0 PASS')),
 'missing_rx':(k,s.replace('WS73_TEST_IN kind=chip actual=124 synthetic','',1)),
 'wrong_rx_size':(k,s.replace('kind=chip actual=124','kind=chip actual=123',1)),
 'missing_disconnect':(k,s.replace('USB disconnect','USB missing',1)),
 'kasan':(k+'\nBUG: KASAN',s),
 'ubsan':(k+'\nUBSAN:',s),
 'lockdep':(k+'\npossible circular locking dependency',s),
 'wrong_identity':(k,s.replace('package 2 bus 0','package 1 bus 1',1)),
 'wrong_removal_errno':(k,re.sub(r'observation failed: -(19|108|71)','observation failed: -5',s)),
 'missing_kmsg':(k,s.replace('WS73_TEST_KMSG','WS73_MISSING_KMSG',1)),
 'missing_version_tx':(k,s.replace('WS73_TEST_OUT step=3 actual=128 verified','',1)),
 'wrong_version':(k,s.replace('firmware 0x010a6e7f','firmware 0x010a6e00',1)),
 'wrong_reply_role':(k,s.replace('kind=version','kind=chip',1)),
 'missing_stale_reply':(k,s.replace('WS73_TEST_IN kind=stale actual=124 synthetic','',1)),
 'wrong_speed':(k.replace('new full-speed','new high-speed',1),s),
 'wrong_notification':(k,s.replace('notification observation 2;','notification observation 4;',1)),
 'missing_notification':(k,s.replace('function notification observation 3;','ignored observation 3;',1)),
 'duplicate_notification':(k,s.replace('WS73_TEST_CASE 8 PASS','WS73_TEST_KMSG function notification observation 0; no request completion\nWS73_TEST_CASE 8 PASS')),
 'partial_publication':(k,s.replace('WS73_TEST_CASE 10 PASS','WS73_TEST_KMSG function notification observation 0; no request completion\nWS73_TEST_CASE 10 PASS')),
 'post_drain_callback':(k,s.replace('WS73_TEST_CASE 16 PASS','WS73_TEST_KMSG interrupt observation failed: -5\nWS73_TEST_CASE 16 PASS')),
 'missing_drain':(k,s.replace('device callbacks drained','device callbacks missing',1)),
 'missing_remove_marker':(k,s.replace('WS73_TEST_REMOVE','WS73_MISSING_REMOVE',1)),
 'wrong_interrupt_pool':(k,s.replace('pool 0xabcdef12','pool 0x00000000',1)),
 'wrong_interrupt_bits':(k,s.replace('bits=00040001','bits=00000001',1)),
 'missing_interrupt':(k,s.replace('WS73_TEST_INT kind=event actual=7 bits=00000041 synthetic','',1)),
 'false_bsp_on_panic':(k,s.replace('WS73_TEST_CASE 13 PASS','WS73_TEST_KMSG BSP notification observation; pool 0xabcdef12 is not a TX credit\nWS73_TEST_CASE 13 PASS')),
 'missing_interrupt_fault':(k,s.replace('interrupt observation failed: -105','interrupt observation failed: -5',1)),
 'missing_unsupported':(k,s.replace('WS73_TEST_IN kind=unsupported actual=156 synthetic','')),
}
rejected=[]
for name,(a,b) in trials.items():
 assert (a,b)!=(k,s),name
 try:m.validate(a,b)
 except (AssertionError, ValueError):rejected.append(name)
 else:raise AssertionError('accepted '+name)
out=root/'records'/('virtual-usb-negative-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ'))
out.mkdir(parents=True)
(out/'check-supervisor.py').write_bytes(pathlib.Path(__file__).read_bytes())
result={'status':'PASS','runner_sha256':hashlib.sha256(runner.read_bytes()).hexdigest(),'checker_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'positive_record':str(p),'negative_rejected':rejected,'artifact_rejected':artifact_trials,'scope':'text supervisor rejection checks only'}
(out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(out);print('PASS',len(rejected),'log controls and',len(artifact_trials),'artifact controls')

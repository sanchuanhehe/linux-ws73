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
k=(p/'kernel.log').read_text();s=(p/'fixture.log').read_text();validator=m.validate_hci if proof.get('phase')=='hci' else m.validate_startup if proof.get('phase')=='startup' else m.validate_loader if proof.get('phase')=='loader' else m.validate_download if proof.get('phase')=='download' else m.validate_boot if proof.get('phase')=='boot' else m.validate;validator(k,s)
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


if proof.get('phase')=='loader':
 m.validate_loader_inputs(p,proof)
 for name in ['missing_input','wrong_input_hash','wrong_provenance','wrong_mutant_hash']:
  changed=copy.deepcopy(proof)
  if name=='missing_input':del changed['firmware_inputs']['firmware/us/btc_cali.bin']
  elif name=='wrong_input_hash':changed['materials']['firmware/us/ws73.bin']='0'*64
  elif name=='wrong_provenance':changed['firmware_inputs']['firmware/e/ws73.bin']['sha256']='0'*64
  else:changed['materials']['firmware/rehashed-ws73.bin']='0'*64
  try:m.validate_loader_inputs(p,changed)
  except (AssertionError,ValueError,KeyError):artifact_trials[name]='REJECTED'
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
if proof.get('phase')=='boot':
 trials={
  'missing_overall':(k,s.replace('WS73_VIRTUAL_USB_PASS','')),
  'duplicate_overall':(k,s+'\nWS73_VIRTUAL_USB_PASS\n'),
  'missing_case':(k,s.replace('WS73_BOOT_CASE 10 PASS','')),
  'short_command':(k,s.replace('actual=19 READM','actual=18 READM',1)),
  'missing_version_query':(k,s.replace('WS73_BOOT_VERSION_OUT actual=8 verified','',1)),
  'short_version_query':(k,s.replace('WS73_BOOT_VERSION_OUT actual=8','WS73_BOOT_VERSION_OUT actual=7',1)),
  'missing_version_reply':(k,s.replace('WS73_BOOT_VERSION_IN actual=3 synthetic','',1)),
  'wrong_version':(k,s.replace('boot version observation: 3 bytes 563100','boot version observation: 3 bytes 563200',1)),
  'missing_kernel_version':(k.replace('boot version observation: 3 bytes 563100','missing version',1),s),
  'wrong_command':(k,s.replace('READM_40019000_4','READM_40019004_4',1)),
  'missing_reply':(k,s.replace('WS73_BOOT_IN actual=4 word=00000000 synthetic','',1)),
  'wrong_reply_size':(k,s.replace('actual=4 word=00000000','actual=3 word=00000000',1)),
  'wrong_zero_identity':(k,s.replace('package 0 bus 0','package 2 bus 0',1)),
  'wrong_e_bus':(k,s.replace('package 2 bus 2','package 2 bus 0',1)),
  'short_reply_accepted':(k,s.replace('boot chip observation failed: -90','boot chip observation 0x00000000: package 0 bus 0; cold-read qualification pending',1)),
  'wrong_timeout':(k,s.replace('boot chip observation failed: -110','boot chip observation failed: -90',1)),
  'missing_withhold':(k,s.replace('WS73_BOOT_WITHHOLD_REPLY','')),
  'missing_no_extra_out':(k,s.replace('WS73_BOOT_NO_EXTRA_OUT','',1)),
  'extra_out':(k,s.replace('WS73_BOOT_CASE 0 PASS','WS73_BOOT_OUT actual=19 READM_40019000_4 verified\nWS73_BOOT_CASE 0 PASS')),
  'missing_remove':(k,s.replace('WS73_TEST_REMOVE','',1)),
  'missing_drain':(k,s.replace('device callbacks drained','missing drain',1)),
  'post_drain':(k,s.replace('WS73_BOOT_CASE 0 PASS','WS73_TEST_KMSG boot chip observation failed: -19\nWS73_BOOT_CASE 0 PASS')),
  'missing_kernel_observation':(k.replace('boot chip observation','missing observation',1),s),
  'wrong_speed':(k.replace('new full-speed','new high-speed',1),s),
  'kasan':(k+'\nBUG: KASAN',s),
  'ubsan':(k+'\nUBSAN:',s),
  'lockdep':(k+'\npossible circular locking dependency',s),
 }


if proof.get('phase')=='download':
 trials={
  'missing_overall':(k,s.replace('WS73_VIRTUAL_USB_PASS','')),
  'duplicate_overall':(k,s+'\nWS73_VIRTUAL_USB_PASS\n'),
  'missing_case':(k,s.replace('WS73_DOWNLOAD_CASE 3 PASS','')),
  'wrong_speed':(k.replace('new full-speed','new high-speed',1),s),
  'short_command':(k,s.replace('chunk=0 actual=28 verified','chunk=0 actual=27 verified',1)),
  'short_payload':(k,s.replace('actual=32768 delay_us','actual=32767 delay_us',1)),
  'missing_payload':(k,s.replace('WS73_DOWNLOAD_PAYLOAD','MISSING_PAYLOAD',1)),
  'wrong_packet_size':(k,s.replace('WS73_DOWNLOAD_PACKET_SIZE 64','WS73_DOWNLOAD_PACKET_SIZE 512',1)),
  'short_helper_wait':(re.sub(r'synthetic READY wait ns=\d+','synthetic READY wait ns=4999999',k,count=1),s),
  'missing_helper_wait':(k.replace('synthetic READY wait ns=','missing wait ns=',1),s),
  'short_ready_wait':(k,re.sub(r'delay_us=\d+','delay_us=4999',s,count=1)),
  'bad_ready_accepted':(k,s.replace('result=-71 stage=2','result=0 stage=2',1)),
  'bad_files_ok_accepted':(k,s.replace('result=-71 stage=4','result=0 stage=4',1)),
  'wrong_timeout':(k,s.replace('result=-110','result=-90',1)),
  'missing_withhold':(k,s.replace('WS73_DOWNLOAD_WITHHOLD_READY','')),
  'wrong_progress':(k,s.replace('image=2 ack=32','image=2 ack=31',1)),
  'false_ready':(k,s.replace('result=0 stage=6','result=0 stage=0',1)),
  'missing_quit':(k,s.replace('WS73_DOWNLOAD_QUIT actual=5 verified','',1)),
  'extra_quit':(k,s.replace('WS73_DOWNLOAD_CASE 1 PASS','WS73_DOWNLOAD_QUIT actual=5 verified\nWS73_DOWNLOAD_CASE 1 PASS')),
  'wrong_reply':(k,s.replace('FILES NO actual=8','FILES OK actual=8',1)),
  'missing_quiet':(k,s.replace('WS73_DOWNLOAD_NO_EXTRA_OUT','',1)),
  'missing_remove':(k,s.replace('WS73_TEST_REMOVE','',1)),
  'missing_drain':(k,s.replace('synthetic download callbacks drained','missing drain',1)),
  'post_drain_result':(k,s.replace('WS73_DOWNLOAD_CASE 0 PASS','WS73_TEST_KMSG synthetic download result=0 stage=6 image=2 ack=32\nWS73_DOWNLOAD_CASE 0 PASS')),
  'missing_kernel_result':(k.replace('synthetic download result=','missing result=',1),s),
  'kasan':(k+'\nBUG: KASAN',s),
  'ubsan':(k+'\nUBSAN:',s),
  'lockdep':(k+'\npossible circular locking dependency',s),
 }


if proof.get('phase')=='loader':
 trials={
  'missing_overall':(k,s.replace('WS73_VIRTUAL_USB_PASS','')),
  'duplicate_overall':(k,s+'\nWS73_VIRTUAL_USB_PASS\n'),
  'missing_case':(k,s.replace('WS73_LOADER_CASE 9 PASS','')),
  'wrong_speed':(k.replace('new full-speed','new high-speed',1),s),
  'short_identity':(k,s.replace('READM actual=19','READM actual=18',1)),
  'wrong_identity':(k,s.replace('word=0000000a','word=00000000',1)),
  'wrong_identity_result':(k,s.replace('raw=0000000a','raw=00000000',1)),
  'missing_preparation':(k,s.replace('WS73_LOADER_WRITEM step=10','MISSING_WRITEM step=10',1)),
  'missing_ready_echo':(k,re.sub(r'WS73_LOADER_READY_ECHO[^\n]*\n','',s,count=1)),
  'duplicate_ready_echo':(k,s+'\n'+re.search(r'WS73_LOADER_READY_ECHO[^\n]*',s).group(0)+'\n'),
  'wrong_ready_echo_length':(k,s.replace('WS73_LOADER_READY_ECHO actual=42','WS73_LOADER_READY_ECHO actual=41',1)),
  'wrong_ready_echo_tag':(k,s.replace('52454144593a46494c4530313a','52454144593a46494c4530323a',1)),
  'missing_files_echo':(k,re.sub(r'WS73_LOADER_FILES_ECHO[^\n]*\n','',s,count=1)),
  'duplicate_files_echo':(k,s+'\n'+re.search(r'WS73_LOADER_FILES_ECHO[^\n]*',s).group(0)+'\n'),
  'wrong_files_echo_length':(k,s.replace('WS73_LOADER_FILES_ECHO actual=38','WS73_LOADER_FILES_ECHO actual=37',1)),
  'wrong_files_echo_command':(k,s.replace('46494c4553204f4b3a46494c45532031203078343030303030','46494c4553204f4b3a46494c45532031203078343030303031',1)),
  'missing_echo':(k,re.sub(r'WS73_LOADER_ECHO[^\n]*\n','',s,count=1)),
  'duplicate_echo':(k,s+'\n'+re.search(r'WS73_LOADER_ECHO[^\n]*',s).group(0)+'\n'),
  'wrong_echo_length':(k,s.replace('WS73_LOADER_ECHO actual=34','WS73_LOADER_ECHO actual=33',1)),
  'wrong_echo_bytes':(k,s.replace('57524954454d204f4b3a','57524954454d204f4b3b',1)),
  'wrong_write_length':(k,s.replace('step=0 actual=23','step=0 actual=22',1)),
  'wrong_chunk':(k,s.replace('offset=32768 size=32768','offset=32769 size=32768',1)),
  'wrong_flag':(k,s.replace('size=32768 flag=1','size=32768 flag=3',1)),
  'missing_payload':(k,s.replace('WS73_LOADER_PAYLOAD','MISSING_PAYLOAD',1)),
  'short_payload':(k,s.replace('actual=32768 original','actual=32767 original',1)),
  'missing_files_ack':(k,s.replace('WS73_LOADER_FILES_OK actual=8','',1)),
  'missing_quit':(k,s.replace('WS73_LOADER_QUIT actual=5 verified','',1)),
  'extra_quit':(k,s.replace('WS73_LOADER_CASE 2 PASS','WS73_LOADER_QUIT actual=5 verified\nWS73_LOADER_CASE 2 PASS')),
  'missing_file_accepted':(k,s.replace('result=-2 repeat=-2','result=0 repeat=0',1)),
  'rehashed_modification_accepted':(k,s.replace('result=-129 repeat=-129','result=0 repeat=0',1)),
  'mixed_profile_accepted':(k,s.replace('result=-90 repeat=-90','result=0 repeat=0',1)),
  'unsupported_accepted':(k,s.replace('result=-95 repeat=-95','result=0 repeat=0',1)),
  'failed_session_retried':(k,s.replace('repeat=-129','repeat=0',1)),
  'completed_session_retried':(k,s.replace('repeat=-114','repeat=0',1)),
  'false_ready':(k,s.replace('phase=4 stage=6','phase=4 stage=0',1)),
  'wrong_preparation_count':(k,s.replace('prep=11','prep=10',1)),
  'wrong_progress':(k,s.replace('ack=34056','ack=7708',1)),
  'missing_quiet':(k,s.replace('WS73_LOADER_NO_EXTRA_OUT','',1)),
  'missing_remove':(k,s.replace('WS73_TEST_REMOVE','',1)),
  'missing_drain':(k,s.replace('synthetic download callbacks drained','missing drain',1)),
  'missing_kernel_result':(k.replace('synthetic loader result=','missing result=',1),s),
  'short_helper_wait':(re.sub(r'synthetic READY wait ns=\d+','synthetic READY wait ns=4999999',k,count=1),s),
  'kasan':(k+'\nBUG: KASAN',s),
  'ubsan':(k+'\nUBSAN:',s),
  'lockdep':(k+'\npossible circular locking dependency',s),
 }

if proof.get('phase')=='startup':
 trials={
  'missing_platform':(k,s.replace('WS73_STARTUP_PLATFORM sequence=4 actual=128 sleep=0 verified','',1)),
  'short_platform':(k,s.replace('sequence=4 actual=128 sleep=0','sequence=4 actual=127 sleep=0',1)),
  'late_platform':(k,s.replace('WS73_STARTUP_PLATFORM sequence=4 actual=128 sleep=0 verified','',1).replace('WS73_STARTUP_OUT step=7 actual=864 verified','WS73_STARTUP_OUT step=7 actual=864 verified\nWS73_STARTUP_PLATFORM sequence=4 actual=128 sleep=0 verified',1)),
  'missing_overall':(k,s.replace('WS73_VIRTUAL_USB_PASS','')),
  'duplicate_overall':(k,s+'\nWS73_VIRTUAL_USB_PASS\n'),
  'missing_case':(k,s.replace('WS73_STARTUP_CASE 4 PASS','')),
  'short_customization':(k,s.replace('step=7 actual=864','step=7 actual=863',1)),
  'missing_queue_switch':(k,s.replace('WS73_STARTUP_OUT step=5 actual=36 verified','',1)),
  'wrong_action':(k,s.replace('message=25','message=26',1)),
  'short_action':(k,s.replace('message=25 actual=4','message=25 actual=3',1)),
  'wrong_setup_direction':(k,s.replace('2100000000000400','a100000000000400',1)),
  'missing_stall':(k,s.replace('WS73_STARTUP_CONTROL_STALL','')),
  'missing_withhold':(k,s.replace('WS73_STARTUP_WITHHOLD_ACTION_REPLY','')),
  'missing_close_reply':(k,s.replace('WS73_STARTUP_NOTIFY kind=3 actual=108 malformed=0 synthetic','',1)),
  'missing_malformed_reply':(k,s.replace('kind=1 actual=108 malformed=1','kind=1 actual=108 malformed=0',1)),
  'false_disabled':(k,s.replace('stage=disable state=6 action=4','stage=disable state=6 action=3',1)),
  'timeout_accepted':(k,s.replace('result=-110','result=0',1)),
  'missing_kernel_result':(k.replace('synthetic startup result=','missing result=',1),s),
  'missing_quiet':(k,s.replace('WS73_STARTUP_NO_EXTRA_OUT','',1)),
  'missing_drain':(k,s.replace('device callbacks drained','missing drain',1)),
  'post_drain':(k,s.replace('WS73_STARTUP_CASE 0 PASS','WS73_TEST_KMSG function notification observation 0; delivered to armed request\nWS73_STARTUP_CASE 0 PASS')),
  'missing_remove':(k,s.replace('WS73_TEST_REMOVE','',1)),
  'wrong_identity':(k,s.replace('WS73_TEST_IN kind=chip actual=124 synthetic','',1)),
  'panic_loses_priority':(k,s.replace('bits=00000041','bits=00000001',1)),
  'wrong_speed':(k.replace('new full-speed','new high-speed',1),s),
  'kasan':(k+'\nBUG: KASAN',s),
  'ubsan':(k+'\nUBSAN:',s),
  'lockdep':(k+'\npossible circular locking dependency',s),
  'skip':(k,s+'\nSKIP\n'),
 }

if proof.get('phase')=='hci':
 pending=re.search(r'WS73_HCI_CASE 1 BEGIN(.*?)WS73_HCI_CASE 1 PASS',s,re.S)[1]
 detached=int(re.search(r'WS73_TEST_KMSG \d+,\d+,(\d+),[^;]*;[^\n]*USB disconnect',pending)[1])
 late=re.sub(r'(WS73_TEST_KMSG \d+,\d+,)\d+(,[^;]*;[^\n]*device callbacks drained)',lambda m:m[1]+str(detached+1000000)+m[2],pending)
 trials={
  'missing_platform':(k,s.replace('WS73_STARTUP_PLATFORM sequence=4 actual=128 sleep=0 verified','',1)),
  'short_platform':(k,s.replace('sequence=4 actual=128 sleep=0','sequence=4 actual=127 sleep=0',1)),
  'late_platform':(k,s.replace('WS73_STARTUP_PLATFORM sequence=4 actual=128 sleep=0 verified','',1).replace('WS73_STARTUP_OUT step=7 actual=864 verified','WS73_STARTUP_OUT step=7 actual=864 verified\nWS73_STARTUP_PLATFORM sequence=4 actual=128 sleep=0 verified',1)),
  'late_reset_drain':(k,s.replace(pending,late)),
  'hci_command_timeout':(k+'\nBluetooth: hci0: command 0x0c03 tx timeout\n',s),
  'missing_overall':(k,s.replace('WS73_VIRTUAL_USB_PASS','')),
  'duplicate_overall':(k,s+'\nWS73_VIRTUAL_USB_PASS\n'),
  'missing_case':(k,s.replace('WS73_HCI_CASE 1 PASS','')),
  'missing_auxiliary':(k,s.replace('synthetic auxiliary result=0','missing auxiliary',1)),
  'missing_kernel_auxiliary':(k.replace('synthetic auxiliary result=0','missing auxiliary',1),s),
  'wrong_action':(k,s.replace('message=25','message=26',1)),
  'short_action':(k,s.replace('message=25 actual=4','message=25 actual=3',1)),
  'wrong_setup':(k,s.replace('2100000000000400','a100000000000400',1)),
  'wrong_queue':(k,s.replace('WS73_HCI_QUEUE step=8','WS73_HCI_QUEUE step=7',1)),
  'wrong_opcode':(k,s.replace('sequence=10 opcode=1003','sequence=10 opcode=1009',1)),
  'wrong_sequence':(k,s.replace('sequence=10 opcode=1003','sequence=11 opcode=1003',1)),
  'missing_reply':(k,s.replace('WS73_HCI_COMPLETE step=0 opcode=1003 h4=15 synthetic','',1)),
  'missing_fragment':(k,s.replace('WS73_HCI_FRAGMENT h4=1 actual=100 synthetic','',1)),
  'wrong_fragment_size':(k,s.replace('h4=1 actual=100','h4=1 actual=99',1)),
  'wrong_management_address':(k,s.replace('address=112233445566','address=000000000000',1)),
  'short_management_info':(k,s.replace('INFO actual=289','INFO actual=288',1)),
  'missing_management_index':(k,s.replace('WS73_HCI_MGMT_INDEX index=0 configured','',1)),
  'missing_management_info':(k,s.replace('WS73_HCI_MGMT_INFO','MISSING_INFO',1)),
  'missing_auto_off':(k,s.replace('WS73_HCI_AUTO_OFF_ACK','',1)),
  'missing_withhold':(k,s.replace('WS73_HCI_WITHHOLD_RESET','')),
  'missing_quiet':(k,s.replace('WS73_STARTUP_NO_EXTRA_OUT','',1)),
  'missing_drain':(k,s.replace('device callbacks drained','missing drain',1)),
  'post_drain':(k,s.replace('WS73_HCI_CASE 0 PASS','WS73_TEST_KMSG function notification observation 0; delivered to armed request\nWS73_HCI_CASE 0 PASS')),
  'missing_remove':(k,s.replace('WS73_TEST_REMOVE','',1)),
  'wrong_speed':(k.replace('new full-speed','new high-speed',1),s),
  'kasan':(k+'\nBUG: KASAN',s),
  'ubsan':(k+'\nUBSAN:',s),
  'lockdep':(k+'\npossible circular locking dependency',s),
  'skip':(k,s+'\nSKIP\n'),
 }

rejected=[]
for name,(a,b) in trials.items():
 assert (a,b)!=(k,s),name
 try:validator(a,b)
 except (AssertionError, ValueError):rejected.append(name)
 else:raise AssertionError('accepted '+name)
out=root/'records'/('virtual-usb-negative-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ'))
out.mkdir(parents=True)
(out/'check-supervisor.py').write_bytes(pathlib.Path(__file__).read_bytes())
result={'status':'PASS','runner_sha256':hashlib.sha256(runner.read_bytes()).hexdigest(),'checker_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'positive_record':str(p),'negative_rejected':rejected,'artifact_rejected':artifact_trials,'phase':proof.get('phase','runtime'),'scope':'log and build artifact supervisor rejection checks only'}
(out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(out);print('PASS',len(rejected),'log controls and',len(artifact_trials),'artifact controls')

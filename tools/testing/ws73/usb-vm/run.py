#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
import argparse,datetime,fcntl,gzip,hashlib,json,os,pathlib,re,shutil,stat,subprocess
TOOLS=pathlib.Path(__file__).resolve().parent
SOURCE=TOOLS.parents[3]
WORK=SOURCE.parent/'.dev/ws73-upstream'
if not __debug__: raise RuntimeError('Run without Python optimization; verification assertions are required')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validate_build(proofdir, proof, expected_head):
 required={'.config','arch/x86/boot/bzImage','vmlinux'}
 required.update('drivers/misc/ws73/'+name+'.o' for name in
                 ['usb','usb-io','usb-boot-test','usb-startup-test','boot','boot-loader','firmware','runtime','usb-tx','usb-wire','service',
                  'ble-control','customize','customize-request','startup','ble','ble-link'])
 assert proof['status']=='PASS','failed/incomplete build'
 assert proof['source']==expected_head,'stale source artifact'
 assert set(proof['artifacts'])==required,'missing/extra build artifact'
 for name,digest in proof['artifacts'].items():
  assert sha(proofdir/name)==digest,'artifact hash mismatch: '+name

def validate(kernel, log, speed="full"):
 bad=re.search(r'BUG:|WARNING:|UBSAN:|Kernel panic|possible circular|inconsistent lock state|sleeping function called|WS73_TEST_ERROR|WS73_VIRTUAL_USB_FAIL',kernel+'\n'+log)
 assert not bad, bad.group(0) if bad else ''
 assert len(re.findall(r"new "+speed+r"-speed USB device",kernel))==17,"speed/enumeration count"
 assert log.count('WS73_VIRTUAL_USB_PASS')==1,'missing/repeated overall PASS'
 parts=re.findall(r'WS73_TEST_CASE (\d+) BEGIN(.*?)WS73_TEST_CASE \1 PASS',log,re.S)
 assert [int(x[0]) for x in parts]==list(range(17)),'missing/reordered case'
 expected_messages=['runtime chip observation: package 2 bus 0; firmware compatibility unverified','runtime chip observation: package 0 bus 0; firmware compatibility unverified','runtime chip observation failed: -121','runtime chip observation failed: -90','runtime chip observation failed: -19','runtime version observation failed: -121','runtime version observation failed: -90','runtime chip observation: package 2 bus 0; firmware compatibility unverified']
 expected_messages += [expected_messages[0], 'runtime version observation failed: -90', 'runtime version observation failed: -71']
 expected_messages += [expected_messages[0], 'runtime version observation failed: -90', 'runtime version observation failed: -5', 'runtime version observation failed: -5', 'runtime version observation failed: -105', expected_messages[0]]
 removal_errors=0
 for number,part in parts:
  n=int(number); records=re.findall(r'WS73_TEST_KMSG ([^\r\n]*)',part)
  if n==4:assert sum(bool(re.search(r'runtime chip observation failed: -(19|108|71)$',line)) for line in records)==1,number
  else:assert sum(expected_messages[n] in line for line in records)==1,number
  assert sum('USB disconnect' in line for line in records)==1,number
  if n in [0,1,7,8,11,16]:assert sum('runtime version observation: firmware 0x010a6e7f ROM 0x12345678' in line for line in records)==1,number
  out=[('0','36'),('1','36'),('2','128')]
  incoming=[] if n==4 else [('chip','123' if n==3 else '124')]
  if n in [0,1,5,6,7,8,9,10,11,12,13,14,15,16]:
   out+=[('3','128')]
   if n==7:incoming+=[('stale','124')]
   if n==8:incoming += [('unsupported','156'),('notification','156'),('duplicate','156')]
   if n in [9,10]:incoming += [('notification','108' if n==9 else '124')]
   elif n not in [12,13,14,15]:incoming+=[('version','124')]
  interrupts=[]
  if n==11:interrupts=[('empty','8','00000000'),('event','8','80000819'),('duplicate','8','80000819')]
  if 12<=n<=16:interrupts=[('event','7' if n==12 else '8', '00040001' if n==14 else '00000021' if n==15 else '00000041')]
  assert re.findall(r'WS73_TEST_INT kind=(\w+) actual=(\d+) bits=([0-9a-f]+) synthetic',part)==interrupts,number
  bsp=[x for x in records if 'BSP notification observation;' in x]
  assert len(bsp)==(1 if n==11 else 0), 'BSP publication '+number
  if bsp:assert 'pool 0xabcdef12 is not a TX credit' in bsp[0],number
  errors=re.findall(r'interrupt observation failed: (-\d+)', '\n'.join(records))
  removal=[x for x in errors if x in ['-71','-108','-19']]
  assert len(removal)<=1, 'duplicate removal error '+number
  if removal:
   assert part.index('WS73_TEST_REMOVE') < part.index('interrupt observation failed: '+removal[0]), 'error before injected removal '+number
  removal_errors += len(removal)
  errors=[x for x in errors if x not in removal]
  assert sum('device callbacks drained' in x for x in records)==1, 'missing drain '+number
  assert 'device callbacks drained' in records[-1], 'post-drain callback '+number
  assert part.index('WS73_TEST_REMOVE') < part.index('device callbacks drained'), 'drain order '+number
  if removal:assert part.index('interrupt observation failed: '+removal[0]) < part.index('device callbacks drained'), 'post-drain callback '+number
  assert errors==(['-90' if n==12 else '-105' if n==15 else '-5'] if 12<=n<=16 else []), 'interrupt fault '+number
  notifications=re.findall(r'function notification observation (\d+); no request completion', '\n'.join(records))
  assert notifications==(['0','1','2','3'] if n==8 else []), 'notification publication '+number
  assert re.findall(r'WS73_TEST_OUT step=(\d) actual=(\d+) verified',part)==out,number
  assert re.findall(r'WS73_TEST_IN kind=(\w+) actual=(\d+) synthetic',part)==incoming,number
 assert len(re.findall(r'WS73_TEST_KMSG ',log))==67+removal_errors,'missing/extra captured kernel records'
 return {'cases':17,'out_transfers':65,'in_transfers':30,'interrupt_transfers':8,'disconnects':17,'drains':17,'removal_interrupt_errors':removal_errors}
def validate_startup(kernel, log, speed="full"):
 bad=re.search(r'BUG:|WARNING:|UBSAN:|Kernel panic|possible circular|inconsistent lock state|sleeping function called|WS73_TEST_ERROR|WS73_VIRTUAL_USB_FAIL|SKIP',kernel+'\n'+log)
 assert not bad,bad.group(0) if bad else ''
 assert len(re.findall(r'new '+speed+r'-speed USB device',kernel))==7,'startup enumeration count/speed'
 assert log.count('WS73_VIRTUAL_USB_PASS')==1
 assert re.findall(r'WS73_STARTUP_CASE (\d+) BEGIN',log)==list(map(str,range(7)))
 assert re.findall(r'WS73_STARTUP_CASE (\d+) PASS',log)==list(map(str,range(7)))
 parts=re.findall(r'WS73_STARTUP_CASE (\d+) BEGIN(.*?)WS73_STARTUP_CASE \1 PASS',log,re.S)
 assert [int(n) for n,_ in parts]==list(range(7))
 kernel_results=re.findall(r'synthetic startup result=([^\r\n]*)',kernel)
 log_results=re.findall(r'synthetic startup result=([^\r\n]*)',log)
 assert kernel_results==log_results and len(kernel_results)==7,'actual/captured results'
 assert kernel.count('synthetic runtime identity verified')==7
 assert kernel.count('device callbacks drained')==7
 for number,part in parts:
  n=int(number); records=re.findall(r'WS73_TEST_KMSG ([^\r\n]*)',part)
  results=re.findall(r'synthetic startup result=(-?\d+) stage=(\w+) state=(\d+) action=(\d+)',part)
  assert len(results)==1,number
  result,stage,state,action=results[0]
  if n in (0,6):assert (result,stage,state,action)==('0','disable','6','4'),number
  else:
   assert stage=='startup' and state=='7' and action=='5',number
   assert int(result) in ({4:{-19,-108,-71}}.get(n, {[-1,-110,-32,-90,-1,-5,-1][n]})),number
  assert re.findall(r'WS73_TEST_OUT step=(\d) actual=(\d+) verified',part)==[('0','36'),('1','36'),('2','128'),('3','128')],number
  assert re.findall(r'WS73_TEST_IN kind=(\w+) actual=(\d+) synthetic',part)==[('chip','124'),('version','124')],number
  assert part.count('synthetic runtime identity verified')==1,number
  assert re.findall(r'WS73_STARTUP_OUT step=(\d) actual=(\d+) verified',part)==([] if n==5 else [('4','36'),('5','36'),('6','864')]),number
  assert re.findall(r'WS73_STARTUP_INT actual=(\d+) bits=([0-9a-f]+) synthetic',part)==[('8','00000041' if n==5 else '00000001')],number
  notifications=[] if n==5 else [('0','108','0'),('1','108',str(int(n==3)))]
  if n in (0,6):notifications += [('2','108','0'),('3','108','0')]
  assert re.findall(r'WS73_STARTUP_NOTIFY kind=(\d+) actual=(\d+) malformed=(\d+) synthetic',part)==notifications,number
  actions=[] if n in (2,3,5) else [('25','4')]
  if n in (0,6):actions += [('26','4')]
  assert re.findall(r'WS73_STARTUP_ACTION message=(\d+) actual=(\d+) verified',part)==actions,number
  setups=len(actions)+int(n==2)
  assert re.findall(r'WS73_STARTUP_SETUP (\w+) verified',part)==['2100000000000400']*setups,number
  assert part.count('WS73_STARTUP_CONTROL_STALL')==int(n==2),number
  assert part.count('WS73_STARTUP_WITHHOLD_ACTION_REPLY')==int(n==1),number
  delivered=re.findall(r'function notification observation (\d+); delivered to armed request',part)
  assert delivered==([] if n==5 else ['0'] if n==3 else ['0','1','2','3'] if n in (0,6) else ['0','1']),number
  assert 'no request completion' not in part,number
  assert sum('BSP notification observation' in x for x in records)==int(n!=5),number
  assert sum('USB disconnect' in x for x in records)==1,number
  assert sum('device callbacks drained' in x for x in records)==1 and 'device callbacks drained' in records[-1],number
  assert part.count('WS73_TEST_REMOVE')==1,number
  assert part.index('WS73_TEST_REMOVE')<part.index('device callbacks drained'),number
  assert part.index('synthetic startup result=')<part.index('device callbacks drained'),number
  assert part.count('WS73_STARTUP_NO_EXTRA_OUT window_ms=100')==int(n!=4),number
  if n!=4:assert part.index('synthetic startup result=')<part.index('WS73_STARTUP_NO_EXTRA_OUT')<part.index('WS73_TEST_REMOVE'),number
  else:assert part.index('WS73_TEST_REMOVE')<part.index('synthetic startup result='),number
  errors=re.findall(r'interrupt observation failed: (-\d+)',part)
  removal=[e for e in errors if e in ('-19','-108','-71')]
  assert len(removal)<=1,number
  if removal:assert part.index('WS73_TEST_REMOVE')<part.index('interrupt observation failed: '+removal[0]),number
  assert [e for e in errors if e not in removal]==(['-5'] if n==5 else []),number
  expected_records=4+int(n!=5)+len(delivered)+int(n==5)+len(removal)
  assert len(records)==expected_records,number
 return {'cases':7,'bulk_out':46,'bulk_in':30,'class_setups':7,'class_payloads':6,'interrupts':7,'disconnects':7,'drains':7,'no_extra_out_checks':6}

def validate_boot(kernel, log, speed="full"):
 bad=re.search(r'BUG:|WARNING:|UBSAN:|Kernel panic|possible circular|inconsistent lock state|sleeping function called|WS73_TEST_ERROR|WS73_VIRTUAL_USB_FAIL',kernel+'\n'+log)
 assert not bad, bad.group(0) if bad else ''
 assert len(re.findall(r"new "+speed+r"-speed USB device",kernel))==14,'boot enumeration count/speed'
 assert log.count('WS73_VIRTUAL_USB_PASS')==1,'missing/repeated overall PASS'
 assert re.findall(r'WS73_BOOT_CASE (\d+) BEGIN',log)==[str(i) for i in range(14)]
 assert re.findall(r'WS73_BOOT_CASE (\d+) PASS',log)==[str(i) for i in range(14)]
 parts=re.findall(r'WS73_BOOT_CASE (\d+) BEGIN(.*?)WS73_BOOT_CASE \1 PASS',log,re.S)
 assert [int(x[0]) for x in parts]==list(range(14)),'missing/reordered boot case'
 words=[0,10,12,4,40,0,0,0,0,0,0,0,0,10]
 lengths=[4,4,4,4,4,0,1,3,5,512,0,0,4,4]
 identities=[(0,0),(2,0),(0,0),(1,1),(2,2)]
 for number,part in parts:
  n=int(number)
  records=re.findall(r'WS73_TEST_KMSG ([^\r\n]*)',part)
  assert len(records)==(4 if n<5 or n>11 else 3),'missing/extra boot kernel record '+number
  observations=[x for x in records if 'boot chip observation' in x]
  assert len(observations)==1,number
  observed=observations[0]
  if n<5 or n>11:
   package,bus=identities[n if n<5 else n-12]
   expected=f'boot chip observation 0x{words[n]:08x}: package {package} bus {bus}; cold-read qualification pending'
   assert observed.endswith(expected),number
  elif n<10:assert observed.endswith('boot chip observation failed: -90'),number
  elif n==10:assert observed.endswith('boot chip observation failed: -110'),number
  else:assert re.search(r'boot chip observation failed: -(19|108|71)$',observed),number
  assert sum('USB disconnect' in x for x in records)==1,number
  assert 'device callbacks drained' in records[-1],number
  assert re.findall(r'WS73_BOOT_OUT actual=(\d+) (\w+) verified',part)==[('19','READM_40019000_4')],number
  incoming=[] if n in (10,11) else [(str(lengths[n]),f'{words[n]:08x}')]
  assert re.findall(r'WS73_BOOT_IN actual=(\d+) word=([0-9a-f]+) synthetic',part)==incoming,number
  version_ok = n<5 or n>11
  assert part.count('WS73_BOOT_VERSION_OUT actual=8 verified')==int(version_ok),number
  version_len=64 if n in (0,12) else 3
  assert re.findall(r'WS73_BOOT_VERSION_IN actual=(\d+) synthetic',part)==([str(version_len)] if version_ok else []),number
  versions=[x for x in records if 'boot version observation' in x]
  assert len(versions)==int(version_ok),number
  if version_ok:
   assert versions[0].endswith(f'boot version observation: {version_len} bytes '+('56'*64 if version_len==64 else '563100')),number
   assert part.index('boot chip observation')<part.index('WS73_BOOT_VERSION_OUT')<part.index('boot version observation')<part.index('WS73_BOOT_NO_EXTRA_OUT'),number
  assert part.count('WS73_BOOT_WITHHOLD_REPLY')==(1 if n==10 else 0),number
  assert part.count('WS73_BOOT_NO_EXTRA_OUT window_ms=100')==(0 if n==11 else 1),number
  assert part.count('WS73_TEST_REMOVE')==1,number
  assert part.index('WS73_TEST_REMOVE')<part.index('USB disconnect')<part.index('device callbacks drained'),number
  if n==11:assert part.index('WS73_TEST_REMOVE')<part.index('boot chip observation failed:')<part.index('device callbacks drained'),number
  else:assert part.index('boot chip observation')<part.index('WS73_BOOT_NO_EXTRA_OUT')<part.index('WS73_TEST_REMOVE'),number
 assert log.count('WS73_TEST_KMSG ')==49,'extra captured boot records'
 assert len(re.findall(r'boot version observation: 3 bytes 563100',kernel))==5,'short version observation count'
 assert len(re.findall('boot version observation: 64 bytes '+'56'*64,kernel))==2,'full-packet version observation count'
 assert len(re.findall(r'boot chip observation(?: failed:| 0x)',kernel))==14,'extra/missing actual boot observations'
 assert log.count('WS73_BOOT_OUT ')==14 and log.count('WS73_BOOT_IN ')==12
 assert 'runtime chip observation' not in kernel and 'WS73_TEST_CASE' not in log
 return {'cases':14,'out_transfers':21,'in_transfers':19,'version_queries':7,'disconnects':14,'drains':14,'timeout_cases':1,'no_extra_out_checks':13}
def validate_download(kernel, log, speed="full"):
 bad=re.search(r'BUG:|WARNING:|UBSAN:|Kernel panic|possible circular|inconsistent lock state|sleeping function called|WS73_TEST_ERROR|WS73_VIRTUAL_USB_FAIL',kernel+'\n'+log)
 assert not bad,bad.group(0) if bad else ''
 assert len(re.findall(r'new '+speed+r'-speed USB device',kernel))==7,'download enumeration count/speed'
 assert log.count('WS73_VIRTUAL_USB_PASS')==1
 assert re.findall(r'WS73_DOWNLOAD_CASE (\d+) BEGIN',log)==list(map(str,range(7)))
 assert re.findall(r'WS73_DOWNLOAD_CASE (\d+) PASS',log)==list(map(str,range(7)))
 parts=re.findall(r'WS73_DOWNLOAD_CASE (\d+) BEGIN(.*?)WS73_DOWNLOAD_CASE \1 PASS',log,re.S)
 assert [int(n) for n,_ in parts]==list(range(7))
 commands=['FILES 1 0x400000 0x8000 0x0 ','FILES 1 0x408000 0x1 0x2 ','FILES 1 0x430000 0x20 0x3 ','FILES 1 0x440000 0x20 0x3 ']
 payload_count=0
 for number,part in parts:
  n=int(number);success=n in (0,6)
  assert re.findall(r"WS73_DOWNLOAD_PACKET_SIZE (\d+)",part)==["64" if speed=="full" else "512"],number
  records=re.findall(r'WS73_TEST_KMSG ([^\r\n]*)',part)
  assert len(records)==(4 if n==5 else 3),number
  results=re.findall(r'synthetic download result=(-?\d+) stage=(\d+) image=(\d+) ack=(\d+)',part)
  assert len(results)==1,number
  result,stage,image,ack=map(int,results[0])
  if success:assert (result,stage,image,ack)==(0,6,2,32),number
  elif n in (1,2,3):assert (result,stage,image,ack)==(-110 if n==3 else -71,4 if n==2 else 2,0,0),number
  else:assert result in (-19,-108,-71,-2,-104) and (stage,image,ack)==(2 if n==4 else 3,0,0),number
  assert re.findall(r'WS73_DOWNLOAD_COMMAND chunk=(\d+) actual=(\d+) verified',part)==[(str(i),str(len(commands[i]))) for i in range(4 if success else 1)],number
  payloads=re.findall(r'WS73_DOWNLOAD_PAYLOAD chunk=(\d+) actual=(\d+) delay_us=(\d+) verified',part)
  assert [(a,b) for a,b,_ in payloads]==[(str(i),str([32768,1,32,32][i])) for i in range(4 if success else 1 if n==2 else 0)],number
  assert all(int(delay)>=5000 for _,_,delay in payloads),number
  payload_count+=len(payloads)
  replies=[('READY','5'),('FILES OK','8')]*4 if success else [('READX','5')] if n==1 else [('READY','5'),('FILES NO','8')] if n==2 else [('READY','5')] if n==5 else []
  assert re.findall(r'WS73_DOWNLOAD_REPLY ([A-Z ]+) actual=(\d+)',part)==replies,number
  assert part.count('WS73_DOWNLOAD_QUIT actual=5 verified')==int(success),number
  assert part.count('WS73_DOWNLOAD_WITHHOLD_READY')==int(n==3),number
  assert part.count('WS73_DOWNLOAD_NO_EXTRA_OUT window_ms=100')==int(n not in (4,5)),number
  assert part.count('WS73_TEST_REMOVE')==1,number
  assert sum('USB disconnect' in x for x in records)==1,number
  assert records[-1].endswith('synthetic download callbacks drained'),number
  assert part.index('WS73_TEST_REMOVE')<part.index('USB disconnect')<part.index('synthetic download callbacks drained'),number
  if n in (4,5):assert part.index('WS73_TEST_REMOVE')<part.index('synthetic download result='),number
  else:assert part.index('synthetic download result=')<part.index('WS73_DOWNLOAD_NO_EXTRA_OUT')<part.index('WS73_TEST_REMOVE'),number
  if n==5:assert part.index('synthetic payload admission')<part.index('WS73_TEST_REMOVE'),number
 waits=re.findall(r'synthetic READY wait ns=(\d+)',kernel)
 assert len(waits)==10 and all(int(ns)>=5000000 for ns in waits),'actual shared READY delay'
 assert log.count('WS73_TEST_KMSG ')==22
 assert log.count('WS73_DOWNLOAD_COMMAND ')==13 and log.count('WS73_DOWNLOAD_PAYLOAD ')==9
 assert log.count('WS73_DOWNLOAD_REPLY ')==20 and log.count('WS73_DOWNLOAD_QUIT ')==2
 assert kernel.count('synthetic download result=')==7 and kernel.count('synthetic download callbacks drained')==7
 assert 'boot chip observation' not in kernel and 'runtime chip observation' not in kernel
 return {'cases':7,'files_commands':13,'payloads':payload_count,'quit_commands':2,'disconnects':7,'drains':7,'no_extra_out_checks':5,'scope':'synthetic download only; READY timing measured from gadget submission to first payload read'}
def validate_loader(kernel, log, speed="full"):
 bad=re.search(r'BUG:|WARNING:|UBSAN:|Kernel panic|possible circular|inconsistent lock state|sleeping function called|WS73_TEST_ERROR|WS73_VIRTUAL_USB_FAIL',kernel+'\n'+log)
 assert not bad,bad.group(0) if bad else ''
 assert len(re.findall(r'new '+speed+r'-speed USB device',kernel))==10
 assert log.count('WS73_VIRTUAL_USB_PASS')==1
 assert re.findall(r'WS73_LOADER_CASE (\d+) BEGIN',log)==list(map(str,range(10)))
 assert re.findall(r'WS73_LOADER_CASE (\d+) PASS',log)==list(map(str,range(10)))
 parts=re.findall(r'WS73_LOADER_CASE (\d+) BEGIN(.*?)WS73_LOADER_CASE \1 PASS',log,re.S)
 assert [int(n) for n,_ in parts]==list(range(10))
 words=[0,10,0,0,0,4,40,0,0,0]
 writes=['WRITEM 4 0x40017750 51 ','WRITEM 4 0x40017790 0 ','WRITEM 4 0x40017794 0 ','WRITEM 4 0x4001775c 4390912 ','WRITEM 4 0x40017760 4423680 ','WRITEM 4 0x40017750 63 ','WRITEM 4 0x40017750 47 ','WRITEM 4 0x40017798 0 ','WRITEM 4 0x40017764 4456448 ','WRITEM 4 0x40017750 63 ','WRITEM 4 0x40019408 513 ']
 for number,part in parts:
  n=int(number);success=n in (0,1,9)
  records=re.findall(r'WS73_TEST_KMSG ([^\r\n]*)',part)
  assert len(records)==3,number
  results=re.findall(r'synthetic loader result=(-?\d+) repeat=(-?\d+) phase=(\d+) stage=(\d+) prep=(\d+) image=(\d+) ack=(\d+) raw=([0-9a-f]{8})',part)
  assert len(results)==1,number
  result,repeated,phase,stage,prep,image,ack=map(int,results[0][:7]);raw=int(results[0][7],16)
  assert raw==words[n],number
  if success:assert (result,repeated,phase,stage,prep,image,ack)==(0,-114,4,6,11,2,34056 if n==1 else 7708),number
  else:
   assert repeated==result,number
   if n==7:assert result in (-19,-108,-71,-2,-104) and phase==1,number
   else:assert (result,phase)==({2:-2,3:-129,4:-90,5:-95,6:-95,8:-71}[n],3 if n==8 else 2),number
   assert (stage,prep,image,ack)==(0,0,0,0),number
  assert part.count('WS73_LOADER_READM actual=19 verified')==1,number
  assert re.findall(r'WS73_LOADER_ID actual=(\d+) word=([0-9a-f]+) synthetic',part)==([] if n==7 else [('4',f'{words[n]:08x}')]),number
  assert re.findall(r'WS73_LOADER_WRITEM step=(\d+) actual=(\d+) verified',part)==[(str(i),str(len(writes[i]))) for i in range(11 if success else 1 if n==8 else 0)],number
  assert re.findall(r'WS73_LOADER_REPLY ([A-Z ]+) actual=(\d+)',part)==([('WRITEM OK','9')]*11 if success else [('WRITEM NO','9')] if n==8 else []),number
  expected=[]
  if success:
   for image,size in enumerate([137580,20996,34056] if n==1 else [143892,20980,7708]):
    for offset in range(0,size,32768):
     amount=min(32768,size-offset);flag=(3 if amount==size else 0) if not offset else 2 if amount==size-offset else 1
     expected.append(tuple(map(str,(image,offset,amount,flag))))
  assert re.findall(r'WS73_LOADER_FILES image=(\d+) offset=(\d+) size=(\d+) flag=(\d+) verified',part)==expected,number
  assert re.findall(r'WS73_LOADER_PAYLOAD image=(\d+) offset=(\d+) actual=(\d+) original',part)==[x[:3] for x in expected],number
  assert part.count('WS73_LOADER_FILES_OK actual=8')==len(expected),number
  assert part.count('WS73_LOADER_QUIT actual=5 verified')==int(success),number
  assert part.count('WS73_LOADER_NO_EXTRA_OUT window_ms=100')==int(n!=7),number
  assert part.count('WS73_TEST_REMOVE')==1 and sum('USB disconnect' in x for x in records)==1,number
  assert records[-1].endswith('synthetic download callbacks drained'),number
  assert part.index('WS73_TEST_REMOVE')<part.index('USB disconnect')<part.index('synthetic download callbacks drained'),number
  if n==7:assert part.index('WS73_TEST_REMOVE')<part.index('synthetic loader result='),number
  else:assert part.index('synthetic loader result=')<part.index('WS73_LOADER_NO_EXTRA_OUT')<part.index('WS73_TEST_REMOVE'),number
  if success:assert part.index('WS73_LOADER_ID')<part.index('WS73_LOADER_WRITEM')<part.index('WS73_LOADER_FILES')<part.index('WS73_LOADER_QUIT')<part.index('synthetic loader result='),number
 assert log.count('WS73_TEST_KMSG ')==30
 assert log.count('WS73_LOADER_READM ')==10 and log.count('WS73_LOADER_ID ')==9
 assert log.count('WS73_LOADER_WRITEM ')==34 and log.count('WS73_LOADER_FILES ')==22 and log.count('WS73_LOADER_PAYLOAD ')==22
 assert log.count('WS73_LOADER_QUIT ')==3
 assert kernel.count('synthetic loader result=')==10 and kernel.count('synthetic download callbacks drained')==10
 waits=re.findall(r'synthetic READY wait ns=(\d+)',kernel)
 assert len(waits)==22 and all(int(x)>=5000000 for x in waits)
 assert 'runtime chip observation' not in kernel and 'synthetic download result=' not in kernel
 return {'cases':10,'identity_queries':10,'preparation_commands':34,'files_commands':22,'payloads':22,'quit_commands':3,'drains':10,'no_extra_out_checks':9,'scope':'original SDK files via kernel firmware loader to synthetic USB; no physical firmware execution'}

def validate_loader_inputs(record, manifest):
 import importlib.util
 import sys
 sys.dont_write_bytecode=True
 source=TOOLS.parent/'check-firmware.py'
 spec=importlib.util.spec_from_file_location('ws73_expected_firmware',source)
 checker=importlib.util.module_from_spec(spec);spec.loader.exec_module(checker)
 expected={'firmware/'+variant+'/'+name for variant,files in checker.PROFILES.items() for name in files}
 assert set(manifest['firmware_inputs'])==expected,'firmware input set'
 for variant,files in checker.PROFILES.items():
  for name,(size,digest) in files.items():
   key='firmware/'+variant+'/'+name;p=record/key
   assert p.stat().st_size==size and sha(p)==digest,'original firmware identity'
   assert manifest['materials'][key]==digest,'firmware material hash'
   entry=manifest['firmware_inputs'][key]
   assert entry['status']=='PASS' and entry['sha256']==digest and entry['size_read']==size,'firmware provenance'
 original=(record/'firmware/us/ws73.bin').read_bytes();mutant=bytearray(original);mutant[100]^=1
 mutant[:64]=hashlib.sha256(mutant[64:]).hexdigest().encode()
 p=record/'firmware/rehashed-ws73.bin'
 assert p.read_bytes()==mutant and manifest['materials']['firmware/rehashed-ws73.bin']==sha(p),'rehashed negative input'

def loader_materials(record, directory):
 import importlib.util
 import sys
 sys.dont_write_bytecode=True
 source=TOOLS.parent/'check-firmware.py'
 spec=importlib.util.spec_from_file_location('ws73_firmware_inputs',source)
 checker=importlib.util.module_from_spec(spec);spec.loader.exec_module(checker)
 archive_files={};materials={};provenance={}
 shutil.copyfile(source,record/'check-firmware.py');materials['check-firmware.py']=sha(record/'check-firmware.py')
 for variant in ['us','e']:
  report=checker.check(directory/variant,variant);assert report['status']=='PASS',report
  for entry in report['images']:
   local='firmware/'+variant+'/'+entry['file'];dest=record/local;dest.parent.mkdir(parents=True,exist_ok=True)
   shutil.copyfile(entry['path'],dest);assert sha(dest)==entry['sha256']
   archive_files['lib/firmware/hisilicon/ws73/1.10.110/'+variant+'/'+entry['file']]=dest
   materials[local]=sha(dest);provenance[local]=entry
 original=record/'firmware/us/ws73.bin';mutant=bytearray(original.read_bytes());mutant[100]^=1
 mutant[:64]=hashlib.sha256(mutant[64:]).hexdigest().encode()
 dest=record/'firmware/rehashed-ws73.bin';dest.write_bytes(mutant)
 assert len(mutant)==original.stat().st_size and sha(dest)!=sha(original)
 archive_files['mutant.bin']=dest;materials['firmware/rehashed-ws73.bin']=sha(dest)
 return archive_files,materials,provenance

def library(name):
 path=pathlib.Path(subprocess.check_output(['gcc','-print-file-name='+name],text=True).strip()).resolve()
 if not path.is_file():raise RuntimeError('Missing host runtime library: '+name)
 return path
def source_identity():
 return {key:subprocess.check_output(['git','-C',str(SOURCE),*args],text=True).strip() for key,args in [('head',['rev-parse','HEAD']),('status',['status','--porcelain'])]}
def archive(init_binary, firmware):
 result=bytearray();ino=1
 def item(name,mode,data=b'',major=0,minor=0):
  nonlocal ino
  n=name.encode()+b'\0'
  fields=[ino,mode,0,0,1,0,len(data),0,0,major,minor,len(n),0];ino+=1
  result.extend(b'070701'+''.join(f'{x:08x}' for x in fields).encode()+n)
  result.extend(b'\0'*((-len(result))%4));result.extend(data);result.extend(b'\0'*((-len(result))%4))
 for name in ['dev','proc','sys','lib64']:item(name,stat.S_IFDIR|0o755)
 item('dev/console',stat.S_IFCHR|0o600,major=5,minor=1)
 files={'init':init_binary,'lib64/libc.so.6':library('libc.so.6'),'lib64/ld-linux-x86-64.so.2':library('ld-linux-x86-64.so.2')}
 files.update(firmware)
 parents={str(parent) for name in firmware for parent in pathlib.PurePosixPath(name).parents if str(parent)!='.'}
 for name in sorted(parents,key=lambda x:(x.count('/'),x)):item(name,stat.S_IFDIR|0o755)
 for name,path in files.items():item(name,stat.S_IFREG|0o755,path.read_bytes())
 item('TRAILER!!!',0)
 return gzip.compress(bytes(result),mtime=0),{str(path):sha(path) for path in files.values()}
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--speed',choices=['full','high'],default='full');parser.add_argument('--phase',choices=['runtime','boot','download','loader','startup'],default='runtime');parser.add_argument('--firmware-root',type=pathlib.Path);args=parser.parse_args()
 assert bool(args.firmware_root)==(args.phase=='loader'),'loader phase requires the local SDK firmware root; other phases take no firmware'
 with (WORK/'build.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
  proofdir=pathlib.Path((WORK/'usb-vm-build-record').read_text().strip());proof=json.loads((proofdir/'manifest.json').read_text())
  before=source_identity();assert not before['status']
  validate_build(proofdir,proof,before['head'])
  stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');record=WORK/'records'/('virtual-usb-'+stamp);record.mkdir();print(record,flush=True)
  manifest={'status':'INCOMPLETE','compiled_source':proof['source'],'speed':args.speed,'phase':args.phase,'scope':'FunctionFS/dummy_hcd synthetic USB integration in hardware-free VM; no physical/firmware/HCI qualification','build_record':str(proofdir),'harness_source':before,'tools':{},'materials':{}}
  def save():(record/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
  save()
  try:
   for tool in ['gcc','qemu-system-x86_64','python3']:
    path=pathlib.Path(shutil.which(tool));manifest['tools'][tool]={'path':str(path),'sha256':sha(path),'version':subprocess.check_output([tool,'--version'],text=True).splitlines()[0]}
    candidates={(pathlib.Path(entry)/tool).resolve() for entry in os.get_exec_path() if (pathlib.Path(entry)/tool).is_file()}
    manifest['tools'][tool]['path_candidates_sha256']={str(p):sha(p) for p in sorted(candidates)}
   for source,name in [(proofdir/'manifest.json','build-proof.json'),(proofdir/'arch/x86/boot/bzImage','bzImage'),(proofdir/'.config','.config'),(TOOLS/'init.c','init.c'),(TOOLS/'run.py','run.py'),(TOOLS/'build.py','build.py')]:
    shutil.copyfile(source,record/name);manifest['materials'][name]=sha(record/name)
   compile_cmd=['gcc','-DWS73_TEST_STARTUP='+('1' if args.phase=='startup' else '0'),'-DWS73_TEST_BOOT='+('1' if args.phase=='boot' else '0'),'-DWS73_TEST_DOWNLOAD='+('1' if args.phase=='download' else '0'),'-DWS73_TEST_LOADER='+('1' if args.phase=='loader' else '0'),'-O2','-Wall','-Wextra','-Werror','-o',str(record/'init'),str(record/'init.c')]
   manifest['init_compile']=compile_cmd
   with (record/'init-build.log').open('w') as build_log:
    subprocess.run(compile_cmd,check=True,stdout=build_log,stderr=subprocess.STDOUT,env=dict(os.environ,CCACHE_DISABLE='1'))
   manifest['materials']['init']=sha(record/'init')
   firmware={}
   if args.phase=='loader':
    firmware,materials,inputs=loader_materials(record,args.firmware_root.resolve())
    manifest['materials'].update(materials);manifest['firmware_inputs']=inputs
    validate_loader_inputs(record,manifest)
   blob,inputs=archive(record/'init',firmware);(record/'initramfs.cpio.gz').write_bytes(blob);manifest['materials']['initramfs.cpio.gz']=sha(record/'initramfs.cpio.gz');manifest['archive_inputs']=inputs
   cmd=['qemu-system-x86_64','-machine','q35','-accel','kvm','-smp','2','-m','1024','-nodefaults','-display','none','-monitor','none','-serial','stdio','-serial','file:'+str(record/'fixture.log'),'-nic','none','-no-reboot','-kernel',str(record/'bzImage'),'-initrd',str(record/'initramfs.cpio.gz'),'-append','console=ttyS0 rdinit=/init panic_on_warn=1 panic=1 dyndbg="file drivers/misc/ws73/usb.c +p" dummy_hcd.is_high_speed='+('1' if args.speed=='high' else '0')]
   manifest['command']=cmd;save()
   with (record/'kernel.log').open('w') as log:
    result=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=45)
   manifest['exit_code']=result.returncode;assert result.returncode==0
   manifest['observed']=(validate_startup if args.phase=='startup' else validate_loader if args.phase=='loader' else validate_download if args.phase=='download' else validate_boot if args.phase=='boot' else validate)((record/'kernel.log').read_text(errors='replace'),(record/'fixture.log').read_text(errors='replace'),args.speed)
   assert source_identity()==before,'source changed during VM test'
   manifest['status']='PASS'
  except BaseException as error:
   manifest['status']='FAIL';manifest['error']=repr(error);raise
  finally:
   for name in ['kernel.log','fixture.log']:
    if (record/name).exists():manifest['materials'][name]=sha(record/name)
   save();print(manifest['status'],flush=True)
if __name__=='__main__':main()

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
                 ['usb','boot','firmware','runtime','usb-tx','usb-wire','service',
                  'ble-control','customize','customize-request','startup'])
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
  assert len(records)==3,'missing/extra boot kernel record '+number
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
  assert part.count('WS73_BOOT_WITHHOLD_REPLY')==(1 if n==10 else 0),number
  assert part.count('WS73_BOOT_NO_EXTRA_OUT')==(0 if n==11 else 1),number
  assert part.count('WS73_TEST_REMOVE')==1,number
  assert part.index('WS73_TEST_REMOVE')<part.index('USB disconnect')<part.index('device callbacks drained'),number
  if n==11:assert part.index('WS73_TEST_REMOVE')<part.index('boot chip observation failed:')<part.index('device callbacks drained'),number
  else:assert part.index('boot chip observation')<part.index('WS73_BOOT_NO_EXTRA_OUT')<part.index('WS73_TEST_REMOVE'),number
 assert log.count('WS73_TEST_KMSG ')==42,'extra captured boot records'
 assert len(re.findall(r'boot chip observation(?: failed:| 0x)',kernel))==14,'extra/missing actual boot observations'
 assert log.count('WS73_BOOT_OUT ')==14 and log.count('WS73_BOOT_IN ')==12
 assert 'runtime chip observation' not in kernel and 'WS73_TEST_CASE' not in log
 return {'cases':14,'out_transfers':14,'in_transfers':12,'disconnects':14,'drains':14,'timeout_cases':1,'no_extra_out_checks':13}
def library(name):
 path=pathlib.Path(subprocess.check_output(['gcc','-print-file-name='+name],text=True).strip()).resolve()
 if not path.is_file():raise RuntimeError('Missing host runtime library: '+name)
 return path
def source_identity():
 return {key:subprocess.check_output(['git','-C',str(SOURCE),*args],text=True).strip() for key,args in [('head',['rev-parse','HEAD']),('status',['status','--porcelain'])]}
def archive(init_binary):
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
 for name,path in files.items():item(name,stat.S_IFREG|0o755,path.read_bytes())
 item('TRAILER!!!',0)
 return gzip.compress(bytes(result),mtime=0),{str(path):sha(path) for path in files.values()}
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--speed',choices=['full','high'],default='full');parser.add_argument('--phase',choices=['runtime','boot'],default='runtime');args=parser.parse_args()
 with (WORK/'build.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
  proofdir=pathlib.Path((WORK/'usb-vm-build-record').read_text().strip());proof=json.loads((proofdir/'manifest.json').read_text())
  before=source_identity();assert not before['status']
  validate_build(proofdir,proof,before['head'])
  stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');record=WORK/'records'/('virtual-usb-'+stamp);record.mkdir();print(record,flush=True)
  manifest={'status':'INCOMPLETE','compiled_source':proof['source'],'speed':args.speed,'phase':args.phase,'scope':'FunctionFS/dummy_hcd synthetic USB integration in hardware-free VM; no physical/firmware/HCI qualification','build_record':str(proofdir),'harness_source':before,'tools':{},'materials':{}}
  def save():(record/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
  for tool in ['gcc','qemu-system-x86_64','python3']:
   path=pathlib.Path(shutil.which(tool));manifest['tools'][tool]={'path':str(path),'sha256':sha(path),'version':subprocess.check_output([tool,'--version'],text=True).splitlines()[0]}
   candidates={(pathlib.Path(entry)/tool).resolve() for entry in os.get_exec_path() if (pathlib.Path(entry)/tool).is_file()}
   manifest['tools'][tool]['path_candidates_sha256']={str(p):sha(p) for p in sorted(candidates)}
  for source,name in [(proofdir/'manifest.json','build-proof.json'),(proofdir/'arch/x86/boot/bzImage','bzImage'),(proofdir/'.config','.config'),(TOOLS/'init.c','init.c'),(TOOLS/'run.py','run.py'),(TOOLS/'build.py','build.py')]:
   shutil.copyfile(source,record/name);manifest['materials'][name]=sha(record/name)
  compile_cmd=['gcc','-DWS73_TEST_BOOT='+('1' if args.phase=='boot' else '0'),'-O2','-Wall','-Wextra','-Werror','-o',str(record/'init'),str(record/'init.c')]
  manifest['init_compile']=compile_cmd
  with (record/'init-build.log').open('w') as build_log:
   subprocess.run(compile_cmd,check=True,stdout=build_log,stderr=subprocess.STDOUT,env=dict(os.environ,CCACHE_DISABLE='1'))
  manifest['materials']['init']=sha(record/'init')
  blob,inputs=archive(record/'init');(record/'initramfs.cpio.gz').write_bytes(blob);manifest['materials']['initramfs.cpio.gz']=sha(record/'initramfs.cpio.gz');manifest['archive_inputs']=inputs
  cmd=['qemu-system-x86_64','-machine','q35','-accel','kvm','-smp','2','-m','1024','-nodefaults','-display','none','-monitor','none','-serial','stdio','-serial','file:'+str(record/'fixture.log'),'-nic','none','-no-reboot','-kernel',str(record/'bzImage'),'-initrd',str(record/'initramfs.cpio.gz'),'-append','console=ttyS0 rdinit=/init panic_on_warn=1 panic=1 dyndbg="file drivers/misc/ws73/usb.c +p" dummy_hcd.is_high_speed='+('1' if args.speed=='high' else '0')]
  manifest['command']=cmd;save()
  try:
   with (record/'kernel.log').open('w') as log:
    result=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=45)
   manifest['exit_code']=result.returncode;assert result.returncode==0
   manifest['observed']=(validate_boot if args.phase=='boot' else validate)((record/'kernel.log').read_text(errors='replace'),(record/'fixture.log').read_text(errors='replace'),args.speed)
   assert source_identity()==before,'source changed during VM test'
   manifest['status']='PASS'
  except BaseException as error:
   manifest['status']='FAIL';manifest['error']=repr(error);raise
  finally:
   for name in ['kernel.log','fixture.log']:
    if (record/name).exists():manifest['materials'][name]=sha(record/name)
   save();print(manifest['status'],flush=True)
if __name__=='__main__':main()

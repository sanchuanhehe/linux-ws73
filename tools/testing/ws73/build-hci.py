#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build native WS73 HCI without loading modules or accessing hardware."""
from pathlib import Path
import argparse,datetime,fcntl,hashlib,json,os,shutil,subprocess
source=Path(__file__).resolve().parents[3];work=source.parent/'.dev/ws73-upstream'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(source),*args],text=True).strip()
if not __debug__:raise RuntimeError('Run without Python optimization')
PROFILES = {
 'builtin': ('y', 'y', 'y'),
 'module': ('m', 'm', 'm'),
 'bt-module': ('y', 'm', 'm'),
 'hci-module': ('y', 'y', 'm'),
}
# USB owner, Bluetooth stack and native consumer; selectors resolve the core.
MODULES = {
 'WS73_CORE': 'drivers/misc/ws73/ws73-core.ko',
 'WS73_USB': 'drivers/misc/ws73/ws73-usb.ko',
 'WS73_USB_IO': 'drivers/misc/ws73/ws73-usb-io.ko',
 'WS73_USB_STARTUP_TEST': 'drivers/misc/ws73/ws73-usb-startup-test.ko',
 'BT': 'net/bluetooth/bluetooth.ko',
 'BT_WS73': 'drivers/bluetooth/btws73-hci.ko',
 'BT_WS73_PROTO': 'drivers/bluetooth/btws73_proto.ko',
}
p=argparse.ArgumentParser(description=__doc__);p.add_argument('mode',choices=PROFILES);args=p.parse_args()
work.mkdir(parents=True,exist_ok=True)
with (work/'build.lock').open('a') as lock:
 fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
 assert not git('status','--porcelain')
 head=git('rev-parse','HEAD');record=work/'records'/('hci-build-'+args.mode+'-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ'));record.mkdir(parents=True);print(record,flush=True)
 build=work/'build-hci';build.mkdir(exist_ok=True)
 text=(source/'tools/testing/ws73/usb-vm/usb.config').read_text().replace('CONFIG_WS73_USB_BOOT_TEST=y', '# CONFIG_WS73_USB_BOOT_TEST is not set')
 usb, bluetooth, hci = PROFILES[args.mode]
 expected = {'WS73_USB': usb, 'WS73_USB_IO': usb, 'BT': bluetooth,
             'BT_WS73': hci, 'BT_WS73_PROTO': hci,
             'WS73_USB_STARTUP_TEST': usb,
             'WS73_CORE': 'y' if 'y' in (usb, hci) else 'm'}
 for symbol in ('WS73_USB', 'BT', 'BT_WS73', 'WS73_USB_STARTUP_TEST'):
  text = text.replace(f'CONFIG_{symbol}=y', f'CONFIG_{symbol}={expected[symbol]}')
 has_modules = 'm' in expected.values()
 text += 'CONFIG_MODULES=y\n' if has_modules else '# CONFIG_MODULES is not set\n'
 expected_lines = {f'CONFIG_{symbol}={value}' for symbol, value in expected.items()}
 expected_modules = {MODULES[symbol] for symbol, value in expected.items() if value == 'm'}
 (record/'config').write_text(text);shutil.copyfile(__file__,record/'build.py')
 result={'status':'INCOMPLETE','source':head,'mode':args.mode,'resolved_config':expected,'expected_modules':sorted(expected_modules),'jobs':4,'builder_sha256':sha(Path(__file__)),'config_sha256':sha(record/'config'),'commands':[],'tools':{},'artifacts':{},'scope':'native HCI compile/link only; no HCI runtime or hardware qualification'}
 def save():(record/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
 save()
 try:
  for name in ['make','clang','ld.lld','python3']:
   candidates={(Path(d)/name).resolve() for d in os.get_exec_path() if (Path(d)/name).is_file()};result['tools'][name]={'version':subprocess.check_output([name,'--version'],text=True),'sha256':{str(c):sha(c) for c in sorted(candidates)}}
  env=dict(os.environ,CCACHE_DISABLE='1',KCONFIG_ALLCONFIG=str(record/'config'),KCONFIG_DEFCONFIG_LIST='/dev/null',KCONFIG_CONFIG='.config')
  with (record/'build.log').open('w') as log:
   for target in ['allnoconfig','bzImage'] + (['modules'] if has_modules else []):
    cmd=['make','-C',str(source),f'O={build}','ARCH=x86_64','LLVM=1','W=1','-j4',target];result['commands'].append(cmd);save();subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
    if target=='allnoconfig':
     lines=(build/'.config').read_text().splitlines();assert {line for line in text.splitlines() if line.startswith(('CONFIG_', '# CONFIG_'))}<=set(lines),'unsatisfied config'
     assert expected_lines <= set(lines), 'wrong shared-core/consumer linkage'
  names=['.config','vmlinux','arch/x86/boot/bzImage','drivers/misc/ws73/ble.o','drivers/misc/ws73/ble-link.o','drivers/misc/ws73/usb.o','drivers/misc/ws73/usb-startup-test.o','drivers/misc/ws73/usb-io.o','drivers/misc/ws73/boot-loader.o','drivers/misc/ws73/firmware.o','drivers/bluetooth/btws73.o','drivers/bluetooth/btws73_proto.o']
  if has_modules:
   # Old .ko files may survive a configuration switch. modules.order, not
   # file existence, determines which outputs belong to this invocation.
   order = set((build/'modules.order').read_text().splitlines())
   # The kernel's internal modules.order lists .o; modfinal emits .ko.
   assert all(name.endswith('.o') for name in order), 'unexpected module-order format'
   order = {name[:-2] + '.ko' for name in order}
   assert order & set(MODULES.values()) == expected_modules, 'wrong module membership'
   names += ['Module.symvers', 'modules.order'] + sorted(expected_modules)
  for name in names:
   dest=record/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(build/name,dest);result['artifacts'][name]=sha(dest)
  assert git('rev-parse','HEAD')==head and not git('status','--porcelain')
  result['status']='PASS'
 except BaseException as e:result['status']='FAIL';result['error']=repr(e);raise
 finally:save();print(result['status'],flush=True)

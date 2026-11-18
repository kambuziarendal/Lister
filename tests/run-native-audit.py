# Run after make on Haiku with app_server available. Optional args select tests.
# The Open-with test additionally requires its signed synthetic receiver fixture.
from pathlib import Path
import subprocess
src=Path(__file__).resolve().parents[1];out=src/'build/native-audit-tests';out.mkdir(parents=True,exist_ok=True)
objects=list((src/'src/objects.x86_64-cc13-release').glob('*.o'))
objects=[str(p) for p in objects if p.name not in ['App.o','SelfTest.o','VirtualTest.o'] and not p.name.startswith('_')]
# Link actual native implementation, not Linux surrogates.
names=['file-operation-faults','worker-lifecycle','virtual-operation-faults','archive-read-scope','operation-queue','archive-move-scaling','directory-delivery','search-filters','archive-local-move-faults','archive-local-move','same-file-type','folder-compare','archive-editor-faults','archive-editor','archive-move-faults','archive-move','archive-folder-operations','transfer-races','archive-ambiguity','archive-volumes','sevenzip-backend','archive-passwords','archive-jobs','ar-writer','arj-writer','rar-store','cab-writer','lha-writer','preferences-state','settings-window','miller-columns','navigation-history','selection-model','selection-summary','selection-sort','saved-places','saved-searches','file-search','live-search','file-clipboard','clipboard-text-file','file-drop','image-convert','bulk-rename','shelf','named-entry','file-properties','properties-faults','attribute-copy','job-snapshot','icon-cache','audit-io','theme-text']
import sys
if len(sys.argv)>1: names=sys.argv[1:]
for name in names:
 print('TEST',name,flush=True)
 testObjects=[str(src/'src/VirtualOperation.cpp'),str(src/'src/VirtualFile.cpp')] if name=='virtual-operation-faults' else objects
 cmd=['g++','-std=c++17','-O2','-Wno-multichar','-Isrc','-I/boot/system/develop/headers/private/interface','-I/boot/system/develop/headers/private/shared',str(src/'tests'/f'{name}.cpp'),*testObjects,'-lbe','-lshared','-ltranslation','-lcolumnlistview','-ltracker','-o',str(out/name)]
 if name in ['archive-move-faults','archive-editor-faults']: cmd += ['-Wl,--wrap=rename']
 if name=='archive-local-move-faults': cmd += ['-Wl,--wrap=rename','-Wl,--wrap=unlink']
 if name=='file-operation-faults': cmd += ['-Wl,--wrap=mkstemp','-Wl,--wrap=write','-Wl,--wrap=unlink','-Wl,--wrap=_ZN5BNode10RemoveAttrEPKc']
 if name=='clipboard-text-file': cmd += ['-Wl,--wrap=write']
 if name=='archive-move-scaling': cmd += ['-Wl,--wrap=lstat']
 if name in ['directory-delivery','operation-queue']: cmd += ['-Wl,--wrap=_ZNK10BMessenger11SendMessageEP8BMessageP8BHandlerl']
 if name=='folder-compare': cmd += ['-Wl,--wrap=opendir']
 if name=='properties-faults': cmd += ['-Wl,--wrap=_ZN5BNode9WriteAttrEPKcjlPKvm','-Wl,--wrap=_ZN5BNode4SyncEv','-Wl,--wrap=fsync']
 subprocess.run(cmd,cwd=src,check=True)
 subprocess.run([str(out/name)],cwd=src,check=True,timeout=90)
subprocess.run([str(src/'src/objects.x86_64-cc13-release/Listi'),'--self-test'],check=True,timeout=120)
print('NATIVE TESTS COMPLETE',flush=True)

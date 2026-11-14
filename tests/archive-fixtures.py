# SPDX-License-Identifier: MIT
"""Generate synthetic archives with system libarchive (host Python/ctypes)."""
import sys
import ctypes as C, ctypes.util, pathlib, gzip, bz2, lzma, io, tarfile, zipfile, json
r=pathlib.Path(sys.argv[1]);d=r/'fixtures';d.mkdir(parents=True,exist_ok=True);a=C.CDLL(ctypes.util.find_library('archive'))
def fn(name,restype,args):
 f=getattr(a,name);f.restype=restype;f.argtypes=args;return f
ptr=C.c_void_p;i=C.c_int;s=C.c_char_p
new=fn('archive_write_new',ptr,[]);fmt=fn('archive_write_set_format_by_name',i,[ptr,s]);flt=fn('archive_write_add_filter_by_name',i,[ptr,s]);op=fn('archive_write_open_filename',i,[ptr,s]);hd=fn('archive_write_header',i,[ptr,ptr]);wr=fn('archive_write_data',C.c_ssize_t,[ptr,ptr,C.c_size_t]);close=fn('archive_write_close',i,[ptr]);free=fn('archive_write_free',i,[ptr]);err=fn('archive_error_string',s,[ptr]);enew=fn('archive_entry_new',ptr,[]);efree=fn('archive_entry_free',None,[ptr]);path=fn('archive_entry_set_pathname',None,[ptr,s]);size=fn('archive_entry_set_size',None,[ptr,C.c_int64]);mode=fn('archive_entry_set_mode',None,[ptr,C.c_uint]);mtime=fn('archive_entry_set_mtime',None,[ptr,C.c_long,C.c_long])
data=b'Listi archive payload\n'+bytes(range(256))*4
(d/'payload.expected').write_bytes(data)
cases=[]
for format,suffix in [('7zip','7z'),('zip','zip'),('pax','tar'),('newc','cpio'),('arbsd','ar'),('iso9660','iso'),('xar','xar')]:
 for filter in (['none','gzip','bzip2','xz','lzma','lz4','zstd','compress','lzip'] if format=='pax' else ['none']):
  ext={'none':'','gzip':'.gz','bzip2':'.bz2','xz':'.xz','lzma':'.lzma','lz4':'.lz4','zstd':'.zst','compress':'.Z','lzip':'.lz'}[filter]
  name='sample.'+suffix+ext;h=new();e=enew();entry='data.txt' if format=='arbsd' else 'folder/data.txt'
  try:
   for status in [fmt(h,format.encode()),(flt(h,filter.encode()) if filter!='none' else 0),op(h,str(d/name).encode())]:
    if status<0:raise RuntimeError(str(err(h)))
   path(e,entry.encode());size(e,len(data));mode(e,0o100640);mtime(e,1600000000,0)
   if hd(h,e)<0:raise RuntimeError(str(err(h)))
   assert wr(h,data,len(data))==len(data)
   if close(h)<0:raise RuntimeError(str(err(h)))
   cases.append((name,entry,'payload.expected'));print('fixture',name)
  except Exception as x:print('not generated',name,x)
  finally:efree(e);free(h)
for suffix,comp in [('gz',gzip.compress),('bz2',bz2.compress),('xz',lzma.compress),('lzma',lambda x:lzma.compress(x,format=lzma.FORMAT_ALONE))]:
 name='single.txt.'+suffix;(d/name).write_bytes(comp(b'hello\n'));(d/'single.expected').write_bytes(b'hello\n');cases.append((name,'single.txt','single.expected'))
for ext in ['tgz','tbz2','txz','tlz','tzst']:
 source={'tgz':'gz','tbz2':'bz2','txz':'xz','tlz':'lzma','tzst':'zst'}[ext]
 (d/('alias.'+ext)).write_bytes((d/('sample.tar.'+source)).read_bytes());cases.append(('alias.'+ext,'folder/data.txt','payload.expected'))
for ext in ['jar','epub','cbz']:
 (d/('sample.'+ext)).write_bytes((d/'sample.zip').read_bytes());cases.append(('sample.'+ext,'folder/data.txt','payload.expected'))
(d/'empty.txt').write_bytes(b'');(d/'ordinary.txt').write_bytes(b'ordinary file\n');(d/'broken.7z').write_bytes((d/'sample.7z').read_bytes()[:12])
with zipfile.ZipFile(d/'unsafe.zip','w') as z:z.writestr('../outside.txt',b'no')
with tarfile.open(d/'links.tar','w') as t:
 e=tarfile.TarInfo('link');e.type=tarfile.SYMTYPE;e.linkname='/boot/home/outside';t.addfile(e)
with zipfile.ZipFile(d/'unicode.zip','w') as z:z.writestr('mappe/æ ø [x] $;!.txt',data)
cases.append(('unicode.zip','mappe/æ ø [x] $;!.txt','payload.expected'))
(r/'cases.tsv').write_text(''.join('\t'.join(c)+'\n' for c in cases))

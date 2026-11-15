# p7zip interface headers

Unmodified interface headers and their recursive header dependencies from
https://github.com/p7zip-project/p7zip/tree/v17.05 . Copyright Igor Pavlov and
upstream contributors. LGPL 2.1 or later, as specified in License.txt and
copying.txt. No compression/decompression implementation is bundled here.

Listi's MIT adapter loads Haiku's optional p7zip system package at runtime.
The RAR decoder in that package has the upstream unRAR restriction in addition
to LGPL; it is not copied into Listi. These headers and notices are shipped in
the package documentation as well as source. Do not replace them with headers
from a different plugin ABI without native interface and lifecycle tests.

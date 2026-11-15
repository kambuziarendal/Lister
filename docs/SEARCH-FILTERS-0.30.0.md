# File search filters, private 0.30.0 candidate

The DOpus-inspired file-work direction remains primary. This is one small complete
search workflow, not a ROX port, a new search language or historical browsing.

## Use

Open View > Find files (Command+Shift+F, normally Alt+Shift+F). Combine optional
file filters with a literal, Unicode case-insensitive name fragment or an advanced
BFS formula. A blank name is allowed when at least one file filter is active.
A BFS formula must still be non-empty. All conditions are ANDed.

- Extension: case-insensitive suffix such as `pdf`, `.PDF`, or `tar.gz`.
  Not a MIME category, wildcard or content detector. `.pdf` alone is a dotfile,
  not a file with a PDF extension. A suffix needs a nonempty name before its dot.
- Min/max size: inclusive whole byte count, optionally B, KiB, MiB or GiB.
  Binary units are exact; blank means unbounded, `0` is a real bound. Fractions,
  ambiguous MB/GB, negative values, overflow and reversed intervals are refused.
- From/Through: local calendar dates in YYYY-MM-DD, 1970 to 9999. Both calendar
  days are included. Through is implemented as the next local midnight exclusive,
  not a hard-coded 24-hour interval. Blank endpoints are unbounded. Invalid and
  reversed dates fail visibly. Dates use the current process/system timezone.
- Dates use last modification unless Use creation date is checked. Creation
  uses Haiku's native `st_crtime`, not POSIX metadata-change `ctime`.

Active file filters return regular files only, never folders or symlink targets.
Subfolder traversal continues regardless of whether the folder name matches.
With no file filters, the existing name/formula behavior, including matching
folder and link entries, remains available. Clear filters clears only file
conditions, not the name, formula or root. Enter submits from the criteria fields.
Stop/Escape cancels; fields remain locked during a snapshot or live subscription.
The result scope retains the executed filters, with a full tooltip when clipped.

## BFS and live results

Filters are evaluated against the same lstat metadata for recursive and indexed
BFS results. They do not generate or interpolate BFS expressions. Advanced Live
continues to use existing BQuery notifications and bounded three-second repair
scans; post-filter membership changes such as file growth are reconciled too.
No automatic live subscription is claimed for simple recursive name searches.

Limits remain one persistent local volume, 5,000 displayed hits, 250,000 examined
entries, bounded traversal and explicit incomplete/error outcomes. These limits
also apply to BFS candidates rejected by file filters. This is a read-only,
non-transactional view of present files, not proof that a changing tree is frozen.
No search inside archives, remote providers, content search, MIME builder,
relative-date presets, multi-volume search or snapshot support is added here.

## Persistence and downgrade

Saved searches retain the original date/size text and extension, not result rows
or resolved timestamps. Loading resolves calendar dates in the current timezone.
The existing atomic criteria store now writes format 2. Format 1 definitions load
with empty file filters. Invalid or missing format-2 filters are refused, never
silently broadened into a less restrictive search.

0.29 and earlier cannot read format 2. On rollback they explicitly report the
unsupported store, preserve its bytes and disable saved-search changes. Ordinary
file work remains available. Upgrading restores the saved definitions; do not
replace the store with a stale backup to make an old version read it. This is an
intentional compatibility boundary. Main settings and unrelated stores do not
change format. Actual roundtrip evidence belongs in ACCEPTANCE-0.30.0.md.

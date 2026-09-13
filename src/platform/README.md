# Platform adapters

Operating-system and process-boundary adapters belong here. No platform adapter may leak into the domain modules.

## AtomicFileStore (D01)

`Voelund::Platform` depends only on the C++ standard library and OS file APIs.
`AtomicFileStore::write(path, bytes)` takes a caller-owned immutable byte view,
including embedded NUL bytes. It replaces a directory entry, not a document model.
D03 will connect a serialized snapshot and the application's saved-state update.

The operation resolves the target to an absolute path once, creates a sibling with
exclusive creation, writes in bounded chunks, flushes, closes, then replaces the
target. Random names reduce collisions; `CREATE_NEW`/`O_EXCL` establishes ownership.
Only a successfully created temporary file is cleaned up. No scan or wildcard
cleanup occurs. An existing destination is never opened for truncation.

Errors before replacement retain the destination's bytes. Native I/O errors use
`FileStoreError` with a stage and OS error code; allocation/path-resolution failures
may instead throw standard C++ exceptions. Temporary cleanup is best effort if the
directory becomes inaccessible. No throwing work follows a successful replacement.

### Native behavior and limits

- Windows uses wide-character paths, `FlushFileBuffers`, `CloseHandle` and
  `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`. The copy-across-volume option is
  deliberately absent. In-process commits are serialized to avoid observed sharing
  failures between concurrent replacement calls. Other processes may still deny
  delete sharing; that is an error, never a silent successful save.
- POSIX uses mode `0600`, `fsync`, `close` and same-directory `rename`. Interrupted
  writes/flushes retry; interrupted close does not blindly retry a possibly reused
  descriptor. Open readers keep the old inode. Advisory file locks do not make
  rename a mandatory-lock operation. Directory permissions govern replacement.
- The sibling design avoids intentional cross-filesystem moves. A filesystem error
  never activates a copy/delete fallback. Native filesystem semantics still apply;
  network filesystems, power loss and filesystem fault injection are not certified.
  In particular, this version does not fsync the parent directory after rename.
- Replacement creates new file metadata: POSIX mode is `0600` (subject to umask),
  Windows security is inherited at temporary-file creation. Old ACLs, extended
  attributes, hard-link identity and timestamps are not preserved by this byte-store
  contract. D03 must respect this documented metadata policy when binding Save.
- Callers provide a trusted directory and ordinary file target. This is not a
  defense against another process maliciously exchanging parent directories or
  temporary files. Symlink/hard-link destinations are directory-entry replacements,
  not an in-place edit of the referenced inode. No multi-writer merge/conflict check
  is supplied; successful concurrent calls have last-commit-wins semantics.

### Verification

`platform.atomic_file_store` uses an exclusively created isolated fixture directory.
It exercises Unicode directories and filenames, binary/empty payloads, partial write
failure, distinct create/write/flush/close/replace fault boundaries, exclusive-name
collision, foreign temporary-file preservation, directory-target rejection, native
permissions/Windows sharing locks and eight concurrent writers. Fault hooks are an
internal test boundary, not a simulation of failing hardware syscalls. The test must
run unprivileged on POSIX so its permission assertion is meaningful.

API references: [Microsoft MoveFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw),
[FreeBSD rename(2)](https://man.freebsd.org/cgi/man.cgi?query=rename&sektion=2).
The FreeBSD manual was also read directly on the actual test host.

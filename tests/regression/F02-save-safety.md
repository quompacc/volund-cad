# F02 – Existing target can be truncated before save succeeds

- **Reproduction target:** D01/D03 will inject failures before temporary-file
  creation, during write/flush/close and during replace against an existing
  target.
- **Expected invariant:** before successful atomic replacement, the old target
  bytes, document path and saved revision remain unchanged and the document is
  dirty.
- **Planned owning test:** an `application` + `regression` native fixture named
  `application.save_atomicity`.
- **Boundary:** this is prepared metadata, not evidence of a fixed save path;
  `CadSession::saveDocument` still uses the unsafe historical implementation
  until D01–D03.

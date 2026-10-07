# Verified results

## Public pre-release package

The publishable tree includes a standalone C++ installer in addition to the
unchanged runtime and resource generator. Eight offline checks pass: four runtime
checks, two resource checks and two installer checks. The installer CLI fixture
exercises fresh install, repeated install, status, restore, migration from an
existing UI fix, preserving a PS3 config, and refusing altered archives, metadata
or patch data. The transaction test exercises real file writes and rollback after
an injected range-write failure or a blocked DLL replacement.

All 38 XOR patches reproduce the existing payload identities in both directions.
The package retains the exact installed 0.22.3 DLL SHA256
`a2c97811f0548bab3d0060be9bf23b901c68a05b04e29f3bcc4e92c160da7d13`.
The public installer also successfully verified all three real installed archives,
15 metadata files and 38 XOR patches in read-only status mode. It has not been run
in install/remove mode on the working development game. PS3 in-game presentation
and full campaign coverage still require gameplay verification.

## 0.22.3 prompt configuration

Verified offline on 2026-10-07 with Release Win32 runtime and x64 tools builds:

- All 38 Xbox resources and the SHA1 whitelist still reproduce 0.22.2 exactly.
- Original binding-hook code, native pad IDs, all 14 scaling patches and stored
  archive/metadata bytes are unchanged.
- INI reads, case/whitespace handling, absent files/keys, invalid values and the
  Xbox fallback pass tests.
- Two PS3 libraries retain their decoded lengths. Four local assignments and
  one QTE lookup per library are the only edited spans; 73 bytes differ in each.
- Independent AVM1 execution verifies balanced stacks and local `ps3` values.
  All other 264/base and 232/patched classes, constant pools, instruction offsets
  and the shared Win32 platform remain unchanged.
- Both QTE dictionaries are retained verbatim. The PS3 lookup uses the six
  existing Xbox physical-button IDs.
- A synthetic native-ABI constructor is actually detoured in a separate process.
  Tests cover argument/return preservation, both real local movie fixtures,
  source immutability, stable copy lifetime, corrupt data, reused addresses,
  access-fault fallback with lock release, and clean hook removal.
- Proxy export forwarding and original file I/O/signature behavior pass.

Six CTest tests cover the runtime/configuration and resource generation when
private fixtures are supplied. No game binaries or fixture movies are included
in the source project. These tests do not replace a visual/input test inside BF3.

## Original C++ migration

The C++ migration was checked on 2026-10-07 using MSVC 19.44 / Visual Studio 2022,
a Release x86 DLL build, a Release x64 tools build, and zlib 1.3.1.

| Check | Result |
| --- | --- |
| Own C++ code compiled with `/W4 /WX` | Passed |
| Original input/scenario/configuration resource reproduction | 36/36 byte-identical |
| Both compressed PC UI libraries | 2/2 byte-identical |
| Full patched resource SHA256 and SHA1 | 38/38 exact matches |
| Generated runtime whitelist | Text-identical to the checked-in C++ table |
| Local UI assignments | Four per library, eight total |
| UI bytes outside the assignments | Unchanged |
| CAS multi-block roundtrip, size budget, truncation guards | Passed |
| Wrong original-resource identity and missing UI assignment guards | Passed |
| Native pad selection with keyboard/pad mixed vectors and different contexts | Passed |
| Shared Win32 UI guard and truncated/mismatched image rejection | Passed |
| Original UI scaling patch bytes and page-protection restoration | All 14 passed |
| Exact whitelist entries and rejection of near misses/original digests | Passed |
| Isolated proxy `getBuildInfo` forwarding | Passed |
| Synthetic file I/O and original valid/invalid RSA verification | Passed |

Four CTest tests were run: `patch-unit`, `release-parity`, `runtime-unit`, and
`proxy-forwarding`. The standalone C++ CLI also generated a complete output stage.

The existing source project and installed 0.22.2 binary were preserved. The new
DLL was not installed into the game during migration.

These checks establish source/build parity and offline behavior. They do not
exercise runtime detours inside BF3, mission loading, every campaign prompt, or
the game's actual controller processing. In-game verification remains pending.

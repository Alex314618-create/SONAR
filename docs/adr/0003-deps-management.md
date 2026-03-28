# ADR 0003 — Dependency Management: third_party/ git clone

> **Status**: Accepted
> **Date**: 2026-03-06
> **Owner**: CIO Agent
> **Supersedes**: N/A

---

## Context

The original project constitution (CLAUDE.md) specified **vcpkg** as the dependency manager
for SDL2, OpenAL Soft, and cglm. During M1 setup, vcpkg was unusable on the development
machine because vcpkg's built-in `curl` transport conflicted with **Clash Verge** (a
proxy client), causing SSL certificate verification errors on every download attempt.
The errors were not reproducible on a clean machine, making a vcpkg-side fix impractical.

Additionally, the project requires several single-header libraries (cgltf, stb_vorbis,
dr_wav, glad) that vcpkg handles inconsistently or requires extra configuration for.

## Decision

Replace vcpkg with a **manual `third_party/` directory** populated via shallow git clones:

```
third_party/
├── SDL/          # git clone --depth 1 https://github.com/libsdl-org/SDL
├── openal-soft/  # git clone --depth 1 https://github.com/kcat/openal-soft
├── cglm/         # git clone --depth 1 https://github.com/recp/cglm
├── cgltf/        # git clone --depth 1 https://github.com/jkuhlmann/cgltf
└── glad/         # generated output committed directly (single-use, no upstream churn)
```

Each library is integrated via CMake `add_subdirectory()` or `target_include_directories()`
depending on whether it ships a CMakeLists.txt.

`third_party/` is listed in `.gitignore` — developers must run the setup script
(`tools/setup_deps.sh`) after a fresh clone.

The **glad** OpenGL loader is an exception: its generated output (`src/third_party/glad/`)
is committed to the repository because it is a one-time generated artifact with no expected
upstream churn.

## Consequences

### Positive
- **No third-party toolchain required.** CMake + Git is sufficient; no vcpkg install.
- **Proxy-agnostic.** Plain `git clone` respects system proxy settings correctly.
- **Full control.** Library versions are pinned by the cloned commit SHA.
- **Simpler CI.** No vcpkg bootstrap step; a `git clone --recurse-submodules` equivalent
  via the setup script is sufficient.

### Negative
- **Manual version management.** Updating a library requires re-cloning and re-testing.
  There is no `vcpkg upgrade` equivalent.
- **Disk usage.** Each developer maintains a full `third_party/` copy.
- **No automatic security patches.** Vulnerabilities in vendored libraries require
  explicit action from the team.
- **Setup script required.** New contributors must run `tools/setup_deps.sh` rather than
  a single `vcpkg install`. This must be clearly documented in the build guide.

## References

- `CLAUDE.md` Section 3 — Tech Stack
- `CLAUDE.md` Section 8 — Build & Run (to be updated to reflect this decision)
- `.gitignore` — excludes `third_party/` except `third_party/glad/`

---

## Changelog

| Date       | Change                    |
|------------|---------------------------|
| 2026-03-06 | Initial ADR, status Accepted |

# Contributing

## Workflow

- One branch per change: `feature/<name>` (or `refactor/<name>`), created from `dev`.
- Commit regularly, with a prefix: `[ADD]`, `[FIX]`, `[UP]` (change) or `[DEL]`, then a short message.
- Open the pull request against `dev`, never `main`. CI builds Linux and Windows (MSVC, MinGW) and runs the tests: it must be green.
- A PR that depends on another one waits for it to be merged.

## Build and test

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bin/kronk3d_tests          # CPU tests
./build/bin/kronk3d_gl_tests       # OpenGL tests (skipped without an OpenGL 3.3 context)
```

SFML is only needed by the example, the OpenGL tests and the benchmark (`-DK3_BUILD_EXAMPLES=OFF -DK3_BUILD_TOOLS=OFF -DK3_BUILD_GL_TESTS=OFF` builds the library and the CPU tests alone).

New behavior comes with a test: `modules/test/src` for the CPU side, `modules/test/gl` for what only an OpenGL context can check.

## Style

`.editorconfig` and `.clang-format` describe the style (4 spaces, Allman braces for types, functions and namespaces, namespace contents indented).
The existing code was not reformatted: format only what you change, with `git clang-format`, rather than whole files.

- Comments explain the why, not the what. Keep the file header (`KRONK CORP, 2026 ...`) on new files.

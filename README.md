# HexPuzzle3D

HexPuzzle3D is an interactive spherical tile puzzle. Move the pointer away from
the window center to orbit the planet and select a tile. Left-click rotates the
selected tile. Inset navy panels, cyan connection rims, cased path ribbons, and
port nodes replace the original flat white presentation. The independent routes
through the selected tile are highlighted in cyan, magenta, and green; yellow
ribbons bridge connected sides across tile boundaries, and a gold ring marks the
selected tile. The three longest route components of at least four tile paths
remain persistently highlighted in muted violet, blue, and orange beneath the
selected-route overlay. A compact HUD reports selected-route and whole-board
metrics.

Pointer selection uses an eye-space ray intersected against the rendered tile
polygons, so selection follows visible tile boundaries and clicks outside the
planet do not rotate a stale tile.

Each hexagon contains one, two, or three concurrent paths; five-sided topology
tiles contain one or two. Every path is an independent binary connection between
exactly two sides. Generated side pairs never share an endpoint or cross, and
they are drawn as side-to-side surface ribbons. Each ribbon enters and leaves its
sides perpendicularly using a smooth cubic curve; it remains straight only when
one aligned surface line is perpendicular to both sides. Paths are not forced
through the tile center.
External connections continue only into the matching path on the neighboring
side, so separate paths never interact or form multi-way junctions.

Connector layouts come from exhaustive deterministic catalogs rather than
rejection sampling. The pentagon catalog contains 20 non-empty noncrossing
layouts and the hexagon catalog contains 50. Generation first balances path
count and connector span, then evaluates multiple seeded candidate boards using
connected-edge, connected-path, longest-route, isolation, and component metrics.
The highest-scoring candidate becomes the initial puzzle state.

The program was refactored from a procedural GLUT prototype into object-oriented
C++17 while preserving the original spherical mesh, randomized puzzle tiles,
pointer-driven orbiting, selection, rotation, textures, and connection markers.

## Architecture

- `HexPlanet` owns the spherical mesh, topology, subdivision, polygons, nearest
  tile queries, and ray intersections.
- `PuzzleBoard` owns every `PuzzleTile`, deterministic connector catalogs,
  candidate-board scoring, route metrics, rotation, and connection state.
- `OrbitCamera` owns viewport, pointer, view, and selection-ray state.
- `BoundedAdamsBashforthOrbit` owns the projected AB2 predictor, independent
  phase reference, contractive correction, error cap, and periodic re-anchors.
- `Texture2D` and `TextureLibrary` own OpenGL texture resources and PNG loading.
- `HexPuzzleRenderer` owns OpenGL drawing and has read-only model references.
- `HexPuzzleApplication` owns the GLUT lifecycle and routes static callbacks to
  one application instance.
- `DebugLog` appends structured lifecycle and camera diagnostics when enabled.

The core model has no OpenGL dependency and is covered by deterministic tests.
Raw owning pointers, fixed 3,000-element arrays, application globals, SOIL, and
the obsolete OpenEXR include paths have been removed.

## Dependencies

On current Debian or Ubuntu systems:

```bash
sudo apt-get update
sudo apt-get install build-essential pkg-config freeglut3-dev libimath-dev libpng-dev cmake xvfb
```

Equivalent packages are available on other Linux distributions. OpenGL, GLU,
FreeGLUT, Imath, and libpng must expose `pkg-config` metadata.

## Build

Using Make:

```bash
make
make test
```

Using CMake:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

Run from the repository root so the bundled PNG assets are found:

```bash
./hexp_main
```

Options:

```text
--seed N             reproduce planet and puzzle randomization
--assets PATH        load PNG assets from PATH
--subdivisions 0..5  choose mesh detail; the original used 4
--debug-log PATH     append structured JSONL diagnostics to PATH
--smoke-test         render one frame and exit (for headless validation)
```

Headless smoke test:

```bash
xvfb-run -a ./hexp_main --seed 1 --assets . --subdivisions 2 --smoke-test
```

Capture lifecycle, resize, rotation, periodic camera-health, and detected camera
basis-discontinuity events:

```bash
./hexp_main --debug-log /tmp/hexpuzzle-debug.jsonl
```

Every line is independently flushed using the `hexpuzzle.debug_event.v1` JSONL
schema so diagnostics survive an unexpected process exit.

When a stationary off-center pointer keeps the camera rotating in one direction,
the application compresses consecutive tile selections and checks the resulting
sequence for a repeated cycle. A confirmed cycle displays `REPEAT` in green and
emits `tile_sequence_repeat_started` to the debug log. Moving the pointer or
stopping the orbit resets the sequence check.

Stationary orbit orientation uses a bounded second-order Adams-Bashforth design.
AB2 predicts a quaternion, normalization projects it back onto the rotation
group, and shortest-path quaternion interpolation contracts prediction error
toward an independent phase-locked reference. Each completed revolution
re-anchors both the orientation and the previous AB history. A `1e-6` radian
runtime cap fails closed to the independent reference if correction ever exceeds
the configured bound. This preserves exact repeated revolutions while retaining
an observable Adams-Bashforth predictor. `REPEAT` is shown only after two
complete matching tile cycles.

The numerical error dynamics and an Adams-Bashforth comparison are documented
in `docs/ORBIT_ERROR_BOUND_MODEL.md`. Reproduce the model with
`python3 tools/orbit_error_model.py`.

## Governed Rewrites

The refactor was preceded by a read-only OURD provider preflight and repository
inspection bound to the original Git commit and filesystem snapshot. The exact
evidence, ownership mapping, fail-closed model limits, and behavior-preservation
decisions are recorded in `docs/OURD_REFACTOR.md`.

The later visual and algorithmic rewrite also used OURD in read-only advisory
mode. Its accepted recommendations, corrected model claims, provider evidence,
and deterministic validation contract are recorded in
`docs/OURD_VISUAL_ALGORITHMIC_REWRITE.md`.

## License

See `LICENSE`. The repository retains its original
Attribution-NonCommercial-NoDerivatives 4.0 International license text.

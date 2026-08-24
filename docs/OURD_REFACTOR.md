# OURD-Governed Object-Oriented Refactor

## Source Identity

- Repository: `https://github.com/pghauff55/hexpuzzle3d`
- Original Git commit: `12ff125053b19db7db2ad83cc5252dc9eac89d0e`
- Original OURD workspace snapshot:
  `2f1e9946208572d45f383b10e1a77b8aef9b8117941586e10605b6893719ce91`
- Refactor date: 2026-08-24, Australia/Brisbane

## OURD Use

The local OURD coding agent was run against the exact clone in default read-only
mode before implementation. Provider preflight reported `ready` for
`qwen3.8-27b-fast`, model digest
`07cb98f8840ce491fc28c04a5ecc13c4dec5fd23d9a4732878bc3c02acb5b005`,
Q3_K_S quantization, and the local Ollama Responses endpoint.

Broad model-assisted analysis failed closed rather than silently truncating:

- estimated 8,658 tokens exceeded the configured 6,000-token budget;
- estimated 12,867 tokens exceeded the configured 12,000-token budget;
- an 8,552-token request exceeded the model runtime's 8,192-token context;
- a later focused attempt exhausted its deliberately small step limit, and a
  retry observed a provider disconnect.

Those failures are retained in `.ourd-agent/events.jsonl` locally and are not
treated as successful architectural approval. OURD was used for bounded,
read-only inspection and fail-closed governance evidence; the direct user task
remained the authority for this working-tree refactor. No OURD mutation authority
was invented, and `.ourd-agent/` is excluded from Git.

After deterministic implementation and validation, a final no-tool OURD review
was supplied the exact class, dependency, grep, build, test, sanitizer, and
headless-render facts. It returned advisory `APPROVE_WITH_LIMITS`, finding single
ownership, no raw allocation, OpenGL confinement, and layered dependencies. Its
remaining unknowns were cross-platform GLUT behavior, uninspected texture
lifecycle paths beyond the supplied facts, and files outside the declared audit
surface. This verdict is supporting analysis, not human approval or certification.

## Ownership Decision

| Original responsibility | Object-oriented owner |
| --- | --- |
| `m_hexes`, `m_hexdual`, subdivision, polygon and neighbor queries | `HexPlanet` |
| raw `tileNode*` list, ID/index maps, rotation and connection flags | `PuzzleBoard` and `PuzzleTile` |
| recursive global path generation | `PuzzleBoard::buildTraversal` |
| camera basis, mouse position, pointer selection direction | `OrbitCamera` |
| SOIL texture IDs and global texture arrays | `Texture2D` and `TextureLibrary` |
| `glut_Display`, tile drawing, selection and number quads | `HexPuzzleRenderer` |
| global GLUT callbacks and startup | `HexPuzzleApplication` |
| procedural `main` setup | option parsing plus one application object |

## Preserved Behavior

- sqrt(3)-style triangle-center subdivision of the icosahedral sphere;
- twelve pentagonal tiles with the remaining tiles hexagonal;
- randomized traversal, tile type, and tile rotation;
- the original five six-side connection patterns;
- pointer-driven orbit and nearest-tile selection;
- left-click rotation of the selected non-empty tile;
- PNG puzzle textures, selected-tile outline, activation borders, and numbered
  connection markers.

## Correctness Repairs

- Mesh connectivity uses stable triangle indices instead of pointers into
  replaceable vectors.
- Puzzle tiles are stored by value and indexed directly by immutable tile ID.
- Every tile receives an initialized, ordered neighbor list; the original start
  tile could read uninitialized adjacency storage.
- Bounds-checked vectors replace fixed six-side and 3,000-tile C arrays.
- `rayHitPlanet` now performs a real ray-sphere intersection instead of returning
  `true` without setting its result.
- Imath uses its current include path, and libpng replaces unavailable SOIL.
- Model logic is buildable and testable without a window or OpenGL context.

## Validation Contract

Completion requires both build systems, deterministic core tests, a headless
render smoke test using the bundled assets, strict warning compilation, clean
dynamic dependencies, and a final source/status audit. A successful focused test
alone is not a completion claim.

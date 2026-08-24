# OURD-Governed Visual and Algorithmic Rewrite

## Scope and Authority

The user requested that the local OURD coding agent be used to rewrite
HexPuzzle3D visually and algorithmically. OURD was run in read-only advisory mode
against the exact working tree before integration. It did not receive mutation
authority, prepare a transaction, apply files, or approve its own proposals.
The direct user request remained the authority for the implementation and the
deterministic build, test, and visual gates remained authoritative.

- Date: 2026-08-24, Australia/Brisbane
- Pre-integration OURD workspace snapshot:
  `f5c9bec68818e8f77d9603a16aafc6dd1e692e856ca23c5def21448862f57f3b`
- Provider: local Ollama Responses endpoint
- Model: `qwen3.8-27b-fast`
- Model digest:
  `07cb98f8840ce491fc28c04a5ecc13c4dec5fd23d9a4732878bc3c02acb5b005`
- Quantization: `Q3_K_S`
- Visual advisory run: `5513bb33-e83a-432b-b6af-055e72cedb8b`
- Algorithm advisory run: `664ecc57-2613-4b50-8584-ee6597e651e8`

The complete append-only trace is retained locally in
`.ourd-agent/events.jsonl`. The extracted visual and algorithm advisory outputs
had SHA-256 hashes
`7f2323c8b28508f15dbd2a296a7de7effb5595e9fda91c14e469b61c2a15bafc`
and
`ae3b216269931cb5f1327b74b8a2f9a6f3e33c2328c6c592776223ace687b79f`.

## Fail-Closed Model Use

Broad tool-using visual attempts exceeded configured context budgets by small
and large margins. Several algorithm attempts also exceeded their budgets, and
one deliberately constrained retry produced a malformed local tool call. OURD
reported these failures instead of silently truncating or inventing success.
The successful advisories therefore used exact facts extracted from the current
code and bounded governance-then-answer runs within the verified runtime limit.

## Accepted Visual Direction

- Replace flat white tiles and red inactive borders with dark inset panels,
  subtle beveled shells, and restrained cyan connection rims.
- Render every internal path as a dark casing plus a readable colored core with
  explicit endpoint nodes.
- Highlight the selected tile's independent routes with a fixed cyan, magenta,
  and green palette.
- Draw connected sides as yellow cross-edge ribbons with dark under-casing.
- Use a gold selection ring so selection is not confused with route identity.
- Replace the single orange status line and large digit quads with a compact
  translucent HUD and selected-port indicators.
- Preserve exact polygon-ray picking and the existing renderer ownership/API
  boundary.

The implementation goes beyond fixed-width OpenGL lines. Tile-internal paths use
deterministic cubic side-to-side curves whose endpoint tangents are perpendicular
to the entry and exit edges. A path remains a straight surface line only when
that line is perpendicular to both sides. The sampled points are projected onto
the spherical surface and expanded into triangle-strip ribbons, avoiding both
face clipping from straight 3D chords and driver-dependent gaps at thick joins.

## Accepted Algorithmic Direction

- Enumerate every valid non-empty noncrossing partial matching for five- and
  six-sided tiles once, in stable lexicographic order.
- Select path count uniformly, then select the maximum connector span uniformly,
  then select a matching uniformly within that cell using a fixed bounded draw
  from `std::mt19937`.
- Generate multiple complete seeded board candidates and retain the strongest
  measured candidate instead of accepting the first independent random board.
- Traverse the complete connector graph at `(tileId, pathIndex)` granularity and
  expose cached board metrics: total paths, connected paths, connected edges,
  route components, longest route, isolated paths, and quality score.
- Recompute connection state and metrics after every rotation.

The generator remains deterministic for the same seed and settings. Candidate
selection is linear in the number of candidates times the number of tile paths
and physical adjacency edges.

## Corrected Advisory Claims

OURD's algorithm advisory was useful but not accepted verbatim:

- The old per-tile rejection loop was bounded to 256 attempts, so it could fail
  but could not run forever.
- Noncrossing partial matchings on five and six labeled boundary sides follow
  Motzkin counts, not the large Schroeder counts stated by the advisory. After
  excluding the empty matching, the exact catalogs used here contain 20
  pentagon layouts and 50 hexagon layouts. Tests further require the exact
  per-path-count distributions: `10/10` for pentagons and `15/30/5` for
  hexagons.
- Rotation is a required puzzle action and remains supported; the advisory's
  accidental "no rotations" invariant was rejected.
- Fixed-function OpenGL has no portable round line-cap setting. Explicit ribbon
  geometry and endpoint discs provide deterministic caps instead.

## Validation Contract

Completion requires all of the following from the final working tree:

- strict-warning Make build and deterministic core tests;
- CMake configure/build and CTest;
- headless Xvfb smoke render;
- visual capture of a selected three-path tile showing three independent routes,
  noncrossing internal ribbons, yellow cross-edge bridges, gold selection, and
  readable HUD metrics;
- source-format and status audit without treating unrelated dirty files as part
  of the rewrite.

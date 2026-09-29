# Release checklist

[日本語](RELEASE_CHECKLIST.ja.md)

What to confirm before releasing `ESP32KeyBridge`. Version bumps and releases follow the shared [arduino-library-release-toolkit](https://github.com/tanakamasayuki/arduino-library-release-toolkit); `tools/bump_version.py` and `.github/workflows/release.yml` are **not** edited in this repository (fix them in the toolkit and distribute with `sync_release_assets.py`).

## Documentation

- `docs/SCOPE.md` contains only features explicitly agreed in ChatGPT and accurately marks their implementation status.
- Public support claims and release validation name ESP32-S3 as the only supported target.
- The feature, adapter, and compatibility tables in `README.md` / `README.ja.md` match the implementation.
- The status column in `docs/ADAPTERS.ja.md` (**implemented, hardware-verification pending** / **implemented, hardware-verified** / **out of scope**) matches reality.
- The lists in `examples/README.md` / `examples/README.ja.md` match `examples/practical/` (checked automatically by `tests/unit/test_repository_structure.py`).
- The gap between `docs/REQUIREMENTS.ja.md` and what is implemented can be explained.
- The core / adapter / example boundary and the callback-context rules in `docs/CORE_DESIGN.ja.md` match the implementation.
- `docs/CONFIGURATION.ja.md` remains clearly marked as out-of-scope historical material.
- `docs/DEVELOPMENT_PLAN.ja.md` reflects the closed agreed scope and does not invent remaining work.
- `docs/DECISIONS.ja.md` records the design decisions settled since the previous release.
- `tests/README.ja.md` / `tests/TEST_PLAN.ja.md` match the current test layout and coverage.

## Metadata

- `library.properties` `name`, `sentence`, `paragraph`, `architectures`, and `includes` match what is published.
- `keywords.txt` covers the public classes, methods, and constants (add entries when adapters or APIs are added).
- The minimum dependency versions (EspUsbHost / EspUsbDevice / EspBle) agree across the READMEs, `docs/ADAPTERS.ja.md`, and `examples/**/sketch.yaml`.
- `CHANGELOG.md` `## Unreleased` covers everything going into this release (the bump moves that section under the new version).

## Tests

The two hardware-free suites also run in CI (`.github/workflows/tests.yml`), but run them locally before a release:

```sh
cd tests
uv run pytest unit/ examples_compile/
```

Run the hardware suites for whatever is connected and reflect the outcome in `tests/TEST_PLAN.ja.md`:

```sh
uv run --env-file .env pytest peer/
uv run --env-file .env pytest single/
```

Keep the `manual/` procedures in `tests/manual/README.md` and do not mix them into the automated pass criteria.

## Releasing

1. Settle the contents of `## Unreleased` and the bump level (major / minor / patch).

   ```sh
   python tools/bump_version.py --preview
   ```

2. Run the `Release` GitHub Actions workflow (workflow_dispatch). The shared workflow:
   - updates the version in `library.properties`, moves `CHANGELOG.md` `## Unreleased` into a new version section, and regenerates `src/esp32keybridge_version.h`;
   - commits and pushes the default branch;
   - recreates the `release` branch (rewriting `dir: ../../../` in `examples/**/sketch.yaml` to `ESP32KeyBridge (<version>)` and removing `tests/`);
   - creates the tag, the ZIP, and the GitHub release.

3. Confirm the final diff contains no build artifacts, caches, or local profile changes.

## After the release

- Confirm the new version appears in Arduino Library Manager (it can take a few hours).
- Confirm `examples/**/sketch.yaml` still uses `dir:` references on the default branch — the rewrite belongs to the `release` branch only.

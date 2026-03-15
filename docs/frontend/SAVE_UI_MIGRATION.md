# Save Frontend Migration Status

## Goal

Replace the old `MainUI`-driven frontend with the lightweight `ui/saves` frontend layer, while preserving the existing save, backup, zip, and Dropbox logic.

This is not a visual reference exercise. The goal is a lightweight GUI that keeps the useful parts of the Sphaira-style structure without carrying over heavy runtime features.

- menu framework
- grid/list rendering
- sidebar actions
- focus/navigation model
- image/icon loading
- simple sidebar actions
- low-memory icon loading

## Direction

Use this split:

- Frontend shell: `ui/saves`
- Save backend: current project logic (`SaveManager`, `ZipArchive`, `Dropbox`)
- Glue: adapter layer consumed by the save menu frontend

The old `MainUI` path has been removed from the active product path and deleted from the tree.

## Current status

Implemented:

- `source/main.cpp` now boots the lightweight `SaveShell` frontend by default.
- `MainUI` is no longer part of the build or runtime path.
- Dropbox revision listing is title-scoped and no longer recursively loads the full remote tree.
- `SaveShell` icon loading now uses a bounded cache and thumbnail-sized textures.
- Save list and revision list rendering only draw visible entries.

Remaining cleanup candidates:

- Keep new work inside `ui/saves`; do not reintroduce a large monolithic frontend class.
- Trim unused translation keys and legacy docs.
- Add real on-device memory measurements to tune icon cache size.

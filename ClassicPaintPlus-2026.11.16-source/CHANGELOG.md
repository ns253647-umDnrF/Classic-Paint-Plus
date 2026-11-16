# Changelog

## 2026.11.16

Initial packaged release of Classic Paint Plus, based on ReactOS Paint / RPaint.
The README and offline manual describe the full set of Plus features.

Final fixes included in this release:

- Ctrl+wheel zoom is constrained to the natural canvas scroll range: no blank
  space is introduced above or left of the image. Pointer anchoring is retained
  wherever the scroll bounds allow it; images that fit stay at the top left.
- Canvas expansion uses the background colour, or transparency when alpha editing
  and Replace canvas background are enabled. Canvas dialogs retain explicit fill
  choices; their initial choice follows the same policy.
- Classic palette is first and the default; Modern is renamed RPaint palette.
- Regular five-point stars have equal edges and aligned pentagram diagonals.
- Recent-file entries are legible after repeated light/dark theme changes.
- Dark-mode bucket paint is white; filtering icons have light outlines.
- All 30 language variants were audited for missing strings, resource encoding,
  format placeholders and accelerator conflicts. Shortcut labels match bindings.
- Russian and English manuals cover the final interface; author sections and
  the logo banner were removed from the manuals.
- Compound selections, vector clipboard, rotate/skew handles, grouped figures,
  pixel corner radii and bottom-panel parameter snapshots are documented.
- Zoom retains editable objects; Ctrl+wheel anchors the image under the cursor,
  and the status bar always shows the zoom percentage.
- Text angles follow S/R presets. Unsaved angles do not leak into the next box.
- Empty text frames immediately fit the selected font unless height is specified.
- A rotated text frame has one rotated caret, without a second upright caret.
- Changing the background colour updates an active semi-transparent selection
  immediately, including subsequent opacity changes.
- PNG transparency backdrops load from the standard bKGD chunk even when the
  image contains no Classic Paint Plus service metadata.
- Both x86 and x64 builds use the static runtime and bundled static codecs.

Earlier development also corrected codec buffer bounds, failed save/load state,
clipboard failures, selection transform bounds, GDI leaks and theme contrast.

Known limitations: no layers or animation editing; page-setup choices are not
fully applied by the inherited printing implementation. See README.md.

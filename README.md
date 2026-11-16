# Classic Paint Plus

A portable Windows bitmap editor with the familiar classic Paint interface and
additional editing tools. Based on **ReactOS Paint / RPaint**.

Classic Paint Plus development: **Nicolay S.**

## Download and run

Download a Windows archive from this repository's **Releases** page:

| Archive | Use |
| --- | --- |
| `ClassicPaintPlus-2026.11.16-windows-x64.zip` | 64-bit Windows 10 or Windows 11 |
| `ClassicPaintPlus-2026.11.16-windows-x86.zip` | 32-bit Windows 10; also runs on x64 Windows |

Extract the entire archive to a writable folder and run `ClassicPaintPlus.exe`.
No installer, Visual Studio, or Visual C++ Redistributable installation is needed.
The runtime and image codecs are linked statically; Windows supplies system DLLs.
These builds do not support Windows XP, Vista, 7, or 8/8.1.

Settings are created in `config.ini` beside the executable. The application does
not import settings from the registry. Release archives contain no personal
settings or recent-file history. To update, close the application and extract the
new archive over its folder, retaining your existing `config.ini`.

Open **Help → Help topics** for the illustrated offline manual. Russian and
English manuals are included beside the executable; keep both HTML files there.

## Features

- Classic toolbox and palette, light/dark themes, 100/150/200/250% panel sizes,
  adjustable canvas handles and 30 interface language variants with live switching.
- Compound selections with Shift to add and Alt to subtract, selection inversion,
  rotation/skew handles, opacity and nearest/bilinear/bicubic resampling.
- Editable shapes in line, polygon, star, arrow and callout groups; regular stars,
  corner radii in pixels, star bend and consistent stroke widths. Copy shapes as
  editable objects within Classic Paint Plus or as pixels into other editors.
- Exact dimensions, rotation and skew on the bottom panel, optional parameter
  fixation and per-shape S/R snapshots of both values and fixation states.
- Text at any zoom, recent fonts, paragraph alignment, editable rotated frames,
  numeric dimensions and angles, S/R presets, and 15 text-warp styles.
- Configurable brush/eraser shapes, size and hardness; classic and additional
  spray patterns with smoothing controls.
- Canvas cropping by selection or selection corner, signed edge offsets,
  anchored canvas resizing, and background-colour or transparent canvas extension.
- Real alpha-channel editing, a configurable transparency backdrop, colour-key
  selection transparency, palette dialogs with canvas sampling, and colour swaps.
- PNG, JPEG, BMP, WebP, ICO, TGA, DDS, TIFF and GIF support. PNG optimisation and
  interlacing; JPEG options; 1/4/8/24-bit BMP export; DDS DXT1/DXT3/DXT5 options.
- Optional Classic Paint Plus service metadata in supported alpha-capable formats.
  Standard PNG background colour is restored without requiring private metadata.
- Cursor-anchored Ctrl+wheel zoom, permanent zoom status, and zoom changes that
  preserve the active selection, figure and text area.

The Classic palette is first and selected by default; the other sets are RPaint
palette, Grayscale and Monochrome. Existing saved palette choices are retained.

## Scope

This is a raster editor. Commit text and shapes to turn them into image pixels;
they are editable as objects while their frame remains active. It is not a layer
editor or an animation editor. Animated WebP imports its first frame. ICO editing
supports selecting a frame and preserving other frames when saving.

The inherited page-setup settings are not fully applied by the printing path.
Windows 10 x64 was used for release validation; Windows 11 was not separately
tested for this release. Release binaries are not Authenticode-signed.

## Source and credits

The release includes `ClassicPaintPlus-2026.11.16-source.zip` with application
source, resources, vendored codec source and build instructions. The source
archive accompanies both Windows archives.

Based on ReactOS commit `447673979ce8a19188628cac6053a5d20c368237`,
[ReactOS Paint](https://github.com/reactos/reactos/tree/447673979ce8a19188628cac6053a5d20c368237/base/applications/mspaint).
Original contributors include Benedikt Freisen, Katayama Hirofumi MZ,
Stanislav Motylkov and other ReactOS contributors. See [AUTHORS.txt](AUTHORS.txt).

The Paint code retains **LGPL-2.0-or-later** licensing. See [COPYING](COPYING),
[NOTICE.txt](NOTICE.txt), [JPEG-NOTICES.txt](JPEG-NOTICES.txt) and
[IMAGE-NOTICES.txt](IMAGE-NOTICES.txt) for component notices. This modified
application is an independent project, not an official ReactOS or Microsoft release.

After successfully opening an image, the application registers its supported file
types in Windows Open With for the current user. It does not change the default
application; editor preferences remain in `config.ini`.

# Classic Paint Plus 2026.11.16

Classic Paint Plus brings the familiar classic Paint interface together with
editable shapes and text, rotation, alpha-channel editing and additional image
formats. Based on ReactOS Paint / RPaint. Classic Paint Plus development: **Nicolay S.**

## Downloads

| Asset | Contents |
| --- | --- |
| `ClassicPaintPlus-2026.11.16-windows-x64.zip` | Portable app for Windows 10/11 x64 |
| `ClassicPaintPlus-2026.11.16-windows-x86.zip` | Portable app for Windows 10 x86 and x64 Windows |
| `ClassicPaintPlus-2026.11.16-source.zip` | Corresponding source, resources, codec source and build instructions |
| `SHA256SUMS.txt` | SHA-256 checksums for all three archives |

Extract the entire Windows archive and run `ClassicPaintPlus.exe`. No installer
or Visual C++ Redistributable installation is needed. Settings stay in `config.ini`
beside the EXE; existing settings are retained when updating. Windows XP/7/8 are
not supported by these builds. Offline English and Russian manuals are included.

## Highlights

- Compound selections with Shift/Alt, inversion, rotation, skew, opacity and
  three resize interpolation modes.
- Grouped editable figures, vector clipboard, pixel corner radii and star bend.
  Exact dimensions, angles and fixation states on the bottom panel; S/R snapshots.
- Editable text, 15-degree Shift snapping, alignment and text-warp styles.
- Flexible canvas cropping and expansion with background-colour or transparent fill.
- Alpha editing with a configurable backdrop, colour-key selection transparency,
  and a colour picker that can sample the canvas while its dialog stays open.
- Custom brushes and erasers; classic and additional spray patterns.
- PNG optimisation/interlacing, JPEG options, 1/4/8/24-bit BMP export,
  WebP/ICO/TGA/DDS/TIFF, and DDS DXT compression options.
- Light/dark themes, adjustable panel and marker sizes, 30 interface language
  variants and portable INI settings.
- Cursor-anchored Ctrl+wheel zoom and a permanent zoom indicator; changing zoom
  preserves editable selections, figures and text.

## Final fixes

Ctrl+wheel zoom no longer detaches the canvas from the top-left workspace corner.
Pointer anchoring respects the normal scroll bounds, including when zooming out
until the image fits the window.

Text-angle presets now follow S/R, empty text frames immediately fit the font
unless height was explicitly specified, and rotated text no longer displays a
second upright caret. Background colour changes immediately affect active
semi-transparent selections. PNG backdrops restore from standard bKGD data even
without private Classic Paint Plus metadata.

The Classic palette is now first and the default; Modern is called RPaint palette.
Canvas growth follows the renamed Replace canvas background setting. The default
five-point star has regular geometry. Recent-file labels, bucket paint and
filtering icons have improved dark-theme contrast. All 30 language variants were
checked for resource completeness, encoding, format parameters and shortcut
conflicts. The revised manuals contain no author section or logo banner.

Known limitations: no layers or animation editing; inherited page-setup choices
are not fully applied when printing. Validated on Windows 10 x64, including the
x86 build; Windows 11 was not separately tested. EXEs are not Authenticode-signed.

The Paint code retains LGPL-2.0-or-later licensing; third-party notices and
corresponding source are included in the release assets. See the source README.

---

## По-русски

Первый подготовленный выпуск **Classic Paint Plus** — редактора на основе
ReactOS Paint / RPaint. Разработка Classic Paint Plus: **Nicolay S.**

Скачайте архив **x64** для 64-разрядной Windows 10/11 или **x86** для 32-разрядной
Windows 10. Распакуйте его целиком и запустите `ClassicPaintPlus.exe`. Установка
не нужна; библиотеки C++ и кодеки встроены. Настройки остаются в `config.ini`
рядом с EXE. Windows XP/7/8 эти сборки не поддерживают.

Возможности: составное выделение с Shift/Alt, поворот, наклон и прозрачность,
сглаживание при изменении размеров, редактируемые фигуры и текст, точные параметры
на нижней панели с фиксацией и S/R, векторный буфер обмена, деформация текста,
обрезка и наращивание холста, альфа-канал и подложка, настройки кисти/ластика,
дополнительные распылители, светлая/тёмная темы, масштаб панелей и новые форматы.

Исправлены сохранение угла текста кнопками S/R, начальная высота текстовой области,
лишний вертикальный курсор при повороте, обновление фонового цвета у активного
полупрозрачного выделения и загрузка подложки PNG без наших служебных данных.

Ctrl+колесо больше не сдвигает холст от левого верхнего края: привязка к курсору
ограничена обычными границами прокрутки, без лишнего пространства слева и сверху.

Новая область холста заполняется фоновым цветом либо прозрачностью при включённых
альфа-канале и «Замещать фон на холсте». «Классическая» палитра стоит первой и
выбрана по умолчанию; «Современная» переименована в «Палитра RPaint».
Уточнена геометрия пятиконечной звезды. Исправлена читаемость недавних файлов,
краски на иконке заливки и кнопок фильтрации в тёмной теме. Проверены ресурсы
30 языков и горячие клавиши. Справка обновлена; раздел об авторах и логотип удалены.

В архиве есть иллюстрированная справка на русском и английском. Исходники
распространяются отдельным архивом вместе с этим выпуском. Для проверки скачанных
архивов приложен `SHA256SUMS.txt`.

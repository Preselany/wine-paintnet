#!/usr/bin/env bash
# Focused COM, resource, and effect-rendering regressions.
set -euo pipefail
here="$(cd -- "$(dirname -- "$0")" && pwd)"
work_root="$(realpath -e -- "${1:-$here/work}")"
if [[ $# -gt 0 ]]; then shift; fi
if [[ $# == 0 ]]; then set -- glyph_clip rectangle_stroke glyph_bitmap dc_background png_metadata pixel_color_context primitive_combine float_conversion conflict queue wic_target command_data font_cache alpha_format composite flood curve_bounds crop wic_upload half_format invert offset_transform ellipse_widen feature_level feedback composition path_combine widen contained_combine gradient_stops white_level effect_identity effect_context effect_node graph_fanout alpha_conversion histogram opacity_metadata alpha_mask convolve_matrix contrast bitmap_source emboss opacity draw_transform draw_input command_list_state color_context timeline keyframes uianimation requirements coremessaging; fi
export WINEPREFIX="$work_root/prefix" WINEDEBUG="${WINEDEBUG:--all}"
export DXVK_LOG_LEVEL="${DXVK_LOG_LEVEL:-error}"
export WINEDLLOVERRIDES='dcomp=b;coremessaging=b;d2d1=b;uianimation=b;wined3d=b;d3dcompiler_47=b;d3d11=n;d3d10core=n;dxgi=n;mshtml='
result=0
for test_name in "$@"; do
    module=d2d1
    if [[ "$test_name" == png_metadata || "$test_name" == pixel_color_context || "$test_name" == half_format || "$test_name" == alpha_format || "$test_name" == float_conversion ]]; then module=windowscodecs; fi
    if [[ "$test_name" == font_cache || "$test_name" == glyph_bitmap || "$test_name" == glyph_clip ]]; then module=dwrite; fi
    if [[ "$test_name" == feedback ]]; then module=user32; fi
    if [[ "$test_name" == composition ]]; then module=dcomp; fi
    if [[ "$test_name" == coremessaging ]]; then module=coremessaging; fi
    if [[ "$test_name" == conflict || "$test_name" == queue || "$test_name" == timeline || "$test_name" == keyframes || "$test_name" == uianimation ]]; then module=uianimation; fi
    if [[ "$test_name" == requirements || "$test_name" == reflection ]]; then module=d3dcompiler_47; fi
    if ! "$work_root/wine/opt/wine-devel/bin/wine" \
        "$work_root/build/dlls/$module/tests/x86_64-windows/${module}_test.exe" "$test_name"; then
        result=1
    fi
done
exit "$result"

/* Wrapped glyphs must not write beyond the run's allocated ink bounds.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <limits.h>
#include "dwrite_2.h"
#include "wine/test.h"

struct texture { RECT bounds; UINT size; BYTE *data; };
static IDWriteFactory2 *factory;
static IDWriteFontFace *face;

static IDWriteGlyphRunAnalysis *analyze(UINT16 *glyphs, UINT count, FLOAT position,
        BOOL transform, DWRITE_RENDERING_MODE mode, DWRITE_TEXT_ANTIALIAS_MODE aa)
{
    DWRITE_MATRIX matrix = {1,0,0,1,0,transform ? position : 0};
    FLOAT advances[2] = {0,0};
    DWRITE_GLYPH_RUN run = {0};
    IDWriteGlyphRunAnalysis *analysis = NULL;
    HRESULT hr;
    run.fontFace = face;
    run.fontEmSize = 32;
    run.glyphCount = count;
    run.glyphIndices = glyphs;
    run.glyphAdvances = advances;
    hr = IDWriteFactory2_CreateGlyphRunAnalysis(factory, &run, &matrix, mode,
            DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DISABLED, aa,
            0, transform ? 0 : position, &analysis);
    ok(hr == S_OK, "CreateGlyphRunAnalysis %#lx.\n", hr);
    return analysis;
}

static BOOL texture(IDWriteGlyphRunAnalysis *analysis, DWRITE_TEXTURE_TYPE type, struct texture *out)
{
    UINT64 width, height;
    UINT i;
    HRESULT hr;
    memset(out, 0, sizeof(*out));
    hr = IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(analysis, type, &out->bounds);
    ok(hr == S_OK, "GetAlphaTextureBounds %#lx.\n", hr);
    if (FAILED(hr)) return FALSE;
    if (!IsRectEmpty(&out->bounds))
    {
        width = (INT64)out->bounds.right - out->bounds.left;
        height = (INT64)out->bounds.bottom - out->bounds.top;
        if (width * height > 65536) { ok(0, "Unexpected large ink bounds %s.\n", wine_dbgstr_rect(&out->bounds)); return FALSE; }
        out->size = width * height * (type == DWRITE_TEXTURE_CLEARTYPE_3x1 ? 3 : 1);
    }
    out->data = malloc(out->size + 32);
    memset(out->data, 0xcc, out->size + 32);
    hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, type, &out->bounds, out->data, out->size);
    ok(hr == S_OK, "CreateAlphaTexture %#lx for %s.\n", hr, wine_dbgstr_rect(&out->bounds));
    for (i = out->size; i < out->size + 32; ++i) ok(out->data[i] == 0xcc, "Overrun at %u.\n", i);
    return SUCCEEDED(hr);
}

static void test_wrapped_glyph(void)
{
    static const FLOAT positions[] = {0, -2147483648.0f, 2147483648.0f};
    static const DWRITE_RENDERING_MODE modes[] = {
        DWRITE_RENDERING_MODE_ALIASED, DWRITE_RENDERING_MODE_NATURAL,
    };
    UINT32 codepoint = 'A';
    UINT16 glyphs[2] = {0,0};
    UINT pos, transform, mode, aa, order, x, y, c, changed, pixel_size, source_index, dest_index;
    LONG normal_y;
    RECT expected;
    IDWriteGlyphRunAnalysis *analysis;
    struct texture valid, combined, baseline;
    DWRITE_TEXTURE_TYPE type;
    HRESULT hr;
    hr = IDWriteFontFace_GetGlyphIndices(face, &codepoint, 1, glyphs);
    ok(hr == S_OK && glyphs[0] != 0, "Test glyph lookup %#lx, %u.\n", hr, glyphs[0]);
    for (pos = 0; pos < ARRAY_SIZE(positions); ++pos)
    for (transform = 0; transform < 2; ++transform)
    for (mode = 0; mode < ARRAY_SIZE(modes); ++mode)
    for (aa = 0; aa < 2; ++aa)
    for (order = 0; order < 2; ++order)
    {
        winetest_push_context("position %g transform %u mode %u aa %u order %u", positions[pos], transform, modes[mode], aa, order);
        type = mode == 0 || aa ? DWRITE_TEXTURE_ALIASED_1x1 : DWRITE_TEXTURE_CLEARTYPE_3x1;
        analysis = analyze(glyphs, 1, positions[pos], transform, modes[mode], aa);
        if (!analysis) { winetest_pop_context(); continue; }
        texture(analysis, type, &valid);
        IDWriteGlyphRunAnalysis_Release(analysis);
        if (order) { glyphs[1] = glyphs[0]; glyphs[0] = 0; }
        analysis = analyze(glyphs, 2, 0, transform, modes[mode], aa);
        if (!analysis) { free(valid.data); winetest_pop_context(); continue; }
        texture(analysis, type, &baseline);
        IDWriteGlyphRunAnalysis_Release(analysis);
        analysis = analyze(glyphs, 2, positions[pos], transform, modes[mode], aa);
        if (analysis)
        {
            texture(analysis, type, &combined);
            IDWriteGlyphRunAnalysis_Release(analysis);
            if (pos)
            {
                /* The wrapped baseline glyph still expands the horizontal bounds.
                 * Its inverted vertical bound cannot expand those of the upper glyph. */
                expected = valid.bounds;
                expected.left = baseline.bounds.left;
                expected.right = baseline.bounds.right;
                ok(EqualRect(&expected, &combined.bounds), "Bounds %s, expected %s.\n",
                        wine_dbgstr_rect(&combined.bounds), wine_dbgstr_rect(&expected));
            }
            else ok(!EqualRect(&valid.bounds, &combined.bounds), "Control glyph unexpectedly invisible.\n");
            pixel_size = type == DWRITE_TEXTURE_CLEARTYPE_3x1 ? 3 : 1;
            changed = 0;
            for (y = 0; y < (UINT)(combined.bounds.bottom - combined.bounds.top); ++y)
            for (x = 0; x < (UINT)(combined.bounds.right - combined.bounds.left); ++x)
            {
                normal_y = (UINT)combined.bounds.top + y - (pos ? 0x80000000u : 0);
                for (c = 0; c < pixel_size; ++c)
                {
                    BYTE reference = 0;
                    LONG normal_x = combined.bounds.left + x;
                    dest_index = (y * (combined.bounds.right - combined.bounds.left) + x) * pixel_size + c;
                    if (normal_x >= baseline.bounds.left && normal_x < baseline.bounds.right &&
                            normal_y >= baseline.bounds.top && normal_y < baseline.bounds.bottom)
                    {
                        source_index = ((normal_y - baseline.bounds.top) * (baseline.bounds.right - baseline.bounds.left)
                                + normal_x - baseline.bounds.left) * pixel_size + c;
                        reference = baseline.data[source_index];
                    }
                    if (combined.data[dest_index] != reference) ++changed;
                }
            }
            ok(!changed, "%u of %u bytes differ from the corresponding ordinary-origin pixels.\n", changed, combined.size);
            free(combined.data);
        }
        if (order) { glyphs[0] = glyphs[1]; glyphs[1] = 0; }
        free(baseline.data);
        free(valid.data);
        winetest_pop_context();
    }
}

static void test_texture_sizes(void)
{
    static const RECT rects[] = {
        {1000000,1000000,1000001,1000001},
        {1000000,1000000,1065536,1065536},
        {1000000,1000000,1070000,1070000},
        {1000000,1000000,1000000,1000000},
        {1000001,1000000,1000000,1000001},
        {1000001,1000001,1000000,1000000},
        {INT_MIN,1000000,INT_MAX,1000000},
        {INT_MIN,1000000,INT_MAX,1000001},
        {INT_MIN,1000000,INT_MAX,1000002},
    };
    IDWriteGlyphRunAnalysis *analysis;
    UINT16 glyph = 5;
    BYTE data[64];
    UINT i, mode, requested, j;
    HRESULT expected;
    BOOL valid_payload;
    HRESULT hr;
    for (mode = 0; mode < 2; ++mode)
    {
        analysis = analyze(&glyph, 1, 0, FALSE, mode ? DWRITE_RENDERING_MODE_NATURAL : DWRITE_RENDERING_MODE_ALIASED,
                DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);
        if (!analysis) continue;
        for (requested = 0; requested < 2; ++requested)
        for (i = 0; i < ARRAY_SIZE(rects); ++i)
        {
            memset(data, 0xcc, sizeof(data));
            hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis,
                    requested, &rects[i], data, 32);
            expected = (i >= 3 && i <= 6) ? E_INVALIDARG :
                    !mode && requested ? DWRITE_E_UNSUPPORTEDOPERATION :
                    !i ? S_OK : requested ? HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) : E_NOT_SUFFICIENT_BUFFER;
            winetest_push_context("analysis type %u requested %u rect %u", mode, requested, i);
            /* Native also permits an empty aliased texture from a ClearType run.
             * That pre-existing texture-type capability is separate from sizing. */
            todo_wine_if(mode && !requested && !i)
            ok(hr == expected, "CreateAlphaTexture %#lx, expected %#lx.\n", hr, expected);
            valid_payload = TRUE;
            for (j = 0; j < 32; ++j)
                if (data[j] != (SUCCEEDED(expected) ? 0 : 0xcc)) valid_payload = FALSE;
            todo_wine_if(mode && !requested && !i)
            ok(valid_payload, "Unexpected buffer contents after %#lx.\n", hr);
            for (j = 32; j < sizeof(data); ++j) ok(data[j] == 0xcc, "Buffer overrun at %u.\n", j);
            winetest_pop_context();
        }
        IDWriteGlyphRunAnalysis_Release(analysis);
    }
}

START_TEST(glyph_clip)
{
    WCHAR directory[MAX_PATH], path[MAX_PATH];
    IDWriteFontFile *font_file;
    HANDLE file;
    HRSRC resource;
    DWORD size, written;
    HRESULT hr;
    BOOL ret;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    GetTempPathW(ARRAY_SIZE(directory), directory);
    ret = GetTempFileNameW(directory, L"gcl", 0, path);
    ok(ret, "Temporary filename failed.\n");
    if (!ret) return;
    resource = FindResourceW(NULL, MAKEINTRESOURCEW(1), (const WCHAR *)RT_RCDATA);
    ok(resource != NULL, "Missing font resource.\n");
    size = SizeofResource(NULL, resource);
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "Font file creation failed.\n");
    ret = WriteFile(file, LockResource(LoadResource(NULL, resource)), size, &written, NULL);
    ok(ret && written == size, "Font write failed.\n");
    CloseHandle(file);
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory2, (IUnknown **)&factory);
    ok(hr == S_OK, "Factory %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = IDWriteFactory2_CreateFontFileReference(factory, path, NULL, &font_file);
        ok(hr == S_OK, "FontFile %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            hr = IDWriteFactory2_CreateFontFace(factory, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1,
                    &font_file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
            ok(hr == S_OK, "FontFace %#lx.\n", hr);
            if (SUCCEEDED(hr))
            {
                test_wrapped_glyph();
                test_texture_sizes();
                IDWriteFontFace_Release(face);
            }
            IDWriteFontFile_Release(font_file);
        }
        IDWriteFactory2_Release(factory);
    }
    DeleteFileW(path);
    CoUninitialize();
}

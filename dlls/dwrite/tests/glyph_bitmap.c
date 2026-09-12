/* Glyph bitmap output must not depend on previously used rendering modes.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "dwrite.h"
#include "wine/test.h"

struct test_face
{
    IDWriteFactory *factory;
    IDWriteFontFace *face;
};

struct texture
{
    RECT bounds;
    UINT size;
    BYTE *data;
};

static BOOL create_face(const WCHAR *path, DWRITE_FONT_SIMULATIONS simulations, struct test_face *face)
{
    IDWriteFontFile *file;
    HRESULT hr;

    memset(face, 0, sizeof(*face));
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, (IUnknown **)&face->factory);
    ok(hr == S_OK, "Factory creation failed %#lx.\n", hr);
    if (FAILED(hr)) return FALSE;
    hr = IDWriteFactory_CreateFontFileReference(face->factory, path, NULL, &file);
    ok(hr == S_OK, "Font file creation failed %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = IDWriteFactory_CreateFontFace(face->factory, DWRITE_FONT_FACE_TYPE_TRUETYPE,
                1, &file, 0, simulations, &face->face);
        ok(hr == S_OK, "Font face creation failed %#lx.\n", hr);
        IDWriteFontFile_Release(file);
    }
    if (FAILED(hr)) IDWriteFactory_Release(face->factory);
    return SUCCEEDED(hr);
}

static void release_face(struct test_face *face)
{
    IDWriteFontFace_Release(face->face);
    IDWriteFactory_Release(face->factory);
}

static BOOL render(struct test_face *face, FLOAT emsize, DWRITE_RENDERING_MODE mode, struct texture *texture)
{
    static const UINT32 codepoints[] = {'A', 'D', 'B'};
    DWRITE_TEXTURE_TYPE type = mode == DWRITE_RENDERING_MODE_ALIASED
            ? DWRITE_TEXTURE_ALIASED_1x1 : DWRITE_TEXTURE_CLEARTYPE_3x1;
    IDWriteGlyphRunAnalysis *analysis;
    DWRITE_GLYPH_RUN run = {0};
    UINT16 glyphs[3];
    HRESULT hr;
    UINT i;

    memset(texture, 0, sizeof(*texture));
    hr = IDWriteFontFace_GetGlyphIndices(face->face, codepoints, ARRAY_SIZE(codepoints), glyphs);
    ok(hr == S_OK, "Glyph lookup failed %#lx.\n", hr);
    for (i = 0; i < ARRAY_SIZE(glyphs); ++i) ok(glyphs[i] != 0, "Missing glyph %u.\n", i);
    run.fontFace = face->face;
    run.fontEmSize = emsize;
    run.glyphCount = ARRAY_SIZE(glyphs);
    run.glyphIndices = glyphs;
    hr = IDWriteFactory_CreateGlyphRunAnalysis(face->factory, &run, 1.0f, NULL, mode,
            DWRITE_MEASURING_MODE_NATURAL, 2.25f, emsize + 2.5f, &analysis);
    ok(hr == S_OK, "Analysis creation failed %#lx.\n", hr);
    if (FAILED(hr)) return FALSE;
    hr = IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(analysis, type, &texture->bounds);
    ok(hr == S_OK, "Bounds query failed %#lx.\n", hr);
    ok(!IsRectEmpty(&texture->bounds), "Empty ink bounds.\n");
    texture->size = (texture->bounds.right - texture->bounds.left)
            * (texture->bounds.bottom - texture->bounds.top) * (type == DWRITE_TEXTURE_CLEARTYPE_3x1 ? 3 : 1);
    texture->data = malloc(texture->size + 32);
    ok(texture->data != NULL, "Texture allocation failed.\n");
    if (!texture->data)
    {
        IDWriteGlyphRunAnalysis_Release(analysis);
        return FALSE;
    }
    memset(texture->data, 0xcc, texture->size + 32);
    hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, type, &texture->bounds,
            texture->data, texture->size);
    ok(hr == S_OK, "Texture rendering failed %#lx.\n", hr);
    for (i = texture->size; i < texture->size + 32; ++i)
        ok(texture->data[i] == 0xcc, "Texture overrun at byte %u.\n", i);
    IDWriteGlyphRunAnalysis_Release(analysis);
    return SUCCEEDED(hr);
}

START_TEST(glyph_bitmap)
{
    static const FLOAT sizes[] = {13.0f, 27.0f, 80.0f};
    static const DWRITE_FONT_SIMULATIONS simulations[] =
    {
        DWRITE_FONT_SIMULATIONS_NONE, DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE,
    };
    static const DWRITE_RENDERING_MODE modes[] =
    {
        DWRITE_RENDERING_MODE_ALIASED, DWRITE_RENDERING_MODE_GDI_CLASSIC,
        DWRITE_RENDERING_MODE_GDI_NATURAL, DWRITE_RENDERING_MODE_NATURAL,
        DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
    };
    struct texture expected[ARRAY_SIZE(modes)], actual, warmup;
    WCHAR directory[MAX_PATH], path[MAX_PATH];
    UINT s, sim, first, mode, pass, different, i;
    DWORD written, resource_size;
    struct test_face face;
    HANDLE file;
    HRSRC resource;
    BOOL ret;

    const UINT mode_count = ARRAY_SIZE(modes);
    const BOOL safe_order = GetEnvironmentVariableA("DWRITE_TEST_SAFE_ORDER", NULL, 0) != 0;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    GetTempPathW(ARRAY_SIZE(directory), directory);
    ret = GetTempFileNameW(directory, L"gbm", 0, path);
    ok(ret, "Temporary filename failed.\n");
    if (!ret) return;
    resource = FindResourceW(NULL, MAKEINTRESOURCEW(1), (const WCHAR *)RT_RCDATA);
    ok(resource != NULL, "Missing test font.\n");
    resource_size = SizeofResource(NULL, resource);
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "Font file creation failed.\n");
    ret = WriteFile(file, LockResource(LoadResource(NULL, resource)), resource_size, &written, NULL);
    ok(ret && written == resource_size, "Font file write failed.\n");
    CloseHandle(file);

    for (sim = 0; sim < ARRAY_SIZE(simulations); ++sim)
    for (s = 0; s < ARRAY_SIZE(sizes); ++s)
    {
        winetest_push_context("simulation %u size %g", simulations[sim], sizes[s]);
        memset(expected, 0, sizeof(expected));
        for (mode = 0; mode < mode_count; ++mode)
        {
            winetest_push_context("fresh mode %u", modes[mode]);
            if (create_face(path, simulations[sim], &face))
            {
                render(&face, sizes[s], modes[mode], &expected[mode]);
                release_face(&face);
            }
            winetest_pop_context();
        }
        for (first = 0; first < (safe_order ? 1 : mode_count); ++first)
        {
            winetest_push_context("first mode %u", modes[first]);
            if (create_face(path, simulations[sim], &face))
            {
                render(&face, sizes[s], modes[first], &warmup);
                free(warmup.data);
                for (pass = 0; pass < 2; ++pass)
                for (mode = 0; mode < mode_count; ++mode)
                {
                    winetest_push_context("pass %u mode %u", pass, modes[mode]);
                    if (render(&face, sizes[s], modes[mode], &actual) && expected[mode].data)
                    {
                        ok(EqualRect(&actual.bounds, &expected[mode].bounds), "Bounds changed after mode switch.\n");
                        ok(actual.size == expected[mode].size, "Texture size changed from %u to %u.\n",
                                expected[mode].size, actual.size);
                        if (actual.size == expected[mode].size)
                        {
                            different = 0;
                            for (i = 0; i < actual.size; ++i)
                                if (actual.data[i] != expected[mode].data[i]) ++different;
                            ok(!different, "%u / %u texture bytes differ from a fresh face.\n", different, actual.size);
                        }
                    }
                    free(actual.data);
                    winetest_pop_context();
                }
                release_face(&face);
            }
            winetest_pop_context();
        }
        for (mode = 0; mode < mode_count; ++mode) free(expected[mode].data);
        winetest_pop_context();
    }
    ret = DeleteFileW(path);
    ok(ret, "Font file deletion failed, error %lu.\n", GetLastError());
    CoUninitialize();
}

/* PNG raw metadata persistence and image encoding.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include "wine/test.h"
#include "wincodec.h"
#include "wincodecsdk.h"

static DWORD read_be32(const BYTE *p)
{
    return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3];
}

static DWORD chunk_crc(const BYTE *bytes, UINT size)
{
    DWORD crc = ~0u;
    UINT i, bit;

    for (i = 0; i < size; ++i)
    {
        crc ^= bytes[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & -(crc & 1));
    }
    return ~crc;
}

static void check_png(IWICImagingFactory *factory, IStream *stream,
        const BYTE *metadata, UINT metadata_size, const BYTE *pixels)
{
    IWICBitmapDecoder *decoder;
    IWICBitmapFrameDecode *frame;
    IWICBitmapSource *converted;
    LARGE_INTEGER zero = {0};
    STATSTG stat;
    BYTE data[4096], decoded[8];
    ULONG count, offset, length;
    UINT exif_count = 0, srgb_count = 0, gamma_count = 0;
    BOOL seen_idat = FALSE;
    HRESULT hr;

    hr = IStream_Stat(stream, &stat, STATFLAG_NONAME);
    ok(hr == S_OK, "Stat returned %#lx.\n", hr);
    if (FAILED(hr)) return;
    ok(stat.cbSize.QuadPart >= 8 && stat.cbSize.QuadPart <= sizeof(data),
            "Unexpected PNG size %s.\n", wine_dbgstr_longlong(stat.cbSize.QuadPart));
    if (stat.cbSize.QuadPart < 8 || stat.cbSize.QuadPart > sizeof(data)) return;
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    hr = IStream_Read(stream, data, stat.cbSize.LowPart, &count);
    ok(hr == S_OK && count == stat.cbSize.LowPart, "Read returned %#lx, %lu.\n", hr, count);
    if (FAILED(hr) || count != stat.cbSize.LowPart) return;
    ok(!memcmp(data, "\x89PNG\r\n\x1a\n", 8), "Missing PNG signature.\n");
    for (offset = 8; offset + 12 <= count; offset += length + 12)
    {
        length = read_be32(data + offset);
        ok(length <= count - offset - 12, "Truncated PNG chunk at %lu.\n", offset);
        if (length > count - offset - 12) break;
        ok(chunk_crc(data + offset + 4, length + 4) == read_be32(data + offset + 8 + length),
                "Incorrect CRC at %lu.\n", offset);
        if (!memcmp(data + offset + 4, "eXIf", 4))
        {
            ++exif_count;
            ok(!seen_idat, "EXIF metadata was written after image data.\n");
            ok(metadata_size >= 8 && length == metadata_size - 8, "EXIF length %lu, input %u.\n",
                    length, metadata_size);
            if (metadata_size >= 8 && length == metadata_size - 8)
                ok(!memcmp(data + offset + 8, metadata + 8, length), "EXIF bytes changed.\n");
        }
        else if (!memcmp(data + offset + 4, "sRGB", 4))
        {
            ++srgb_count;
            ok(length == 1 && data[offset + 8] == 0, "Unexpected sRGB metadata.\n");
        }
        else if (!memcmp(data + offset + 4, "gAMA", 4))
        {
            ++gamma_count;
            ok(length == 4 && read_be32(data + offset + 8) == 45455, "Unexpected gamma metadata.\n");
        }
        else if (!memcmp(data + offset + 4, "IDAT", 4)) seen_idat = TRUE;
    }
    ok(offset == count, "Trailing or truncated PNG data at %lu/%lu.\n", offset, count);
    ok(seen_idat, "No image data.\n");
    ok(exif_count == !!metadata_size, "EXIF chunk count %u, expected %u.\n", exif_count, !!metadata_size);
    todo_wine ok(srgb_count == 1, "Default sRGB chunk count %u.\n", srgb_count);
    todo_wine ok(gamma_count == 1, "Default gamma chunk count %u.\n", gamma_count);

    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    hr = IWICImagingFactory_CreateDecoderFromStream(factory, stream, NULL, WICDecodeMetadataCacheOnDemand, &decoder);
    ok(hr == S_OK, "Decode returned %#lx.\n", hr);
    if (FAILED(hr)) return;
    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    ok(hr == S_OK, "GetFrame returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = WICConvertBitmapSource(&GUID_WICPixelFormat32bppBGRA, (IWICBitmapSource *)frame, &converted);
        ok(hr == S_OK, "Convert returned %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            memset(decoded, 0xcc, sizeof(decoded));
            hr = IWICBitmapSource_CopyPixels(converted, NULL, sizeof(decoded), sizeof(decoded), decoded);
            ok(hr == S_OK, "CopyPixels returned %#lx.\n", hr);
            ok(!memcmp(decoded, pixels, sizeof(decoded)), "Encoded pixels changed.\n");
            IWICBitmapSource_Release(converted);
        }
        IWICBitmapFrameDecode_Release(frame);
    }
    IWICBitmapDecoder_Release(decoder);
}

static void test_raw_metadata(IWICImagingFactory *factory, IWICComponentFactory *component)
{
    static const UINT sizes[] = {0, 1, 4, 7, 8, 9, 22, 26};
    BYTE chunk[] = {0,0,0,14,'e','X','I','f','I','I',42,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0};
    BYTE pixels[] = {1,2,3,255,4,5,6,255}, persisted[sizeof(chunk)];
    IWICBitmapEncoder *encoder;
    IWICBitmapFrameEncode *frame;
    IWICMetadataBlockWriter *block;
    IWICMetadataWriter *writer;
    IWICPersistStream *persist;
    IStream *stream, *saved;
    IPropertyBag2 *bag;
    PROPVARIANT empty = {0}, value = {0};
    ULARGE_INTEGER size;
    LARGE_INTEGER zero = {0};
    STATSTG stat;
    GUID format;
    DWORD crc;
    ULONG count;
    UINT i, when;
    BOOL invalid;
    HRESULT hr;

    crc = chunk_crc(chunk + 4, 18);
    chunk[22] = crc >> 24; chunk[23] = crc >> 16; chunk[24] = crc >> 8; chunk[25] = crc;
    for (when = 0; when < 4; ++when)
    for (i = 0; i < ARRAY_SIZE(sizes); ++i)
    {
        winetest_push_context("size %u, add stage %u", sizes[i], when);
        invalid = when == 1 && sizes[i] && sizes[i] < 8;
        hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
        ok(hr == S_OK, "Create stream returned %#lx.\n", hr);
        hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng, NULL, &encoder);
        ok(hr == S_OK, "Create encoder returned %#lx.\n", hr);
        hr = IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache);
        ok(hr == S_OK, "Initialize encoder returned %#lx.\n", hr);
        hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, &bag);
        ok(hr == S_OK, "Create frame returned %#lx.\n", hr);
        hr = IWICBitmapFrameEncode_QueryInterface(frame, &IID_IWICMetadataBlockWriter, (void **)&block);
        ok(hr == S_OK, "Block writer returned %#lx.\n", hr);
        hr = IWICComponentFactory_CreateMetadataWriter(component, &GUID_MetadataFormatUnknown, NULL, 0, &writer);
        ok(hr == S_OK, "Create metadata writer returned %#lx.\n", hr);
        value.vt = VT_BLOB; value.blob.cbSize = sizes[i]; value.blob.pBlobData = chunk;
        hr = IWICMetadataWriter_SetValue(writer, &empty, &empty, &value);
        ok(hr == S_OK, "SetValue returned %#lx.\n", hr);
        hr = IWICMetadataWriter_QueryInterface(writer, &IID_IWICPersistStream, (void **)&persist);
        ok(hr == S_OK, "Persist interface returned %#lx.\n", hr);
        size.QuadPart = 999;
        hr = IWICPersistStream_GetSizeMax(persist, &size);
        ok(hr == S_OK && size.QuadPart == sizes[i], "Persist size %#lx, %s.\n", hr,
                wine_dbgstr_longlong(size.QuadPart));
        CreateStreamOnHGlobal(NULL, TRUE, &saved);
        hr = IWICPersistStream_SaveEx(persist, saved, 0, FALSE);
        ok(hr == S_OK, "SaveEx returned %#lx.\n", hr);
        hr = IStream_Stat(saved, &stat, STATFLAG_NONAME);
        ok(hr == S_OK && stat.cbSize.QuadPart == sizes[i], "Saved size %#lx, %s.\n", hr,
                wine_dbgstr_longlong(stat.cbSize.QuadPart));
        IStream_Seek(saved, zero, STREAM_SEEK_SET, NULL);
        hr = IStream_Read(saved, persisted, sizes[i], &count);
        ok(hr == S_OK && count == sizes[i] && !memcmp(persisted, chunk, sizes[i]),
                "Persisted bytes differ, hr %#lx, count %lu.\n", hr, count);
        IStream_Release(saved);
        IWICPersistStream_Release(persist);
        if (when == 0)
        {
            hr = IWICMetadataBlockWriter_AddWriter(block, writer);
            ok(hr == WINCODEC_ERR_NOTINITIALIZED, "Early AddWriter returned %#lx.\n", hr);
        }
        hr = IWICBitmapFrameEncode_Initialize(frame, bag);
        ok(hr == S_OK, "Initialize frame returned %#lx.\n", hr);
        hr = IWICBitmapFrameEncode_SetSize(frame, 2, 1);
        ok(hr == S_OK, "SetSize returned %#lx.\n", hr);
        format = GUID_WICPixelFormat32bppBGRA;
        hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &format);
        ok(hr == S_OK && IsEqualGUID(&format, &GUID_WICPixelFormat32bppBGRA), "SetPixelFormat returned %#lx.\n", hr);
        if (when == 1)
        {
            hr = IWICMetadataBlockWriter_AddWriter(block, writer);
            ok(hr == S_OK, "AddWriter returned %#lx.\n", hr);
        }
        if (when <= 1)
        {
            IWICMetadataWriter_Release(writer);
            writer = NULL;
        }
        hr = IWICBitmapFrameEncode_WritePixels(frame, 1, sizeof(pixels), sizeof(pixels), pixels);
        ok(hr == (invalid ? WINCODEC_ERR_BADMETADATAHEADER : S_OK), "WritePixels returned %#lx.\n", hr);
        if (when == 2)
        {
            hr = IWICMetadataBlockWriter_AddWriter(block, writer);
            ok(hr == S_OK, "Late AddWriter returned %#lx.\n", hr);
        }
        hr = IWICBitmapFrameEncode_Commit(frame);
        todo_wine_if(invalid)
            ok(hr == (invalid ? WINCODEC_ERR_UNEXPECTEDSIZE : S_OK), "Frame commit returned %#lx.\n", hr);
        if (when == 3)
        {
            hr = IWICMetadataBlockWriter_AddWriter(block, writer);
            ok(hr == S_OK, "Committed AddWriter returned %#lx.\n", hr);
        }
        hr = IWICBitmapEncoder_Commit(encoder);
        ok(hr == (invalid ? WINCODEC_ERR_WRONGSTATE : S_OK), "Encoder commit returned %#lx.\n", hr);
        if (!invalid) check_png(factory, stream, chunk, when == 1 ? sizes[i] : 0, pixels);
        else
        {
            hr = IStream_Stat(stream, &stat, STATFLAG_NONAME);
            ok(hr == S_OK && !stat.cbSize.QuadPart, "Failed frame wrote output bytes.\n");
        }
        if (writer) IWICMetadataWriter_Release(writer);
        IWICMetadataBlockWriter_Release(block);
        IPropertyBag2_Release(bag);
        IWICBitmapFrameEncode_Release(frame);
        IWICBitmapEncoder_Release(encoder);
        IStream_Release(stream);
        winetest_pop_context();
    }
}

START_TEST(png_metadata)
{
    IWICImagingFactory *factory;
    IWICComponentFactory *component;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory, (void **)&factory);
    ok(hr == S_OK, "Factory returned %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = IWICImagingFactory_QueryInterface(factory, &IID_IWICComponentFactory, (void **)&component);
        ok(hr == S_OK, "Component factory returned %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            test_raw_metadata(factory, component);
            IWICComponentFactory_Release(component);
        }
        IWICImagingFactory_Release(factory);
    }
    CoUninitialize();
}

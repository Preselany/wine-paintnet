/*
 * Copyright 2018 Nikolay Sivov for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "d2d1_private.h"
#include <float.h>

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

static inline struct d2d_transform *impl_from_ID2D1OffsetTransform(ID2D1OffsetTransform *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_transform, ID2D1TransformNode_iface);
}

static inline struct d2d_transform *impl_from_ID2D1BlendTransform(ID2D1BlendTransform *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_transform, ID2D1TransformNode_iface);
}

static inline struct d2d_transform *impl_from_ID2D1BorderTransform(ID2D1BorderTransform *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_transform, ID2D1TransformNode_iface);
}

static inline struct d2d_transform *impl_from_ID2D1BoundsAdjustmentTransform(
        ID2D1BoundsAdjustmentTransform *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_transform, ID2D1TransformNode_iface);
}

static inline struct d2d_vertex_buffer *impl_from_ID2D1VertexBuffer(ID2D1VertexBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_vertex_buffer, ID2D1VertexBuffer_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_vertex_buffer_QueryInterface(ID2D1VertexBuffer *iface,
        REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1VertexBuffer)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = iface;
        ID2D1VertexBuffer_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported interface %s.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_vertex_buffer_AddRef(ID2D1VertexBuffer *iface)
{
    struct d2d_vertex_buffer *buffer = impl_from_ID2D1VertexBuffer(iface);
    ULONG refcount = InterlockedIncrement(&buffer->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_vertex_buffer_Release(ID2D1VertexBuffer *iface)
{
    struct d2d_vertex_buffer *buffer = impl_from_ID2D1VertexBuffer(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
        free(buffer);

    return refcount;
}

static HRESULT STDMETHODCALLTYPE d2d_vertex_buffer_Map(ID2D1VertexBuffer *iface, BYTE **data, UINT32 size)
{
    FIXME("iface %p, data %p, size %u.\n", iface, data, size);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_vertex_buffer_Unmap(ID2D1VertexBuffer *iface)
{
    FIXME("iface %p.\n", iface);

    return E_NOTIMPL;
}

static const ID2D1VertexBufferVtbl d2d_vertex_buffer_vtbl =
{
    d2d_vertex_buffer_QueryInterface,
    d2d_vertex_buffer_AddRef,
    d2d_vertex_buffer_Release,
    d2d_vertex_buffer_Map,
    d2d_vertex_buffer_Unmap,
};

static HRESULT d2d_vertex_buffer_create(ID2D1VertexBuffer **buffer)
{
    struct d2d_vertex_buffer *object;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->ID2D1VertexBuffer_iface.lpVtbl = &d2d_vertex_buffer_vtbl;
    object->refcount = 1;

    *buffer = &object->ID2D1VertexBuffer_iface;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_offset_transform_QueryInterface(ID2D1OffsetTransform *iface,
        REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1OffsetTransform)
            || IsEqualGUID(iid, &IID_ID2D1TransformNode)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = iface;
        ID2D1OffsetTransform_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported interface %s.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_offset_transform_AddRef(ID2D1OffsetTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1OffsetTransform(iface);
    ULONG refcount = InterlockedIncrement(&transform->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_offset_transform_Release(ID2D1OffsetTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1OffsetTransform(iface);
    ULONG refcount = InterlockedDecrement(&transform->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
        free(transform);

    return refcount;
}

static UINT32 STDMETHODCALLTYPE d2d_offset_transform_GetInputCount(ID2D1OffsetTransform *iface)
{
    TRACE("iface %p.\n", iface);

    return 1;
}

static void STDMETHODCALLTYPE d2d_offset_transform_SetOffset(ID2D1OffsetTransform *iface,
        D2D1_POINT_2L offset)
{
    struct d2d_transform *transform = impl_from_ID2D1OffsetTransform(iface);

    TRACE("iface %p, offset %s.\n", iface, debug_d2d_point_2l(&offset));

    transform->offset = offset;
}

static D2D1_POINT_2L * STDMETHODCALLTYPE d2d_offset_transform_GetOffset(ID2D1OffsetTransform *iface,
        D2D1_POINT_2L *offset)
{
    struct d2d_transform *transform = impl_from_ID2D1OffsetTransform(iface);

    TRACE("iface %p.\n", iface);

    *offset = transform->offset;
    return offset;
}

static const ID2D1OffsetTransformVtbl d2d_offset_transform_vtbl =
{
    d2d_offset_transform_QueryInterface,
    d2d_offset_transform_AddRef,
    d2d_offset_transform_Release,
    d2d_offset_transform_GetInputCount,
    d2d_offset_transform_SetOffset,
    d2d_offset_transform_GetOffset,
};

static HRESULT d2d_offset_transform_create(D2D1_POINT_2L offset, ID2D1OffsetTransform **transform)
{
    struct d2d_transform *object;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->ID2D1TransformNode_iface.lpVtbl = (ID2D1TransformNodeVtbl *)&d2d_offset_transform_vtbl;
    object->refcount = 1;
    object->offset = offset;

    *transform = (ID2D1OffsetTransform *)&object->ID2D1TransformNode_iface;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_blend_transform_QueryInterface(ID2D1BlendTransform *iface,
        REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1BlendTransform)
            || IsEqualGUID(iid, &IID_ID2D1ConcreteTransform)
            || IsEqualGUID(iid, &IID_ID2D1TransformNode)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = iface;
        ID2D1BlendTransform_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported interface %s.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_blend_transform_AddRef(ID2D1BlendTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1BlendTransform(iface);
    ULONG refcount = InterlockedIncrement(&transform->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_blend_transform_Release(ID2D1BlendTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1BlendTransform(iface);
    ULONG refcount = InterlockedDecrement(&transform->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
        free(transform);

    return refcount;
}

static UINT32 STDMETHODCALLTYPE d2d_blend_transform_GetInputCount(ID2D1BlendTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1BlendTransform(iface);

    TRACE("iface %p.\n", iface);

    return transform->input_count;
}

static HRESULT STDMETHODCALLTYPE d2d_blend_transform_SetOutputBuffer(ID2D1BlendTransform *iface,
        D2D1_BUFFER_PRECISION precision, D2D1_CHANNEL_DEPTH depth)
{
    FIXME("iface %p, precision %u, depth %u stub.\n", iface, precision, depth);

    return E_NOTIMPL;
}

static void STDMETHODCALLTYPE d2d_blend_transform_SetCached(ID2D1BlendTransform *iface,
        BOOL is_cached)
{
    FIXME("iface %p, is_cached %d stub.\n", iface, is_cached);
}

static void STDMETHODCALLTYPE d2d_blend_transform_SetDescription(ID2D1BlendTransform *iface,
        const D2D1_BLEND_DESCRIPTION *description)
{
    struct d2d_transform *transform = impl_from_ID2D1BlendTransform(iface);

    TRACE("iface %p, description %p.\n", iface, description);

    transform->blend_desc = *description;
}

static void STDMETHODCALLTYPE d2d_blend_transform_GetDescription(ID2D1BlendTransform *iface,
        D2D1_BLEND_DESCRIPTION *description)
{
    struct d2d_transform *transform = impl_from_ID2D1BlendTransform(iface);

    TRACE("iface %p, description %p.\n", iface, description);

    *description = transform->blend_desc;
}

static const ID2D1BlendTransformVtbl d2d_blend_transform_vtbl =
{
    d2d_blend_transform_QueryInterface,
    d2d_blend_transform_AddRef,
    d2d_blend_transform_Release,
    d2d_blend_transform_GetInputCount,
    d2d_blend_transform_SetOutputBuffer,
    d2d_blend_transform_SetCached,
    d2d_blend_transform_SetDescription,
    d2d_blend_transform_GetDescription,
};

static HRESULT d2d_blend_transform_create(UINT32 input_count, const D2D1_BLEND_DESCRIPTION *blend_desc,
        ID2D1BlendTransform **transform)
{
    struct d2d_transform *object;

    *transform = NULL;

    if (!input_count)
        return E_INVALIDARG;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->ID2D1TransformNode_iface.lpVtbl = (ID2D1TransformNodeVtbl *)&d2d_blend_transform_vtbl;
    object->refcount = 1;
    object->input_count = input_count;
    object->blend_desc = *blend_desc;

    *transform = (ID2D1BlendTransform *)&object->ID2D1TransformNode_iface;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_border_transform_QueryInterface(ID2D1BorderTransform *iface,
        REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1BorderTransform)
            || IsEqualGUID(iid, &IID_ID2D1ConcreteTransform)
            || IsEqualGUID(iid, &IID_ID2D1TransformNode)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = iface;
        ID2D1BorderTransform_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported interface %s.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_border_transform_AddRef(ID2D1BorderTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1BorderTransform(iface);
    ULONG refcount = InterlockedIncrement(&transform->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_border_transform_Release(ID2D1BorderTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1BorderTransform(iface);
    ULONG refcount = InterlockedDecrement(&transform->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
        free(transform);

    return refcount;
}

static UINT32 STDMETHODCALLTYPE d2d_border_transform_GetInputCount(ID2D1BorderTransform *iface)
{
    TRACE("iface %p.\n", iface);

    return 1;
}

static HRESULT STDMETHODCALLTYPE d2d_border_transform_SetOutputBuffer(
        ID2D1BorderTransform *iface, D2D1_BUFFER_PRECISION precision, D2D1_CHANNEL_DEPTH depth)
{
    FIXME("iface %p, precision %u, depth %u stub.\n", iface, precision, depth);

    return E_NOTIMPL;
}

static void STDMETHODCALLTYPE d2d_border_transform_SetCached(
        ID2D1BorderTransform *iface, BOOL is_cached)
{
    FIXME("iface %p, is_cached %d stub.\n", iface, is_cached);
}

static void STDMETHODCALLTYPE d2d_border_transform_SetExtendModeX(
        ID2D1BorderTransform *iface, D2D1_EXTEND_MODE mode)
{
    struct d2d_transform *transform = impl_from_ID2D1BorderTransform(iface);

    TRACE("iface %p.\n", iface);

    switch (mode)
    {
        case D2D1_EXTEND_MODE_CLAMP:
        case D2D1_EXTEND_MODE_WRAP:
        case D2D1_EXTEND_MODE_MIRROR:
            transform->border.mode_x = mode;
            break;
        default:
            ;
    }
}

static void STDMETHODCALLTYPE d2d_border_transform_SetExtendModeY(
        ID2D1BorderTransform *iface, D2D1_EXTEND_MODE mode)
{
    struct d2d_transform *transform = impl_from_ID2D1BorderTransform(iface);

    TRACE("iface %p.\n", iface);

    switch (mode)
    {
        case D2D1_EXTEND_MODE_CLAMP:
        case D2D1_EXTEND_MODE_WRAP:
        case D2D1_EXTEND_MODE_MIRROR:
            transform->border.mode_y = mode;
            break;
        default:
            ;
    }
}

static D2D1_EXTEND_MODE STDMETHODCALLTYPE d2d_border_transform_GetExtendModeX(
        ID2D1BorderTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1BorderTransform(iface);

    TRACE("iface %p.\n", iface);

    return transform->border.mode_x;
}

static D2D1_EXTEND_MODE STDMETHODCALLTYPE d2d_border_transform_GetExtendModeY(
        ID2D1BorderTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1BorderTransform(iface);

    TRACE("iface %p.\n", iface);

    return transform->border.mode_y;
}

static const ID2D1BorderTransformVtbl d2d_border_transform_vtbl =
{
    d2d_border_transform_QueryInterface,
    d2d_border_transform_AddRef,
    d2d_border_transform_Release,
    d2d_border_transform_GetInputCount,
    d2d_border_transform_SetOutputBuffer,
    d2d_border_transform_SetCached,
    d2d_border_transform_SetExtendModeX,
    d2d_border_transform_SetExtendModeY,
    d2d_border_transform_GetExtendModeX,
    d2d_border_transform_GetExtendModeY,
};

static HRESULT d2d_border_transform_create(D2D1_EXTEND_MODE mode_x, D2D1_EXTEND_MODE mode_y,
        ID2D1BorderTransform **transform)
{
    struct d2d_transform *object;

    *transform = NULL;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->ID2D1TransformNode_iface.lpVtbl = (ID2D1TransformNodeVtbl *)&d2d_border_transform_vtbl;
    object->refcount = 1;
    object->border.mode_x = mode_x;
    object->border.mode_y = mode_y;

    *transform = (ID2D1BorderTransform *)&object->ID2D1TransformNode_iface;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_bounds_adjustment_transform_QueryInterface(
        ID2D1BoundsAdjustmentTransform *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1BoundsAdjustmentTransform)
            || IsEqualGUID(iid, &IID_ID2D1TransformNode)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = iface;
        ID2D1BoundsAdjustmentTransform_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported interface %s.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_bounds_adjustment_transform_AddRef(
        ID2D1BoundsAdjustmentTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1BoundsAdjustmentTransform(iface);
    ULONG refcount = InterlockedIncrement(&transform->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_bounds_adjustment_transform_Release(
        ID2D1BoundsAdjustmentTransform *iface)
{
    struct d2d_transform *transform = impl_from_ID2D1BoundsAdjustmentTransform(iface);
    ULONG refcount = InterlockedDecrement(&transform->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
        free(transform);

    return refcount;
}

static UINT32 STDMETHODCALLTYPE d2d_bounds_adjustment_transform_GetInputCount(
        ID2D1BoundsAdjustmentTransform *iface)
{
    TRACE("iface %p.\n", iface);

    return 1;
}

static void STDMETHODCALLTYPE d2d_bounds_adjustment_transform_SetOutputBounds(
        ID2D1BoundsAdjustmentTransform *iface, const D2D1_RECT_L *bounds)
{
    struct d2d_transform *transform = impl_from_ID2D1BoundsAdjustmentTransform(iface);

    TRACE("iface %p.\n", iface);

    transform->bounds = *bounds;
}

static void STDMETHODCALLTYPE d2d_bounds_adjustment_transform_GetOutputBounds(
        ID2D1BoundsAdjustmentTransform *iface, D2D1_RECT_L *bounds)
{
    struct d2d_transform *transform = impl_from_ID2D1BoundsAdjustmentTransform(iface);

    TRACE("iface %p.\n", iface);

    *bounds = transform->bounds;
}

static const ID2D1BoundsAdjustmentTransformVtbl d2d_bounds_adjustment_transform_vtbl =
{
    d2d_bounds_adjustment_transform_QueryInterface,
    d2d_bounds_adjustment_transform_AddRef,
    d2d_bounds_adjustment_transform_Release,
    d2d_bounds_adjustment_transform_GetInputCount,
    d2d_bounds_adjustment_transform_SetOutputBounds,
    d2d_bounds_adjustment_transform_GetOutputBounds,
};

static HRESULT d2d_bounds_adjustment_transform_create(const D2D1_RECT_L *rect,
        ID2D1BoundsAdjustmentTransform **transform)
{
    struct d2d_transform *object;

    *transform = NULL;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->ID2D1TransformNode_iface.lpVtbl = (ID2D1TransformNodeVtbl *)&d2d_bounds_adjustment_transform_vtbl;
    object->refcount = 1;
    object->bounds = *rect;

    *transform = (ID2D1BoundsAdjustmentTransform *)&object->ID2D1TransformNode_iface;

    return S_OK;
}

static struct d2d_transform_node * d2d_transform_graph_get_node(const struct d2d_transform_graph *graph,
        ID2D1TransformNode *object)
{
    struct d2d_transform_node *node;

    LIST_FOR_EACH_ENTRY(node, &graph->nodes, struct d2d_transform_node, entry)
    {
        if (node->object == object)
            return node;
    }

    return NULL;
}

static HRESULT d2d_transform_graph_add_node(struct d2d_transform_graph *graph,
        ID2D1TransformNode *object)
{
    struct d2d_transform_node *node;

    if (!(node = calloc(1, sizeof(*node))))
        return E_OUTOFMEMORY;
    node->input_count = ID2D1TransformNode_GetInputCount(object);
    if (node->input_count)
    {
        if (!(node->inputs = calloc(node->input_count, sizeof(*node->inputs)))
                || !(node->effect_inputs = malloc(node->input_count * sizeof(*node->effect_inputs))))
        {
            free(node->inputs);
            free(node);
            return E_OUTOFMEMORY;
        }
        memset(node->effect_inputs, 0xff, node->input_count * sizeof(*node->effect_inputs));
    }

    node->object = object;
    ID2D1TransformNode_AddRef(node->object);
    list_add_tail(&graph->nodes, &node->entry);

    return S_OK;
}

static void d2d_transform_graph_delete_node(struct d2d_transform_graph *graph,
        struct d2d_transform_node *node)
{
    struct d2d_transform_node *other;
    unsigned int i;

    list_remove(&node->entry);
    ID2D1TransformNode_Release(node->object);

    if (graph->output == node)
        graph->output = NULL;

    if (node->render_info)
        ID2D1DrawInfo_Release(&node->render_info->ID2D1DrawInfo_iface);

    LIST_FOR_EACH_ENTRY(other, &graph->nodes, struct d2d_transform_node, entry)
    {
        for (i = 0; i < other->input_count; ++i)
            if (other->inputs[i] == node) other->inputs[i] = NULL;
    }

    free(node->effect_inputs);
    free(node->inputs);
    free(node);
}

static void d2d_transform_graph_clear(struct d2d_transform_graph *graph)
{
    struct d2d_transform_node *node, *node_next;

    LIST_FOR_EACH_ENTRY_SAFE(node, node_next, &graph->nodes, struct d2d_transform_node, entry)
    {
        d2d_transform_graph_delete_node(graph, node);
    }
    graph->passthrough = false;
}

static inline struct d2d_transform_graph *impl_from_ID2D1TransformGraph(ID2D1TransformGraph *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_transform_graph, ID2D1TransformGraph_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_QueryInterface(ID2D1TransformGraph *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1TransformGraph)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1TransformGraph_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_transform_graph_AddRef(ID2D1TransformGraph *iface)
{
    struct d2d_transform_graph *graph =impl_from_ID2D1TransformGraph(iface);
    ULONG refcount = InterlockedIncrement(&graph->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_transform_graph_Release(ID2D1TransformGraph *iface)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);
    ULONG refcount = InterlockedDecrement(&graph->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        d2d_transform_graph_clear(graph);
        free(graph);
    }

    return refcount;
}

static UINT32 STDMETHODCALLTYPE d2d_transform_graph_GetInputCount(ID2D1TransformGraph *iface)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);

    TRACE("iface %p.\n", iface);

    return graph->input_count;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_SetSingleTransformNode(ID2D1TransformGraph *iface,
        ID2D1TransformNode *object)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);
    struct d2d_transform_node *node;
    unsigned int i, input_count;
    HRESULT hr;

    TRACE("iface %p, object %p.\n", iface, object);

    d2d_transform_graph_clear(graph);
    if (FAILED(hr = d2d_transform_graph_add_node(graph, object)))
        return hr;

    node = d2d_transform_graph_get_node(graph, object);
    graph->output = node;

    input_count = ID2D1TransformNode_GetInputCount(object);
    if (graph->input_count != input_count)
        return E_INVALIDARG;

    for (i = 0; i < graph->input_count; ++i)
        node->effect_inputs[i] = i;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_AddNode(ID2D1TransformGraph *iface,
        ID2D1TransformNode *object)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);

    TRACE("iface %p, object %p.\n", iface, object);

    if (d2d_transform_graph_get_node(graph, object))
        return E_INVALIDARG;

    return d2d_transform_graph_add_node(graph, object);
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_RemoveNode(ID2D1TransformGraph *iface,
        ID2D1TransformNode *object)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);
    struct d2d_transform_node *node;

    TRACE("iface %p, object %p.\n", iface, object);

    if (!(node = d2d_transform_graph_get_node(graph, object)))
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

    d2d_transform_graph_delete_node(graph, node);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_SetOutputNode(ID2D1TransformGraph *iface,
        ID2D1TransformNode *object)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);
    struct d2d_transform_node *node;

    TRACE("iface %p, object %p.\n", iface, object);

    if (!(node = d2d_transform_graph_get_node(graph, object)))
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

    graph->output = node;
    graph->passthrough = false;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_ConnectNode(ID2D1TransformGraph *iface,
        ID2D1TransformNode *from_node, ID2D1TransformNode *to_node, UINT32 index)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);
    struct d2d_transform_node *from, *to;

    TRACE("iface %p, from_node %p, to_node %p, index %u.\n", iface, from_node, to_node, index);

    if (!(from = d2d_transform_graph_get_node(graph, from_node)))
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

    if (!(to = d2d_transform_graph_get_node(graph, to_node)))
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

    if (index >= to->input_count)
        return E_INVALIDARG;

    to->inputs[index] = from;
    to->effect_inputs[index] = ~0u;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_ConnectToEffectInput(ID2D1TransformGraph *iface,
        UINT32 input_index, ID2D1TransformNode *object, UINT32 node_index)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);
    struct d2d_transform_node *node;
    unsigned int count;

    TRACE("iface %p, input_index %u, object %p, node_index %u.\n", iface, input_index, object, node_index);

    if (!(node = d2d_transform_graph_get_node(graph, object)))
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

    if (input_index >= graph->input_count)
        return E_INVALIDARG;

    count = ID2D1TransformNode_GetInputCount(object);
    if (node_index >= count)
        return E_INVALIDARG;

    /* Connections belong to destination ports: one graph input may feed
     * several nodes, or several ports of the same node. */
    node->inputs[node_index] = NULL;
    node->effect_inputs[node_index] = input_index;
    graph->passthrough = false;

    return S_OK;
}

static void STDMETHODCALLTYPE d2d_transform_graph_Clear(ID2D1TransformGraph *iface)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);

    TRACE("iface %p.\n", iface);

    d2d_transform_graph_clear(graph);
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_SetPassthroughGraph(ID2D1TransformGraph *iface, UINT32 index)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);

    TRACE("iface %p, index %u.\n", iface, index);

    if (index >= graph->input_count)
        return E_INVALIDARG;

    d2d_transform_graph_clear(graph);

    graph->passthrough = true;
    graph->passthrough_input = index;

    return S_OK;
}

static const ID2D1TransformGraphVtbl d2d_transform_graph_vtbl =
{
    d2d_transform_graph_QueryInterface,
    d2d_transform_graph_AddRef,
    d2d_transform_graph_Release,
    d2d_transform_graph_GetInputCount,
    d2d_transform_graph_SetSingleTransformNode,
    d2d_transform_graph_AddNode,
    d2d_transform_graph_RemoveNode,
    d2d_transform_graph_SetOutputNode,
    d2d_transform_graph_ConnectNode,
    d2d_transform_graph_ConnectToEffectInput,
    d2d_transform_graph_Clear,
    d2d_transform_graph_SetPassthroughGraph,
};

static HRESULT d2d_transform_graph_create(UINT32 input_count, struct d2d_transform_graph **graph)
{
    struct d2d_transform_graph *object;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->ID2D1TransformGraph_iface.lpVtbl = &d2d_transform_graph_vtbl;
    object->refcount = 1;
    list_init(&object->nodes);

    object->input_count = input_count;

    *graph = object;

    return S_OK;
}

struct d2d_effect_impl
{
    ID2D1EffectImpl ID2D1EffectImpl_iface;
    LONG refcount;

    /* Followed by properties block, its size and format depends on particular effect. */
};

static inline struct d2d_effect_impl *impl_from_ID2D1EffectImpl(ID2D1EffectImpl *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_effect_impl, ID2D1EffectImpl_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_impl_QueryInterface(ID2D1EffectImpl *iface,
        REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1EffectImpl)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1EffectImpl_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_effect_impl_AddRef(ID2D1EffectImpl *iface)
{
    struct d2d_effect_impl *effect = impl_from_ID2D1EffectImpl(iface);
    return InterlockedIncrement(&effect->refcount);
}

static ULONG STDMETHODCALLTYPE d2d_effect_impl_Release(ID2D1EffectImpl *iface)
{
    struct d2d_effect_impl *effect = impl_from_ID2D1EffectImpl(iface);
    LONG refcount = InterlockedDecrement(&effect->refcount);

    if (!refcount)
        free(effect);

    return refcount;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_impl_Initialize(ID2D1EffectImpl *iface,
        ID2D1EffectContext *context, ID2D1TransformGraph *graph)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_impl_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE type)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_impl_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return S_OK;
}

static const ID2D1EffectImplVtbl d2d_effect_impl_vtbl =
{
    d2d_effect_impl_QueryInterface,
    d2d_effect_impl_AddRef,
    d2d_effect_impl_Release,
    d2d_effect_impl_Initialize,
    d2d_effect_impl_PrepareForRender,
    d2d_effect_impl_SetGraph,
};

static HRESULT d2d_effect_create_impl(IUnknown **effect_impl, const void *props,
        size_t props_size)
{
    struct d2d_effect_impl *object;

    if (!(object = calloc(1, sizeof(*object) + props_size)))
        return E_OUTOFMEMORY;

    object->ID2D1EffectImpl_iface.lpVtbl = &d2d_effect_impl_vtbl;
    object->refcount = 1;
    if (props_size) memcpy(object + 1, props, props_size);

    *effect_impl = (IUnknown *)&object->ID2D1EffectImpl_iface;

    return S_OK;
}

static UINT32 effect_property_type_size(D2D1_PROPERTY_TYPE prop_type)
{
    static const UINT32 sizes[D2D1_PROPERTY_TYPE_MATRIX_5X4 + 1] =
    {
        [D2D1_PROPERTY_TYPE_BOOL]       = sizeof(BOOL),
        [D2D1_PROPERTY_TYPE_UINT32]     = sizeof(UINT32),
        [D2D1_PROPERTY_TYPE_INT32]      = sizeof(INT32),
        [D2D1_PROPERTY_TYPE_FLOAT]      = sizeof(float),
        [D2D1_PROPERTY_TYPE_VECTOR2]    = sizeof(D2D_VECTOR_2F),
        [D2D1_PROPERTY_TYPE_VECTOR3]    = sizeof(D2D_VECTOR_3F),
        [D2D1_PROPERTY_TYPE_VECTOR4]    = sizeof(D2D_VECTOR_4F),
        [D2D1_PROPERTY_TYPE_ENUM]       = sizeof(UINT32),
        [D2D1_PROPERTY_TYPE_MATRIX_3X2] = sizeof(D2D_MATRIX_3X2_F),
        [D2D1_PROPERTY_TYPE_MATRIX_4X3] = sizeof(D2D_MATRIX_4X3_F),
        [D2D1_PROPERTY_TYPE_MATRIX_4X4] = sizeof(D2D_MATRIX_4X4_F),
        [D2D1_PROPERTY_TYPE_MATRIX_5X4] = sizeof(D2D_MATRIX_5X4_F),
    };

    if (prop_type >= ARRAY_SIZE(sizes))
        return 0;

    return sizes[prop_type];
}

static HRESULT effect_impl_prop_get_helper(const void *prop_data, D2D1_PROPERTY_TYPE prop_type,
        BYTE *data, UINT32 data_size, UINT32 *actual_size)
{
    UINT32 size = effect_property_type_size(prop_type);

    if (actual_size)
        *actual_size = size;

    if (data && data_size)
    {
        if (data_size < size)
            return E_NOT_SUFFICIENT_BUFFER;
        memcpy(data, prop_data, size);
    }

    return S_OK;
}

static HRESULT effect_impl_prop_set_helper(void *prop_data, D2D1_PROPERTY_TYPE prop_type,
        const BYTE *data, UINT32 data_size)
{
    if (data_size != effect_property_type_size(prop_type))
        return E_INVALIDARG;

    memcpy(prop_data, data, data_size);
    return S_OK;
}

#define EFFECT_PROPERTY_SET(name, prop, type) \
    static HRESULT __stdcall name##_##prop##_set(IUnknown *iface, const BYTE *data, UINT32 data_size) \
    { \
        struct d2d_effect_impl *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface); \
        struct name##_properties *props = (struct name##_properties *)(effect + 1); \
        return effect_impl_prop_set_helper(&props->prop, D2D1_PROPERTY_TYPE_##type, data, data_size); \
    } \

#define EFFECT_PROPERTY_GET(name, prop, type) \
    static HRESULT __stdcall name##_##prop##_get(const IUnknown *iface, BYTE *data, \
            UINT32 data_size, UINT32 *actual_size) \
    { \
        struct d2d_effect_impl *effect = impl_from_ID2D1EffectImpl((ID2D1EffectImpl *)iface); \
        struct name##_properties *props = (struct name##_properties *)(effect + 1); \
        return effect_impl_prop_get_helper(&props->prop, D2D1_PROPERTY_TYPE_##type, data, data_size, actual_size); \
    } \

#define EFFECT_PROPERTY_RW(name, prop, type) \
    EFFECT_PROPERTY_SET(name, prop, type) \
    EFFECT_PROPERTY_GET(name, prop, type)

#define BINDING_RW(name, prop) name##_##prop##_set, name##_##prop##_get

static const WCHAR _2d_affine_transform_description[] =
L"<?xml version='1.0'?>                                                      \
  <Effect>                                                                   \
    <Property name='DisplayName' type='string' value='2D Affine Transform'/> \
    <Property name='Author'      type='string' value='The Wine Project'/>    \
    <Property name='Category'    type='string' value='Stub'/>                \
    <Property name='Description' type='string' value='2D Affine Transform'/> \
    <Inputs>                                                                 \
      <Input name='Source'/>                                                 \
    </Inputs>                                                                \
    <Property name='InterpolationMode' type='enum' />                        \
    <Property name='BorderMode' type='enum' />                               \
    <Property name='TransformMatrix' type='matrix3x2' />                     \
    <Property name='Sharpness' type='float' />                               \
  </Effect>";

struct _2d_affine_transform_properties
{
    D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE interpolation_mode;
    D2D1_BORDER_MODE border_mode;
    D2D1_MATRIX_3X2_F transform_matrix;
    float sharpness;
};

EFFECT_PROPERTY_RW(_2d_affine_transform, interpolation_mode, ENUM)
EFFECT_PROPERTY_RW(_2d_affine_transform, border_mode, ENUM)
EFFECT_PROPERTY_RW(_2d_affine_transform, transform_matrix, MATRIX_3X2)
EFFECT_PROPERTY_RW(_2d_affine_transform, sharpness, FLOAT)

static const D2D1_PROPERTY_BINDING _2d_affine_transform_bindings[] =
{
    { L"InterpolationMode", BINDING_RW(_2d_affine_transform, interpolation_mode) },
    { L"BorderMode", BINDING_RW(_2d_affine_transform, border_mode) },
    { L"TransformMatrix", BINDING_RW(_2d_affine_transform, transform_matrix) },
    { L"Sharpness", BINDING_RW(_2d_affine_transform, sharpness) },
};

static HRESULT __stdcall _2d_affine_transform_factory(IUnknown **effect)
{
    static const struct _2d_affine_transform_properties properties =
    {
        .interpolation_mode = D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE_LINEAR,
        .border_mode = D2D1_BORDER_MODE_SOFT,
        .transform_matrix = { ._11 = 1.0f, ._22 = 1.0f },
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR _3d_perspective_transform_description[] =
L"<?xml version='1.0'?>                                                           \
  <Effect>                                                                        \
    <Property name='DisplayName' type='string' value='3D Perspective Transform'/> \
    <Property name='Author'      type='string' value='The Wine Project'/>         \
    <Property name='Category'    type='string' value='Stub'/>                     \
    <Property name='Description' type='string' value='3D Perspective Transform'/> \
    <Inputs>                                                                      \
      <Input name='Source'/>                                                      \
    </Inputs>                                                                     \
    <Property name='InterpolationMode' type='enum' />                             \
    <Property name='BorderMode' type='enum' />                                    \
    <Property name='Depth' type='float' />                                        \
    <Property name='PerspectiveOrigin' type='vector2' />                          \
    <Property name='LocalOffset' type='vector3' />                                \
    <Property name='GlobalOffset' type='vector3' />                               \
    <Property name='RotationOrigin' type='vector3' />                             \
    <Property name='Rotation' type='vector3' />                                   \
  </Effect>";

struct _3d_perspective_transform_properties
{
    D2D1_3DPERSPECTIVETRANSFORM_INTERPOLATION_MODE interpolation_mode;
    D2D1_BORDER_MODE border_mode;
    float depth;
    D2D1_VECTOR_2F perspective_origin;
    D2D1_VECTOR_3F local_offset;
    D2D1_VECTOR_3F global_offset;
    D2D1_VECTOR_3F rotation_origin;
    D2D1_VECTOR_3F rotation;
};

EFFECT_PROPERTY_RW(_3d_perspective_transform, interpolation_mode, ENUM)
EFFECT_PROPERTY_RW(_3d_perspective_transform, border_mode, ENUM)
EFFECT_PROPERTY_RW(_3d_perspective_transform, depth, FLOAT)
EFFECT_PROPERTY_RW(_3d_perspective_transform, perspective_origin, VECTOR2)
EFFECT_PROPERTY_RW(_3d_perspective_transform, local_offset, VECTOR3)
EFFECT_PROPERTY_RW(_3d_perspective_transform, global_offset, VECTOR3)
EFFECT_PROPERTY_RW(_3d_perspective_transform, rotation_origin, VECTOR3)
EFFECT_PROPERTY_RW(_3d_perspective_transform, rotation, VECTOR3)

static const D2D1_PROPERTY_BINDING _3d_perspective_transform_bindings[] =
{
    { L"InterpolationMode", BINDING_RW(_3d_perspective_transform, interpolation_mode) },
    { L"BorderMode", BINDING_RW(_3d_perspective_transform, border_mode) },
    { L"Depth", BINDING_RW(_3d_perspective_transform, depth) },
    { L"PerspectiveOrigin", BINDING_RW(_3d_perspective_transform, perspective_origin) },
    { L"LocalOffset", BINDING_RW(_3d_perspective_transform, local_offset) },
    { L"GlobalOffset", BINDING_RW(_3d_perspective_transform, global_offset) },
    { L"RotationOrigin", BINDING_RW(_3d_perspective_transform, rotation_origin) },
    { L"Rotation", BINDING_RW(_3d_perspective_transform, rotation) },
};

static HRESULT __stdcall _3d_perspective_transform_factory(IUnknown **effect)
{
    static const struct _3d_perspective_transform_properties properties =
    {
        .interpolation_mode = D2D1_3DPERSPECTIVETRANSFORM_INTERPOLATION_MODE_LINEAR,
        .border_mode = D2D1_BORDER_MODE_SOFT,
        .depth = 1000.0f,
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR composite_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Composite'/>        \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Composite'/>        \
    <Inputs minimum='1' maximum='0xffffffff' >                            \
      <Input name='Source1'/>                                             \
      <Input name='Source2'/>                                             \
    </Inputs>                                                             \
    <Property name='Mode' type='enum' />                                  \
  </Effect>";

struct composite_properties
{
    D2D1_COMPOSITE_MODE mode;
};

EFFECT_PROPERTY_RW(composite, mode, ENUM)

static const D2D1_PROPERTY_BINDING composite_bindings[] =
{
    { L"Mode", BINDING_RW(composite, mode) },
};

static HRESULT __stdcall composite_factory(IUnknown **effect)
{
    static const struct composite_properties properties =
    {
        .mode = D2D1_COMPOSITE_MODE_SOURCE_OVER,
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR shadow_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Shadow'/>           \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Shadow'/>           \
    <Inputs >                                                             \
      <Input name='Source'/>                                              \
    </Inputs>                                                             \
    <Property name='BlurStandardDeviation' type='float' />                \
    <Property name='Color' type='vector4' />                              \
    <Property name='Optimization' type='enum' />                          \
  </Effect>";

struct shadow_properties
{
    float blur_standard_deviation;
    D2D_VECTOR_4F color;
    D2D1_SHADOW_OPTIMIZATION optimization;
};

EFFECT_PROPERTY_RW(shadow, blur_standard_deviation, FLOAT)
EFFECT_PROPERTY_RW(shadow, color, VECTOR4)
EFFECT_PROPERTY_RW(shadow, optimization, ENUM)

static const D2D1_PROPERTY_BINDING shadow_bindings[] =
{
    { L"BlurStandardDeviation", BINDING_RW(shadow, blur_standard_deviation) },
    { L"Color", BINDING_RW(shadow, color) },
    { L"Optimization", BINDING_RW(shadow, optimization) },
};

static HRESULT __stdcall shadow_factory(IUnknown **effect)
{
    static const struct shadow_properties properties =
    {
        .blur_standard_deviation = 3.0f,
        .color = { 0.0f, 0.0f, 0.0f, 1.0f },
        .optimization = D2D1_SHADOW_OPTIMIZATION_BALANCED,
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR grayscale_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Grayscale'/>        \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Grayscale'/>        \
    <Inputs >                                                             \
      <Input name='Source'/>                                              \
    </Inputs>                                                             \
  </Effect>";

static HRESULT __stdcall grayscale_factory(IUnknown **effect)
{
    return d2d_effect_create_impl(effect, NULL, 0);
}

static const WCHAR color_matrix_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Color Matrix'/>     \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Color Matrix'/>     \
    <Inputs >                                                             \
      <Input name='Source'/>                                              \
    </Inputs>                                                             \
    <Property name='ColorMatrix' type='matrix5x4' />                      \
    <Property name='AlphaMode' type='enum' />                             \
    <Property name='ClampOutput' type='bool' />                           \
  </Effect>";

struct color_matrix_properties
{
    D2D1_MATRIX_5X4_F color_matrix;
    D2D1_COLORMATRIX_ALPHA_MODE alpha_mode;
    BOOL clamp_output;
};

EFFECT_PROPERTY_RW(color_matrix, color_matrix, MATRIX_5X4)
EFFECT_PROPERTY_RW(color_matrix, alpha_mode, ENUM)
EFFECT_PROPERTY_RW(color_matrix, clamp_output, BOOL)

static const D2D1_PROPERTY_BINDING color_matrix_bindings[] =
{
    { L"ColorMatrix", BINDING_RW(color_matrix, color_matrix) },
    { L"AlphaMode", BINDING_RW(color_matrix, alpha_mode) },
    { L"ClampOutput", BINDING_RW(color_matrix, clamp_output) },
};

static HRESULT __stdcall color_matrix_factory(IUnknown **effect)
{
    static const struct color_matrix_properties properties =
    {
        .color_matrix = { ._11 = 1.0f, ._22 = 1.0f, ._33 = 1.0f, ._44 = 1.0f },
        .alpha_mode = D2D1_COLORMATRIX_ALPHA_MODE_PREMULTIPLIED,
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR flood_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Flood'/>            \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Flood'/>            \
    <Inputs minimum='0' maximum='0' >                                     \
    </Inputs>                                                             \
    <Property name='Color' type='vector4' />                              \
  </Effect>";

struct flood_properties
{
    D2D_VECTOR_4F color;
};

EFFECT_PROPERTY_RW(flood, color, VECTOR4)

static const D2D1_PROPERTY_BINDING flood_bindings[] =
{
    { L"Color", BINDING_RW(flood, color) },
};

static HRESULT __stdcall flood_factory(IUnknown **effect)
{
    static const struct flood_properties properties =
    {
        .color = {0.0f, 0.0f, 0.0f, 1.0f},
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR gaussian_blur_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Gaussian Blur'/>    \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Gaussian Blur'/>    \
    <Inputs>                                                              \
      <Input name='Source'/>                                              \
    </Inputs>                                                             \
    <Property name='StandardDeviation' type='float' />                    \
    <Property name='Optimization' type='enum' />                          \
    <Property name='BorderMode' type='enum' />                            \
  </Effect>";

struct gaussian_blur_properties
{
    float standard_deviation;
    D2D1_GAUSSIANBLUR_OPTIMIZATION optimization;
    D2D1_BORDER_MODE border_mode;
};

EFFECT_PROPERTY_RW(gaussian_blur, standard_deviation, FLOAT)
EFFECT_PROPERTY_RW(gaussian_blur, optimization, ENUM)
EFFECT_PROPERTY_RW(gaussian_blur, border_mode, ENUM)

static const D2D1_PROPERTY_BINDING gaussian_blur_bindings[] =
{
    { L"StandardDeviation", BINDING_RW(gaussian_blur, standard_deviation) },
    { L"Optimization", BINDING_RW(gaussian_blur, optimization) },
    { L"BorderMode", BINDING_RW(gaussian_blur, border_mode) },
};

static HRESULT __stdcall gaussian_blur_factory(IUnknown **effect)
{
    static const struct gaussian_blur_properties properties =
    {
        .standard_deviation = 3.0f,
        .optimization = D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED,
        .border_mode = D2D1_BORDER_MODE_SOFT,
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR point_specular_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Point Specular'/>   \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Point Specular'/>   \
    <Inputs>                                                              \
      <Input name='Source'/>                                              \
    </Inputs>                                                             \
    <Property name='LightPosition' type='vector3' />                      \
    <Property name='SpecularExponent' type='float' />                     \
    <Property name='SpecularConstant' type='float' />                     \
    <Property name='SurfaceScale' type='float' />                         \
    <Property name='Color' type='vector3' />                              \
    <Property name='KernelUnitLength' type='vector2' />                   \
    <Property name='ScaleMode' type='enum' />                             \
  </Effect>";

struct point_specular_properties
{
    D2D_VECTOR_3F light_position;
    float specular_exponent;
    float specular_constant;
    float surface_scale;
    D2D_VECTOR_3F color;
    D2D_VECTOR_2F kernel_unit_length;
    D2D1_POINTSPECULAR_SCALE_MODE scale_mode;
};

EFFECT_PROPERTY_RW(point_specular, light_position, VECTOR3)
EFFECT_PROPERTY_RW(point_specular, specular_exponent, FLOAT)
EFFECT_PROPERTY_RW(point_specular, specular_constant, FLOAT)
EFFECT_PROPERTY_RW(point_specular, surface_scale, FLOAT)
EFFECT_PROPERTY_RW(point_specular, color, VECTOR3)
EFFECT_PROPERTY_RW(point_specular, kernel_unit_length, VECTOR2)
EFFECT_PROPERTY_RW(point_specular, scale_mode, ENUM)

static const D2D1_PROPERTY_BINDING point_specular_bindings[] =
{
    { L"LightPosition", BINDING_RW(point_specular, light_position) },
    { L"SpecularExponent", BINDING_RW(point_specular, specular_exponent) },
    { L"SpecularConstant", BINDING_RW(point_specular, specular_constant) },
    { L"SurfaceScale", BINDING_RW(point_specular, surface_scale) },
    { L"Color", BINDING_RW(point_specular, color) },
    { L"KernelUnitLength", BINDING_RW(point_specular, kernel_unit_length) },
    { L"ScaleMode", BINDING_RW(point_specular, scale_mode) },
};

static HRESULT __stdcall point_specular_factory(IUnknown **effect)
{
    static const struct point_specular_properties properties =
    {
        .specular_exponent = 1.0f,
        .specular_constant = 1.0f,
        .surface_scale = 1.0f,
        .color = { 1.0f, 1.0f, 1.0f },
        .kernel_unit_length = { 1.0f, 1.0f },
        .scale_mode = D2D1_POINTSPECULAR_SCALE_MODE_LINEAR,
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR arithmetic_composite_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Arithmetic Composite'/> \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Arithmetic Composite'/> \
    <Inputs minimum='2' maximum='2' >                                     \
      <Input name='Source1'/>                                             \
      <Input name='Source2'/>                                             \
    </Inputs>                                                             \
    <Property name='Coefficients' type='vector4' />                       \
    <Property name='ClampOutput' type='bool' />                           \
  </Effect>";

struct arithmetic_composite_properties
{
    D2D_VECTOR_4F coefficients;
    BOOL clamp_output;
};

EFFECT_PROPERTY_RW(arithmetic_composite, coefficients, VECTOR4)
EFFECT_PROPERTY_RW(arithmetic_composite, clamp_output, BOOL)

static const D2D1_PROPERTY_BINDING arithmetic_composite_bindings[] =
{
    { L"Coefficients", BINDING_RW(arithmetic_composite, coefficients) },
    { L"ClampOutput", BINDING_RW(arithmetic_composite, clamp_output) },
};

static HRESULT __stdcall arithmetic_composite_factory(IUnknown **effect)
{
    static const struct arithmetic_composite_properties properties =
    {
        .coefficients = { 1.0f, 0.0f, 0.0f, 0.0f },
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR blend_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Blend'/>            \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Blend'/>            \
    <Inputs minimum='2' maximum='2' >                                     \
      <Input name='Source1'/>                                             \
      <Input name='Source2'/>                                             \
    </Inputs>                                                             \
    <Property name='Mode' type='enum' />                                  \
  </Effect>";

struct blend_properties
{
    D2D1_BLEND_MODE mode;
};

EFFECT_PROPERTY_RW(blend, mode, ENUM)

static const D2D1_PROPERTY_BINDING blend_bindings[] =
{
    { L"Mode", BINDING_RW(blend, mode) },
};

static HRESULT __stdcall blend_factory(IUnknown **effect)
{
    static const struct blend_properties properties = {};
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR brightness_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Brightness'/>       \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Brightness'/>       \
    <Inputs>                                                              \
      <Input name='Source'/>                                              \
    </Inputs>                                                             \
    <Property name='WhitePoint' type='vector2' />                         \
    <Property name='BlackPoint' type='vector2' />                         \
  </Effect>";

struct brightness_properties
{
    D2D_VECTOR_2F white_point;
    D2D_VECTOR_2F black_point;
};

EFFECT_PROPERTY_RW(brightness, white_point, VECTOR2)
EFFECT_PROPERTY_RW(brightness, black_point, VECTOR2)

static const D2D1_PROPERTY_BINDING brightness_bindings[] =
{
    { L"WhitePoint", BINDING_RW(brightness, white_point) },
    { L"BlackPoint", BINDING_RW(brightness, black_point) },
};

static HRESULT __stdcall brightness_factory(IUnknown **effect)
{
    static const struct brightness_properties properties =
    {
        .white_point = { 1.0f, 1.0f },
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR directional_blur_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Directional Blur'/> \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Directional Blur'/> \
    <Inputs>                                                              \
      <Input name='Source'/>                                              \
    </Inputs>                                                             \
    <Property name='StandardDeviation' type='float' />                    \
    <Property name='Angle' type='float' />                                \
    <Property name='Optimization' type='enum' />                          \
    <Property name='BorderMode' type='enum' />                            \
  </Effect>";

struct directional_blur_properties
{
    float standard_deviation;
    float angle;
    D2D1_DIRECTIONALBLUR_OPTIMIZATION optimization;
    D2D1_BORDER_MODE border_mode;
};

EFFECT_PROPERTY_RW(directional_blur, standard_deviation, FLOAT)
EFFECT_PROPERTY_RW(directional_blur, angle, FLOAT)
EFFECT_PROPERTY_RW(directional_blur, optimization, ENUM)
EFFECT_PROPERTY_RW(directional_blur, border_mode, ENUM)

static const D2D1_PROPERTY_BINDING directional_blur_bindings[] =
{
    { L"StandardDeviation", BINDING_RW(directional_blur, standard_deviation) },
    { L"Angle", BINDING_RW(directional_blur, angle) },
    { L"Optimization", BINDING_RW(directional_blur, optimization) },
    { L"BorderMode", BINDING_RW(directional_blur, border_mode) },
};

static HRESULT __stdcall directional_blur_factory(IUnknown **effect)
{
    static const struct directional_blur_properties properties =
    {
        .standard_deviation = 3.0f,
        .optimization = D2D1_DIRECTIONALBLUR_OPTIMIZATION_BALANCED,
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR hue_rotation_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Hue Rotation'/>     \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Hue Rotation'/>     \
    <Inputs>                                                              \
      <Input name='Source'/>                                              \
    </Inputs>                                                             \
    <Property name='Angle' type='float' />                                \
  </Effect>";

struct hue_rotation_properties
{
    float angle;
};

EFFECT_PROPERTY_RW(hue_rotation, angle, FLOAT)

static const D2D1_PROPERTY_BINDING hue_rotation_bindings[] =
{
    { L"Angle", BINDING_RW(hue_rotation, angle) },
};

static HRESULT __stdcall hue_rotation_factory(IUnknown **effect)
{
    static const struct hue_rotation_properties properties = {};
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR saturation_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Saturation'/>       \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Saturation'/>       \
    <Inputs>                                                              \
      <Input name='Source'/>                                              \
    </Inputs>                                                             \
    <Property name='Saturation' type='float' />                           \
  </Effect>";

struct saturation_properties
{
    float saturation;
};

EFFECT_PROPERTY_RW(saturation, saturation, FLOAT)

static const D2D1_PROPERTY_BINDING saturation_bindings[] =
{
    { L"Saturation", BINDING_RW(saturation, saturation) },
};

static HRESULT __stdcall saturation_factory(IUnknown **effect)
{
    static const struct saturation_properties properties =
    {
        .saturation = 0.5f,
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR scale_description[] =
L"<?xml version='1.0'?>                                                   \
  <Effect>                                                                \
    <Property name='DisplayName' type='string' value='Scale'/>            \
    <Property name='Author'      type='string' value='The Wine Project'/> \
    <Property name='Category'    type='string' value='Stub'/>             \
    <Property name='Description' type='string' value='Scale'/>            \
    <Inputs>                                                              \
      <Input name='Source'/>                                              \
    </Inputs>                                                             \
    <Property name='Scale' type='vector2' />                              \
    <Property name='CenterPoint' type='vector2' />                        \
    <Property name='InterpolationMode' type='enum' />                     \
    <Property name='BorderMode' type='enum' />                            \
    <Property name='Sharpness' type='float' />                            \
  </Effect>";

struct scale_properties
{
    D2D_VECTOR_2F scale;
    D2D_VECTOR_2F center_point;
    D2D1_BORDER_MODE border_mode;
    float sharpness;
    D2D1_SCALE_INTERPOLATION_MODE interpolation_mode;
};

EFFECT_PROPERTY_RW(scale, scale, VECTOR2)
EFFECT_PROPERTY_RW(scale, center_point, VECTOR2)
EFFECT_PROPERTY_RW(scale, border_mode, ENUM)
EFFECT_PROPERTY_RW(scale, sharpness, FLOAT)
EFFECT_PROPERTY_RW(scale, interpolation_mode, ENUM)

static const D2D1_PROPERTY_BINDING scale_bindings[] =
{
    { L"Scale", BINDING_RW(scale, scale) },
    { L"CenterPoint", BINDING_RW(scale, center_point) },
    { L"BorderMode", BINDING_RW(scale, border_mode) },
    { L"Sharpness", BINDING_RW(scale, sharpness) },
    { L"InterpolationMode", BINDING_RW(scale, interpolation_mode) },
};

static HRESULT __stdcall scale_factory(IUnknown **effect)
{
    static const struct scale_properties properties =
    {
        .scale = { 1.0f, 1.0f },
        .border_mode = D2D1_BORDER_MODE_SOFT,
        .interpolation_mode = D2D1_SCALE_INTERPOLATION_MODE_LINEAR,
    };
    return d2d_effect_create_impl(effect, &properties, sizeof(properties));
}

static const WCHAR opacity_metadata_description[] = L"<?xml version='1.0'?><Effect>"
    L"<Property name='DisplayName' type='string' value='Opacity Metadata'/>"
    L"<Property name='Author' type='string' value='The Wine Project'/>"
    L"<Property name='Category' type='string' value='Color'/>"
    L"<Property name='Description' type='string' value='Annotates the opaque region of an input image'/>"
    L"<Inputs minimum='1' maximum='1'><Input name='Source'/></Inputs>"
    L"<Property name='InputOpaqueRect' type='vector4'/></Effect>";

struct opacity_metadata_properties
{
    D2D1_VECTOR_4F input_opaque_rect;
};

EFFECT_PROPERTY_RW(opacity_metadata, input_opaque_rect, VECTOR4)

static const D2D1_PROPERTY_BINDING opacity_metadata_bindings[] =
{
    {L"InputOpaqueRect", BINDING_RW(opacity_metadata, input_opaque_rect)},
};

static HRESULT STDMETHODCALLTYPE opacity_metadata_Initialize(ID2D1EffectImpl *iface,
        ID2D1EffectContext *context, ID2D1TransformGraph *graph)
{
    return ID2D1TransformGraph_SetPassthroughGraph(graph, 0);
}

static HRESULT STDMETHODCALLTYPE opacity_metadata_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return ID2D1TransformGraph_SetPassthroughGraph(graph, 0);
}

static const ID2D1EffectImplVtbl opacity_metadata_vtbl =
{
    d2d_effect_impl_QueryInterface,
    d2d_effect_impl_AddRef,
    d2d_effect_impl_Release,
    opacity_metadata_Initialize,
    d2d_effect_impl_PrepareForRender,
    opacity_metadata_SetGraph,
};

static HRESULT CALLBACK opacity_metadata_factory(IUnknown **effect)
{
    static const struct opacity_metadata_properties properties =
    {
        {-INFINITY, -INFINITY, INFINITY, INFINITY},
    };
    HRESULT hr;

    if (SUCCEEDED(hr = d2d_effect_create_impl(effect, &properties, sizeof(properties))))
        ((ID2D1EffectImpl *)*effect)->lpVtbl = &opacity_metadata_vtbl;
    return hr;
}

void d2d_effects_init_builtins(struct d2d_factory *factory)
{
    static const struct builtin_description
    {
        const CLSID *clsid;
        const WCHAR *description;
        PD2D1_EFFECT_FACTORY factory;
        const D2D1_PROPERTY_BINDING *bindings;
        UINT32 binding_count;
    }
    builtin_effects[] =
    {
#define X(name) name##_description, name##_factory
#define X2(name) name##_description, name##_factory, name##_bindings, ARRAY_SIZE(name##_bindings)
        { &CLSID_D2D12DAffineTransform, X2(_2d_affine_transform) },
        { &CLSID_D2D13DPerspectiveTransform, X2(_3d_perspective_transform) },
        { &CLSID_D2D1Composite, X2(composite) },
        { &CLSID_D2D1Shadow, X2(shadow) },
        { &CLSID_D2D1Grayscale, X(grayscale) },
        { &CLSID_D2D1ColorMatrix, X2(color_matrix) },
        { &CLSID_D2D1Flood, X2(flood) },
        { &CLSID_D2D1GaussianBlur, X2(gaussian_blur) },
        { &CLSID_D2D1PointSpecular, X2(point_specular) },
        { &CLSID_D2D1ArithmeticComposite, X2(arithmetic_composite) },
        { &CLSID_D2D1Blend, X2(blend) },
        { &CLSID_D2D1Brightness, X2(brightness) },
        { &CLSID_D2D1DirectionalBlur, X2(directional_blur) },
        { &CLSID_D2D1HueRotation, X2(hue_rotation) },
        { &CLSID_D2D1Saturation, X2(saturation) },
        { &CLSID_D2D1Scale, X2(scale) },
        { &CLSID_D2D1OpacityMetadata, X2(opacity_metadata) },
#undef X2
#undef X
    };
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE(builtin_effects); ++i)
    {
        const struct builtin_description *desc = &builtin_effects[i];

        if (FAILED(hr = d2d_factory_register_builtin_effect(factory, desc->clsid, desc->description,
                desc->bindings, desc->binding_count, desc->factory)))
        {
            WARN("Failed to register the effect %s, hr %#lx.\n", wine_dbgstr_guid(desc->clsid), hr);
        }
    }
    d2d_histogram_init_builtin(factory);
    d2d_alpha_mask_init_builtin(factory);
    d2d_convolve_matrix_init_builtin(factory);
    d2d_contrast_init_builtin(factory);
    d2d_bitmap_source_init_builtin(factory);
    d2d_emboss_init_builtin(factory);
    d2d_opacity_init_builtin(factory);
    d2d_alpha_conversion_init_builtin(factory);
    d2d_crop_init_builtin(factory);
    d2d_white_level_init_builtin(factory);
}

/* Same syntax is used for value and default values. */
static HRESULT d2d_effect_parse_float_array(D2D1_PROPERTY_TYPE type, const WCHAR *value,
        float *vec)
{
    unsigned int i, num_components;
    WCHAR *end_ptr;

    /* Type values are sequential. */
    switch (type)
    {
        case D2D1_PROPERTY_TYPE_VECTOR2:
        case D2D1_PROPERTY_TYPE_VECTOR3:
        case D2D1_PROPERTY_TYPE_VECTOR4:
            num_components = (type - D2D1_PROPERTY_TYPE_VECTOR2) + 2;
            break;
        case D2D1_PROPERTY_TYPE_MATRIX_3X2:
            num_components = 6;
            break;
        case D2D1_PROPERTY_TYPE_MATRIX_4X3:
        case D2D1_PROPERTY_TYPE_MATRIX_4X4:
        case D2D1_PROPERTY_TYPE_MATRIX_5X4:
            num_components = (type - D2D1_PROPERTY_TYPE_MATRIX_4X3) * 4 + 12;
            break;
        default:
            return E_UNEXPECTED;
    }

    if (*(value++) != '(') return E_INVALIDARG;

    for (i = 0; i < num_components; ++i)
    {
        vec[i] = wcstof(value, &end_ptr);
        if (value == end_ptr) return E_INVALIDARG;
        value = end_ptr;

        /* Trailing characters after last component are ignored. */
        if (i == num_components - 1) continue;
        if (*(value++) != ',') return E_INVALIDARG;
    }

    return S_OK;
}

static HRESULT d2d_effect_properties_internal_add(struct d2d_effect_properties *props,
        const WCHAR *name, UINT32 index, BOOL subprop, D2D1_PROPERTY_TYPE type, const WCHAR *value)
{
    static const UINT32 sizes[] =
    {
        [D2D1_PROPERTY_TYPE_UNKNOWN]       = 0,
        [D2D1_PROPERTY_TYPE_STRING]        = 0,
        [D2D1_PROPERTY_TYPE_BOOL]          = sizeof(BOOL),
        [D2D1_PROPERTY_TYPE_UINT32]        = sizeof(UINT32),
        [D2D1_PROPERTY_TYPE_INT32]         = sizeof(INT32),
        [D2D1_PROPERTY_TYPE_FLOAT]         = sizeof(float),
        [D2D1_PROPERTY_TYPE_VECTOR2]       = sizeof(D2D_VECTOR_2F),
        [D2D1_PROPERTY_TYPE_VECTOR3]       = sizeof(D2D_VECTOR_3F),
        [D2D1_PROPERTY_TYPE_VECTOR4]       = sizeof(D2D_VECTOR_4F),
        [D2D1_PROPERTY_TYPE_BLOB]          = 0,
        [D2D1_PROPERTY_TYPE_IUNKNOWN]      = sizeof(IUnknown *),
        [D2D1_PROPERTY_TYPE_ENUM]          = sizeof(UINT32),
        [D2D1_PROPERTY_TYPE_ARRAY]         = sizeof(UINT32),
        [D2D1_PROPERTY_TYPE_CLSID]         = sizeof(CLSID),
        [D2D1_PROPERTY_TYPE_MATRIX_3X2]    = sizeof(D2D_MATRIX_3X2_F),
        [D2D1_PROPERTY_TYPE_MATRIX_4X3]    = sizeof(D2D_MATRIX_4X3_F),
        [D2D1_PROPERTY_TYPE_MATRIX_4X4]    = sizeof(D2D_MATRIX_4X4_F),
        [D2D1_PROPERTY_TYPE_MATRIX_5X4]    = sizeof(D2D_MATRIX_5X4_F),
        [D2D1_PROPERTY_TYPE_COLOR_CONTEXT] = sizeof(ID2D1ColorContext *),
    };
    struct d2d_effect_property *p;
    HRESULT hr;

    assert(type >= D2D1_PROPERTY_TYPE_STRING && type <= D2D1_PROPERTY_TYPE_COLOR_CONTEXT);

    if (!d2d_array_reserve((void **)&props->properties, &props->size, props->count + 1,
            sizeof(*props->properties)))
    {
        return E_OUTOFMEMORY;
    }

    /* TODO: we could save some space for properties that have both getter and setter. */
    if (!d2d_array_reserve((void **)&props->data.ptr, &props->data.size,
            props->data.count + sizes[type], sizeof(*props->data.ptr)))
    {
        return E_OUTOFMEMORY;
    }
    props->data.count += sizes[type];

    p = &props->properties[props->count++];
    memset(p, 0, sizeof(*p));
    p->index = index;
    if (p->index < 0x80000000)
    {
        props->custom_count++;
        /* FIXME: this should probably be controlled by subproperty */
        p->readonly = FALSE;
    }
    else if (subprop)
        p->readonly = TRUE;
    else
        p->readonly = index != D2D1_PROPERTY_CACHED && index != D2D1_PROPERTY_PRECISION;
    p->name = wcsdup(name);
    p->type = type;
    if (p->type == D2D1_PROPERTY_TYPE_STRING)
    {
        p->data.ptr = wcsdup(value);
        p->size = value ? (wcslen(value) + 1) * sizeof(WCHAR) : sizeof(WCHAR);
    }
    else if (p->type == D2D1_PROPERTY_TYPE_BLOB)
    {
        p->data.ptr = NULL;
        p->size = 0;
    }
    else
    {
        void *src = NULL;
        WCHAR *end_ptr;
        UINT32 _uint32;
        float _vec[20];
        CLSID _clsid;
        BOOL _bool;

        p->data.offset = props->offset;
        p->size = sizes[type];
        props->offset += p->size;

        if (value)
        {
            switch (p->type)
            {
                case D2D1_PROPERTY_TYPE_UINT32:
                case D2D1_PROPERTY_TYPE_INT32:
                    _uint32 = wcstoul(value, NULL, 0);
                    src = &_uint32;
                    break;
                case D2D1_PROPERTY_TYPE_FLOAT:
                    _vec[0] = wcstof(value, &end_ptr);
                    src = &_vec[0];
                    break;
                case D2D1_PROPERTY_TYPE_ENUM:
                    _uint32 = wcstoul(value, NULL, 10);
                    src = &_uint32;
                    break;
                case D2D1_PROPERTY_TYPE_BOOL:
                    if (!wcscmp(value, L"true")) _bool = TRUE;
                    else if (!wcscmp(value, L"false")) _bool = FALSE;
                    else return E_INVALIDARG;
                    src = &_bool;
                    break;
                case D2D1_PROPERTY_TYPE_CLSID:
                    CLSIDFromString(value, &_clsid);
                    src = &_clsid;
                    break;
                case D2D1_PROPERTY_TYPE_VECTOR2:
                case D2D1_PROPERTY_TYPE_VECTOR3:
                case D2D1_PROPERTY_TYPE_VECTOR4:
                case D2D1_PROPERTY_TYPE_MATRIX_3X2:
                case D2D1_PROPERTY_TYPE_MATRIX_4X3:
                case D2D1_PROPERTY_TYPE_MATRIX_4X4:
                case D2D1_PROPERTY_TYPE_MATRIX_5X4:
                    if (FAILED(hr = d2d_effect_parse_float_array(p->type, value, _vec)))
                    {
                        WARN("Failed to parse float array %s for type %u.\n",
                                wine_dbgstr_w(value), p->type);
                        return hr;
                    }
                    src = _vec;
                    break;
                case D2D1_PROPERTY_TYPE_IUNKNOWN:
                case D2D1_PROPERTY_TYPE_COLOR_CONTEXT:
                    break;
                default:
                    FIXME("Initial value for property type %u is not handled.\n", p->type);
            }

            if (src && p->size) memcpy(props->data.ptr + p->data.offset, src, p->size);
        }
        else if (p->size)
            memset(props->data.ptr + p->data.offset, 0, p->size);
    }

    return S_OK;
}

HRESULT d2d_effect_properties_add(struct d2d_effect_properties *props, const WCHAR *name,
        UINT32 index, D2D1_PROPERTY_TYPE type, const WCHAR *value)
{
    return d2d_effect_properties_internal_add(props, name, index, FALSE, type, value);
}

HRESULT d2d_effect_subproperties_add(struct d2d_effect_properties *props, const WCHAR *name,
        UINT32 index, D2D1_PROPERTY_TYPE type, const WCHAR *value)
{
    return d2d_effect_properties_internal_add(props, name, index, TRUE, type, value);
}

static HRESULT d2d_effect_duplicate_properties(struct d2d_effect *effect,
        struct d2d_effect_properties *dst, const struct d2d_effect_properties *src)
{
    HRESULT hr;
    size_t i;

    *dst = *src;
    dst->effect = effect;

    if (!(dst->data.ptr = malloc(dst->data.size)))
        return E_OUTOFMEMORY;
    memcpy(dst->data.ptr, src->data.ptr, dst->data.size);

    if (!(dst->properties = calloc(dst->size, sizeof(*dst->properties))))
        return E_OUTOFMEMORY;

    for (i = 0; i < dst->count; ++i)
    {
        struct d2d_effect_property *d = &dst->properties[i];
        const struct d2d_effect_property *s = &src->properties[i];

        *d = *s;
        d->name = wcsdup(s->name);
        if (d->type == D2D1_PROPERTY_TYPE_STRING)
            d->data.ptr = wcsdup((WCHAR *)s->data.ptr);

        if (s->subproperties)
        {
            if (!(d->subproperties = calloc(1, sizeof(*d->subproperties))))
                return E_OUTOFMEMORY;
            if (FAILED(hr = d2d_effect_duplicate_properties(effect, d->subproperties, s->subproperties)))
                return hr;
        }
    }

    return S_OK;
}

static struct d2d_effect_property * d2d_effect_properties_get_property_by_index(
        const struct d2d_effect_properties *properties, UINT32 index)
{
    unsigned int i;

    for (i = 0; i < properties->count; ++i)
    {
        if (properties->properties[i].index == index)
            return &properties->properties[i];
    }

    return NULL;
}

struct d2d_effect_property * d2d_effect_properties_get_property_by_name(
        const struct d2d_effect_properties *properties, const WCHAR *name)
{
    unsigned int i;

    for (i = 0; i < properties->count; ++i)
    {
        if (!wcscmp(properties->properties[i].name, name))
            return &properties->properties[i];
    }

    return NULL;
}

static UINT32 d2d_effect_properties_get_value_size(const struct d2d_effect_properties *properties,
        UINT32 index)
{
    struct d2d_effect *effect = properties->effect;
    struct d2d_effect_property *prop;
    UINT32 size;

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return 0;

    if (effect && prop->get_function)
    {
        if (FAILED(prop->get_function(effect->impl_unknown, NULL, 0, &size))) return 0;
        return size;
    }

    return prop->size;
}

static HRESULT d2d_effect_return_string(const WCHAR *str, WCHAR *buffer, UINT32 buffer_size)
{
    UINT32 size = str ? wcslen(str) : 0;
    if (size >= buffer_size) return D2DERR_INSUFFICIENT_BUFFER;
    if (str) memcpy(buffer, str, (size + 1) * sizeof(*buffer));
    else *buffer = 0;
    return S_OK;
}

static HRESULT d2d_effect_property_get_value(const struct d2d_effect_properties *properties,
        const struct d2d_effect_property *prop, D2D1_PROPERTY_TYPE type, BYTE *value, UINT32 size)
{
    struct d2d_effect *effect = properties->effect;
    PD2D1_PROPERTY_GET_FUNCTION get_function = effect ? prop->get_function : NULL;
    UINT32 actual_size;

    memset(value, 0, size);

    if (type != D2D1_PROPERTY_TYPE_UNKNOWN && prop->type != type) return E_INVALIDARG;
    /* Do not check sizes for variable-length properties. */
    if (prop->type != D2D1_PROPERTY_TYPE_STRING
            && prop->type != D2D1_PROPERTY_TYPE_BLOB
            && prop->size != size)
    {
        return E_INVALIDARG;
    }

    if (get_function)
        return get_function(effect->impl_unknown, value, size, &actual_size);

    switch (prop->type)
    {
        case D2D1_PROPERTY_TYPE_BLOB:
            memset(value, 0, size);
            break;
        case D2D1_PROPERTY_TYPE_STRING:
            return d2d_effect_return_string(prop->data.ptr, (WCHAR *)value, size / sizeof(WCHAR));
        default:
            memcpy(value, properties->data.ptr + prop->data.offset, size);
            break;
    }

    return S_OK;
}

HRESULT d2d_effect_property_get_uint32_value(const struct d2d_effect_properties *properties,
        const struct d2d_effect_property *prop, UINT32 *value)
{
    return d2d_effect_property_get_value(properties, prop, D2D1_PROPERTY_TYPE_UINT32,
            (BYTE *)value, sizeof(*value));
}

static HRESULT d2d_effect_property_set_value(struct d2d_effect_properties *properties,
        struct d2d_effect_property *prop, D2D1_PROPERTY_TYPE type, const BYTE *value, UINT32 size)
{
    struct d2d_effect *effect = properties->effect;
    if (prop->readonly || !effect) return E_INVALIDARG;
    if (type != D2D1_PROPERTY_TYPE_UNKNOWN && prop->type != type) return E_INVALIDARG;
    if (prop->get_function && !prop->set_function) return E_INVALIDARG;
    if (prop->index < 0x80000000 && !prop->set_function) return E_INVALIDARG;

    if (prop->set_function)
    {
        HRESULT hr = prop->set_function(effect->impl_unknown, value, size);
        if (SUCCEEDED(hr)) effect->changes |= D2D1_CHANGE_TYPE_PROPERTIES;
        return hr;
    }

    if (prop->size != size) return E_INVALIDARG;

    switch (prop->type)
    {
        case D2D1_PROPERTY_TYPE_BOOL:
        case D2D1_PROPERTY_TYPE_UINT32:
        case D2D1_PROPERTY_TYPE_ENUM:
            memcpy(properties->data.ptr + prop->data.offset, value, size);
            break;
        default:
            FIXME("Unhandled type %u.\n", prop->type);
    }

    return S_OK;
}

void d2d_effect_properties_cleanup(struct d2d_effect_properties *props)
{
    struct d2d_effect_property *p;
    size_t i;

    for (i = 0; i < props->count; ++i)
    {
        p = &props->properties[i];
        free(p->name);
        if (p->type == D2D1_PROPERTY_TYPE_STRING)
            free(p->data.ptr);
        if (p->subproperties)
        {
            d2d_effect_properties_cleanup(p->subproperties);
            free(p->subproperties);
        }
    }
    free(props->properties);
    free(props->data.ptr);
}

static inline struct d2d_effect_context *impl_from_ID2D1EffectContext1(ID2D1EffectContext1 *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_effect_context, ID2D1EffectContext1_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_QueryInterface(ID2D1EffectContext1 *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1EffectContext1)
            || IsEqualGUID(iid, &IID_ID2D1EffectContext)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1EffectContext1_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_effect_context_AddRef(ID2D1EffectContext1 *iface)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);
    ULONG refcount = InterlockedIncrement(&effect_context->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_effect_context_Release(ID2D1EffectContext1 *iface)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);
    ULONG refcount = InterlockedDecrement(&effect_context->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        ID2D1DeviceContext6_Release(&effect_context->device_context->ID2D1DeviceContext6_iface);
        free(effect_context);
    }

    return refcount;
}

static void STDMETHODCALLTYPE d2d_effect_context_GetDpi(ID2D1EffectContext1 *iface, float *dpi_x, float *dpi_y)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);

    TRACE("iface %p, dpi_x %p, dpi_y %p.\n", iface, dpi_x, dpi_y);

    ID2D1DeviceContext6_GetDpi(&effect_context->device_context->ID2D1DeviceContext6_iface, dpi_x, dpi_y);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateEffect(ID2D1EffectContext1 *iface,
        REFCLSID clsid, ID2D1Effect **effect)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);

    TRACE("iface %p, clsid %s, effect %p.\n", iface, debugstr_guid(clsid), effect);

    return ID2D1DeviceContext6_CreateEffect(&effect_context->device_context->ID2D1DeviceContext6_iface,
            clsid, effect);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_GetMaximumSupportedFeatureLevel(ID2D1EffectContext1 *iface,
        const D3D_FEATURE_LEVEL *levels, UINT32 level_count, D3D_FEATURE_LEVEL *max_level)
{
    static const D3D_FEATURE_LEVEL supported_levels[] =
    {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1,
    };
    struct d2d_effect_context *context = impl_from_ID2D1EffectContext1(iface);
    struct d2d_device *device = context->device_context->device;
    ID3D11Device1 *d3d = context->device_context->d3d_device;
    D3D_FEATURE_LEVEL supported;
    HRESULT hr = D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES;
    UINT32 i, flags;

    TRACE("iface %p, levels %p, level_count %u, max_level %p.\n", iface, levels, level_count, max_level);
    if (!levels || !max_level) return E_INVALIDARG;
    if (!level_count) return hr;
    supported = InterlockedCompareExchange(&device->max_feature_level, 0, 0);
    if (!supported)
    {
        /* Context states expose the adapter's capabilities even when the caller
         * created its D3D device at a lower emulated level. Direct2D caps this at
         * 11.1. A NULL state pointer queries support without changing active state. */
        flags = ID3D11Device1_GetCreationFlags(d3d) & D3D11_CREATE_DEVICE_SINGLETHREADED
                ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED : 0;
        hr = ID3D11Device1_CreateDeviceContextState(d3d, flags, supported_levels, ARRAY_SIZE(supported_levels),
                D3D11_SDK_VERSION, &IID_ID3D11Device1, &supported, NULL);
        if (FAILED(hr)) return hr;
        InterlockedCompareExchange(&device->max_feature_level, supported, 0);
    }
    /* Native replaces lower compatible candidates in list order, stopping as
     * soon as the device's maximum is present. Preserve output on failure. */
    hr = D2DERR_INSUFFICIENT_DEVICE_CAPABILITIES;
    for (i = 0; i < level_count; ++i)
    {
        if ((int)levels[i] > (int)supported) continue;
        *max_level = levels[i];
        hr = S_OK;
        if (levels[i] == supported) break;
    }
    return hr;
}

static HRESULT d2d_effect_node_create(ID2D1Effect *effect, ID2D1TransformNode **node);

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateTransformNodeFromEffect(ID2D1EffectContext1 *iface,
        ID2D1Effect *effect, ID2D1TransformNode **node)
{
    TRACE("iface %p, effect %p, node %p.\n", iface, effect, node);

    return d2d_effect_node_create(effect, node);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateBlendTransform(ID2D1EffectContext1 *iface,
        UINT32 num_inputs, const D2D1_BLEND_DESCRIPTION *description, ID2D1BlendTransform **transform)
{
    TRACE("iface %p, num_inputs %u, description %p, transform %p,\n", iface, num_inputs, description, transform);

    return d2d_blend_transform_create(num_inputs, description, transform);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateBorderTransform(ID2D1EffectContext1 *iface,
        D2D1_EXTEND_MODE mode_x, D2D1_EXTEND_MODE mode_y, ID2D1BorderTransform **transform)
{
    TRACE("iface %p, mode_x %#x, mode_y %#x, transform %p.\n", iface, mode_x, mode_y, transform);

    return d2d_border_transform_create(mode_x, mode_y, transform);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateOffsetTransform(ID2D1EffectContext1 *iface,
        D2D1_POINT_2L offset, ID2D1OffsetTransform **transform)
{
    TRACE("iface %p, offset %s, transform %p.\n", iface, debug_d2d_point_2l(&offset), transform);

    return d2d_offset_transform_create(offset, transform);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateBoundsAdjustmentTransform(ID2D1EffectContext1 *iface,
        const D2D1_RECT_L *output_rect, ID2D1BoundsAdjustmentTransform **transform)
{
    TRACE("iface %p, output_rect %s, transform %p.\n", iface, debug_d2d_rect_l(output_rect), transform);

    return d2d_bounds_adjustment_transform_create(output_rect, transform);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_LoadPixelShader(ID2D1EffectContext1 *iface,
        REFGUID shader_id, const BYTE *buffer, UINT32 buffer_size)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);
    struct d2d_device *device = effect_context->device_context->device;
    ID3D11PixelShader *shader;
    HRESULT hr;

    TRACE("iface %p, shader_id %s, buffer %p, buffer_size %u.\n",
            iface, debugstr_guid(shader_id), buffer, buffer_size);

    if (d2d_device_get_indexed_object(&device->shaders, shader_id, NULL))
        return S_OK;

    if (FAILED(hr = ID3D11Device1_CreatePixelShader(effect_context->device_context->d3d_device,
            buffer, buffer_size, NULL, &shader)))
    {
        WARN("Failed to create a pixel shader, hr %#lx.\n", hr);
        return hr;
    }

    hr = d2d_device_add_indexed_object(&device->shaders, shader_id, (IUnknown *)shader);
    ID3D11PixelShader_Release(shader);

    return hr;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_LoadVertexShader(ID2D1EffectContext1 *iface,
        REFGUID shader_id, const BYTE *buffer, UINT32 buffer_size)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);
    struct d2d_device *device = effect_context->device_context->device;
    ID3D11VertexShader *shader;
    HRESULT hr;

    TRACE("iface %p, shader_id %s, buffer %p, buffer_size %u.\n",
            iface, debugstr_guid(shader_id), buffer, buffer_size);

    if (d2d_device_get_indexed_object(&device->shaders, shader_id, NULL))
        return S_OK;

    if (FAILED(hr = ID3D11Device1_CreateVertexShader(effect_context->device_context->d3d_device,
            buffer, buffer_size, NULL, &shader)))
    {
        WARN("Failed to create vertex shader, hr %#lx.\n", hr);
        return hr;
    }

    hr = d2d_device_add_indexed_object(&device->shaders, shader_id, (IUnknown *)shader);
    ID3D11VertexShader_Release(shader);

    return hr;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_LoadComputeShader(ID2D1EffectContext1 *iface,
        REFGUID shader_id, const BYTE *buffer, UINT32 buffer_size)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);
    struct d2d_device *device = effect_context->device_context->device;
    ID3D11ComputeShader *shader;
    HRESULT hr;

    TRACE("iface %p, shader_id %s, buffer %p, buffer_size %u.\n",
            iface, debugstr_guid(shader_id), buffer, buffer_size);

    if (d2d_device_get_indexed_object(&device->shaders, shader_id, NULL))
        return S_OK;

    if (FAILED(hr = ID3D11Device1_CreateComputeShader(effect_context->device_context->d3d_device,
            buffer, buffer_size, NULL, &shader)))
    {
        WARN("Failed to create a compute shader, hr %#lx.\n", hr);
        return hr;
    }

    hr = d2d_device_add_indexed_object(&device->shaders, shader_id, (IUnknown *)shader);
    ID3D11ComputeShader_Release(shader);

    return hr;
}

static BOOL STDMETHODCALLTYPE d2d_effect_context_IsShaderLoaded(ID2D1EffectContext1 *iface, REFGUID shader_id)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);
    struct d2d_device *device = effect_context->device_context->device;

    TRACE("iface %p, shader_id %s.\n", iface, debugstr_guid(shader_id));

    return d2d_device_get_indexed_object(&device->shaders, shader_id, NULL);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateResourceTexture(ID2D1EffectContext1 *iface,
        const GUID *id,  const D2D1_RESOURCE_TEXTURE_PROPERTIES *texture_properties,
        const BYTE *data, const UINT32 *strides, UINT32 data_size, ID2D1ResourceTexture **texture)
{
    FIXME("iface %p, id %s, texture_properties %p, data %p, strides %p, data_size %u, texture %p stub!\n",
            iface, debugstr_guid(id), texture_properties, data, strides, data_size, texture);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_FindResourceTexture(ID2D1EffectContext1 *iface,
        const GUID *id, ID2D1ResourceTexture **texture)
{
    FIXME("iface %p, id %s, texture %p stub!\n", iface, debugstr_guid(id), texture);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateVertexBuffer(ID2D1EffectContext1 *iface,
        const D2D1_VERTEX_BUFFER_PROPERTIES *buffer_properties, const GUID *id,
        const D2D1_CUSTOM_VERTEX_BUFFER_PROPERTIES *custom_buffer_properties,
        ID2D1VertexBuffer **buffer)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);
    struct d2d_device_context *context = effect_context->device_context;
    HRESULT hr;

    FIXME("iface %p, buffer_properties %p, id %s, custom_buffer_properties %p, buffer %p stub!\n",
            iface, buffer_properties, debugstr_guid(id), custom_buffer_properties, buffer);

    if (id && d2d_device_get_indexed_object(&context->vertex_buffers, id, (IUnknown **)buffer))
        return S_OK;

    if (SUCCEEDED(hr = d2d_vertex_buffer_create(buffer)))
    {
        if (id)
            hr = d2d_device_add_indexed_object(&context->vertex_buffers, id, (IUnknown *)*buffer);
    }

    if (FAILED(hr))
        *buffer = NULL;

    return hr;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_FindVertexBuffer(ID2D1EffectContext1 *iface,
        const GUID *id, ID2D1VertexBuffer **buffer)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);
    struct d2d_device_context *context = effect_context->device_context;

    TRACE("iface %p, id %s, buffer %p.\n", iface, debugstr_guid(id), buffer);

    if (!d2d_device_get_indexed_object(&context->vertex_buffers, id, (IUnknown **)buffer))
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateColorContext(ID2D1EffectContext1 *iface,
        D2D1_COLOR_SPACE space, const BYTE *profile, UINT32 profile_size, ID2D1ColorContext **color_context)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);

    TRACE("iface %p, space %#x, profile %p, profile_size %u, color_context %p.\n",
            iface, space, profile, profile_size, color_context);

    return ID2D1DeviceContext6_CreateColorContext(&effect_context->device_context->ID2D1DeviceContext6_iface,
            space, profile, profile_size, color_context);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateColorContextFromFilename(ID2D1EffectContext1 *iface,
        const WCHAR *filename, ID2D1ColorContext **color_context)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);

    TRACE("iface %p, filename %s, color_context %p.\n", iface, debugstr_w(filename), color_context);

    return ID2D1DeviceContext6_CreateColorContextFromFilename(&effect_context->device_context->ID2D1DeviceContext6_iface,
            filename, color_context);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateColorContextFromWicColorContext(ID2D1EffectContext1 *iface,
        IWICColorContext *wic_color_context, ID2D1ColorContext **color_context)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);

    TRACE("iface %p, wic_color_context %p, color_context %p.\n", iface, wic_color_context, color_context);

    return ID2D1DeviceContext6_CreateColorContextFromWicColorContext(&effect_context->device_context->ID2D1DeviceContext6_iface,
            wic_color_context, color_context);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CheckFeatureSupport(ID2D1EffectContext1 *iface,
        D2D1_FEATURE feature, void *data, UINT32 data_size)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);
    D3D11_FEATURE d3d11_feature;

    TRACE("iface %p, feature %#x, data %p, data_size %u.\n", iface, feature, data, data_size);

    /* Data structures are compatible. */
    switch (feature)
    {
        case D2D1_FEATURE_DOUBLES: d3d11_feature = D3D11_FEATURE_DOUBLES; break;
        case D2D1_FEATURE_D3D10_X_HARDWARE_OPTIONS: d3d11_feature = D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS; break;
        default:
            WARN("Unexpected feature index %d.\n", feature);
            return E_INVALIDARG;
    }

    return ID3D11Device1_CheckFeatureSupport(effect_context->device_context->d3d_device,
            d3d11_feature, data, data_size);
}

static BOOL STDMETHODCALLTYPE d2d_effect_context_IsBufferPrecisionSupported(ID2D1EffectContext1 *iface,
        D2D1_BUFFER_PRECISION precision)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);

    TRACE("iface %p, precision %u.\n", iface, precision);

    return ID2D1DeviceContext6_IsBufferPrecisionSupported(&effect_context->device_context->ID2D1DeviceContext6_iface,
            precision);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateLookupTable3D(ID2D1EffectContext1 *iface,
        D2D1_BUFFER_PRECISION precision, const UINT32 *extents, const BYTE *data,
        UINT32 data_count, const UINT32 *strides, ID2D1LookupTable3D **lookup_table)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext1(iface);

    TRACE("iface %p, precision %u, extents %p, data %p, data_count %u, strides %p, lookup_table %p.\n",
            iface, precision, extents, data, data_count, strides, lookup_table);

    return d2d_lookup_table_create(effect_context->device_context, precision, extents,
            data, data_count, strides, lookup_table);
}

static const ID2D1EffectContext1Vtbl d2d_effect_context_vtbl =
{
    d2d_effect_context_QueryInterface,
    d2d_effect_context_AddRef,
    d2d_effect_context_Release,
    d2d_effect_context_GetDpi,
    d2d_effect_context_CreateEffect,
    d2d_effect_context_GetMaximumSupportedFeatureLevel,
    d2d_effect_context_CreateTransformNodeFromEffect,
    d2d_effect_context_CreateBlendTransform,
    d2d_effect_context_CreateBorderTransform,
    d2d_effect_context_CreateOffsetTransform,
    d2d_effect_context_CreateBoundsAdjustmentTransform,
    d2d_effect_context_LoadPixelShader,
    d2d_effect_context_LoadVertexShader,
    d2d_effect_context_LoadComputeShader,
    d2d_effect_context_IsShaderLoaded,
    d2d_effect_context_CreateResourceTexture,
    d2d_effect_context_FindResourceTexture,
    d2d_effect_context_CreateVertexBuffer,
    d2d_effect_context_FindVertexBuffer,
    d2d_effect_context_CreateColorContext,
    d2d_effect_context_CreateColorContextFromFilename,
    d2d_effect_context_CreateColorContextFromWicColorContext,
    d2d_effect_context_CheckFeatureSupport,
    d2d_effect_context_IsBufferPrecisionSupported,
    d2d_effect_context_CreateLookupTable3D,
};

void d2d_effect_context_init(struct d2d_effect_context *effect_context, struct d2d_device_context *device_context)
{
    effect_context->ID2D1EffectContext1_iface.lpVtbl = &d2d_effect_context_vtbl;
    effect_context->refcount = 1;
    effect_context->device_context = device_context;
    ID2D1DeviceContext6_AddRef(&device_context->ID2D1DeviceContext6_iface);
}

static inline struct d2d_effect *impl_from_ID2D1Effect(ID2D1Effect *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_effect, ID2D1Effect_iface);
}

static void d2d_effect_cleanup(struct d2d_effect *effect)
{
    unsigned int i;

    for (i = 0; i < effect->input_count; ++i)
    {
        if (effect->inputs[i])
            ID2D1Image_Release(effect->inputs[i]);
    }
    free(effect->inputs);
    ID2D1EffectContext1_Release(&effect->effect_context->ID2D1EffectContext1_iface);
    if (effect->graph)
        ID2D1TransformGraph_Release(&effect->graph->ID2D1TransformGraph_iface);
    d2d_effect_properties_cleanup(&effect->properties);
    if (effect->impl)
        ID2D1EffectImpl_Release(effect->impl);
    if (effect->impl_unknown)
        IUnknown_Release(effect->impl_unknown);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_QueryInterface(ID2D1Effect *iface, REFIID iid, void **out)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1Effect)
            || IsEqualGUID(iid, &IID_ID2D1Properties)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1Effect_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_ID2D1Image)
            || IsEqualGUID(iid, &IID_ID2D1Resource))
    {
        ID2D1Image_AddRef(&effect->ID2D1Image_iface);
        *out = &effect->ID2D1Image_iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_effect_AddRef(ID2D1Effect *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);
    ULONG refcount = InterlockedIncrement(&effect->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_effect_Release(ID2D1Effect *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);
    ULONG refcount = InterlockedDecrement(&effect->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        d2d_effect_cleanup(effect);
        free(effect);
    }

    return refcount;
}

static UINT32 STDMETHODCALLTYPE d2d_effect_GetPropertyCount(ID2D1Effect *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p.\n", iface);

    return ID2D1Properties_GetPropertyCount(&effect->properties.ID2D1Properties_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_GetPropertyName(ID2D1Effect *iface, UINT32 index,
        WCHAR *name, UINT32 name_count)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %u, name %p, name_count %u.\n", iface, index, name, name_count);

    return ID2D1Properties_GetPropertyName(&effect->properties.ID2D1Properties_iface,
            index, name, name_count);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_GetPropertyNameLength(ID2D1Effect *iface, UINT32 index)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %u.\n", iface, index);

    return ID2D1Properties_GetPropertyNameLength(&effect->properties.ID2D1Properties_iface, index);
}

static D2D1_PROPERTY_TYPE STDMETHODCALLTYPE d2d_effect_GetType(ID2D1Effect *iface, UINT32 index)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %#x.\n", iface, index);

    return ID2D1Properties_GetType(&effect->properties.ID2D1Properties_iface, index);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_GetPropertyIndex(ID2D1Effect *iface, const WCHAR *name)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, name %s.\n", iface, debugstr_w(name));

    return ID2D1Properties_GetPropertyIndex(&effect->properties.ID2D1Properties_iface, name);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_SetValueByName(ID2D1Effect *iface, const WCHAR *name,
        D2D1_PROPERTY_TYPE type, const BYTE *value, UINT32 value_size)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, name %s, type %u, value %p, value_size %u.\n", iface, debugstr_w(name),
            type, value, value_size);

    return ID2D1Properties_SetValueByName(&effect->properties.ID2D1Properties_iface, name,
            type, value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_SetValue(ID2D1Effect *iface, UINT32 index, D2D1_PROPERTY_TYPE type,
        const BYTE *value, UINT32 value_size)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %#x, type %u, value %p, value_size %u.\n", iface, index, type, value, value_size);

    return ID2D1Properties_SetValue(&effect->properties.ID2D1Properties_iface, index, type,
            value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_GetValueByName(ID2D1Effect *iface, const WCHAR *name,
        D2D1_PROPERTY_TYPE type, BYTE *value, UINT32 value_size)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, name %s, type %#x, value %p, value_size %u.\n", iface, debugstr_w(name), type,
            value, value_size);

    return ID2D1Properties_GetValueByName(&effect->properties.ID2D1Properties_iface, name, type,
            value, value_size);
}

static HRESULT d2d_effect_get_value(struct d2d_effect *effect, UINT32 index, D2D1_PROPERTY_TYPE type,
        BYTE *value, UINT32 value_size)
{
    return ID2D1Properties_GetValue(&effect->properties.ID2D1Properties_iface, index, type, value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_GetValue(ID2D1Effect *iface, UINT32 index, D2D1_PROPERTY_TYPE type,
        BYTE *value, UINT32 value_size)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %#x, type %u, value %p, value_size %u.\n", iface, index, type, value, value_size);

    return d2d_effect_get_value(effect, index, type, value, value_size);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_GetValueSize(ID2D1Effect *iface, UINT32 index)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %#x.\n", iface, index);

    return ID2D1Properties_GetValueSize(&effect->properties.ID2D1Properties_iface, index);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_GetSubProperties(ID2D1Effect *iface, UINT32 index,
        ID2D1Properties **props)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %u, props %p.\n", iface, index, props);

    return ID2D1Properties_GetSubProperties(&effect->properties.ID2D1Properties_iface, index, props);
}

static void STDMETHODCALLTYPE d2d_effect_SetInput(ID2D1Effect *iface, UINT32 index, ID2D1Image *input, BOOL invalidate)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %u, input %p, invalidate %#x.\n", iface, index, input, invalidate);

    if (index >= effect->input_count)
        return;

    if (input)
        ID2D1Image_AddRef(input);
    if (effect->inputs[index])
        ID2D1Image_Release(effect->inputs[index]);
    effect->inputs[index] = input;
}

static HRESULT d2d_effect_set_input_count(struct d2d_effect *effect, UINT32 count)
{
    bool initialized = effect->inputs != NULL;
    HRESULT hr = S_OK;
    unsigned int i;

    if (count == effect->input_count)
        return S_OK;

    if (count < effect->input_count)
    {
        for (i = count; i < effect->input_count; ++i)
        {
            if (effect->inputs[i])
                ID2D1Image_Release(effect->inputs[i]);
        }
    }
    else
    {
        if (!d2d_array_reserve((void **)&effect->inputs, &effect->inputs_size,
                count, sizeof(*effect->inputs)))
        {
            ERR("Failed to resize inputs array.\n");
            return E_OUTOFMEMORY;
        }

        memset(&effect->inputs[effect->input_count], 0, sizeof(*effect->inputs) * (count - effect->input_count));
    }
    effect->input_count = count;

    if (initialized)
    {
        ID2D1TransformGraph_Release(&effect->graph->ID2D1TransformGraph_iface);
        effect->graph = NULL;

        if (SUCCEEDED(hr = d2d_transform_graph_create(count, &effect->graph)))
        {
            if (FAILED(hr = ID2D1EffectImpl_SetGraph(effect->impl, &effect->graph->ID2D1TransformGraph_iface)))
                WARN("Failed to set a new transform graph, hr %#lx.\n", hr);
        }
    }

    return hr;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_SetInputCount(ID2D1Effect *iface, UINT32 count)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);
    unsigned int min_inputs, max_inputs;

    TRACE("iface %p, count %u.\n", iface, count);

    d2d_effect_get_value(effect, D2D1_PROPERTY_MIN_INPUTS, D2D1_PROPERTY_TYPE_UINT32,
            (BYTE *)&min_inputs, sizeof(min_inputs));
    d2d_effect_get_value(effect, D2D1_PROPERTY_MAX_INPUTS, D2D1_PROPERTY_TYPE_UINT32,
            (BYTE *)&max_inputs, sizeof(max_inputs));

    if (count < min_inputs || count > max_inputs)
        return E_INVALIDARG;

    return d2d_effect_set_input_count(effect, count);
}

static void STDMETHODCALLTYPE d2d_effect_GetInput(ID2D1Effect *iface, UINT32 index, ID2D1Image **input)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %u, input %p.\n", iface, index, input);

    if (index < effect->input_count && effect->inputs[index])
        ID2D1Image_AddRef(*input = effect->inputs[index]);
    else
        *input = NULL;
}

static UINT32 STDMETHODCALLTYPE d2d_effect_GetInputCount(ID2D1Effect *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p.\n", iface);

    return effect->input_count;
}

static void STDMETHODCALLTYPE d2d_effect_GetOutput(ID2D1Effect *iface, ID2D1Image **output)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, output %p.\n", iface, output);

    ID2D1Image_AddRef(*output = &effect->ID2D1Image_iface);
}

static const ID2D1EffectVtbl d2d_effect_vtbl =
{
    d2d_effect_QueryInterface,
    d2d_effect_AddRef,
    d2d_effect_Release,
    d2d_effect_GetPropertyCount,
    d2d_effect_GetPropertyName,
    d2d_effect_GetPropertyNameLength,
    d2d_effect_GetType,
    d2d_effect_GetPropertyIndex,
    d2d_effect_SetValueByName,
    d2d_effect_SetValue,
    d2d_effect_GetValueByName,
    d2d_effect_GetValue,
    d2d_effect_GetValueSize,
    d2d_effect_GetSubProperties,
    d2d_effect_SetInput,
    d2d_effect_SetInputCount,
    d2d_effect_GetInput,
    d2d_effect_GetInputCount,
    d2d_effect_GetOutput,
};

static inline struct d2d_effect *impl_from_ID2D1Image(ID2D1Image *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_effect, ID2D1Image_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_image_QueryInterface(ID2D1Image *iface, REFIID iid, void **out)
{
    struct d2d_effect *effect = impl_from_ID2D1Image(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    return d2d_effect_QueryInterface(&effect->ID2D1Effect_iface, iid, out);
}

static ULONG STDMETHODCALLTYPE d2d_effect_image_AddRef(ID2D1Image *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Image(iface);

    TRACE("iface %p.\n", iface);

    return d2d_effect_AddRef(&effect->ID2D1Effect_iface);
}

static ULONG STDMETHODCALLTYPE d2d_effect_image_Release(ID2D1Image *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Image(iface);

    TRACE("iface %p.\n", iface);

    return d2d_effect_Release(&effect->ID2D1Effect_iface);
}

static void STDMETHODCALLTYPE d2d_effect_image_GetFactory(ID2D1Image *iface, ID2D1Factory **factory)
{
    struct d2d_effect *effect = impl_from_ID2D1Image(iface);

    TRACE("iface %p, factory %p.\n", iface, factory);

    ID2D1Factory_AddRef(*factory = effect->effect_context->device_context->factory);
}

static const ID2D1ImageVtbl d2d_effect_image_vtbl =
{
    d2d_effect_image_QueryInterface,
    d2d_effect_image_AddRef,
    d2d_effect_image_Release,
    d2d_effect_image_GetFactory,
};

struct d2d_effect_node
{
    ID2D1TransformNode ID2D1TransformNode_iface;
    LONG refcount;
    struct d2d_effect *effect;
};

static struct d2d_effect_node *impl_from_effect_node(ID2D1TransformNode *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_effect_node, ID2D1TransformNode_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_node_QueryInterface(ID2D1TransformNode *iface, REFIID iid, void **out)
{
    if (!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_ID2D1TransformNode))
        return E_NOINTERFACE;
    ID2D1TransformNode_AddRef(iface);
    *out = iface;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE d2d_effect_node_AddRef(ID2D1TransformNode *iface)
{
    return InterlockedIncrement(&impl_from_effect_node(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE d2d_effect_node_Release(ID2D1TransformNode *iface)
{
    struct d2d_effect_node *node = impl_from_effect_node(iface);
    ULONG refcount = InterlockedDecrement(&node->refcount);
    if (!refcount)
    {
        ID2D1Effect_Release(&node->effect->ID2D1Effect_iface);
        free(node);
    }
    return refcount;
}

static UINT32 STDMETHODCALLTYPE d2d_effect_node_GetInputCount(ID2D1TransformNode *iface)
{
    return impl_from_effect_node(iface)->effect->input_count;
}

static const ID2D1TransformNodeVtbl d2d_effect_node_vtbl =
{
    d2d_effect_node_QueryInterface,
    d2d_effect_node_AddRef,
    d2d_effect_node_Release,
    d2d_effect_node_GetInputCount,
};

static HRESULT d2d_effect_node_create(ID2D1Effect *effect, ID2D1TransformNode **out)
{
    struct d2d_effect_node *node;

    if (!out) return E_INVALIDARG;
    *out = NULL;
    if (!effect || effect->lpVtbl != &d2d_effect_vtbl) return E_INVALIDARG;
    if (!(node = calloc(1, sizeof(*node)))) return E_OUTOFMEMORY;
    node->ID2D1TransformNode_iface.lpVtbl = &d2d_effect_node_vtbl;
    node->refcount = 1;
    node->effect = impl_from_ID2D1Effect(effect);
    ID2D1Effect_AddRef(effect);
    *out = &node->ID2D1TransformNode_iface;
    return S_OK;
}

static HRESULT d2d_effect_transform_graph_initialize_nodes(struct d2d_transform_graph *graph,
        struct d2d_device *device);

static HRESULT d2d_effect_prepare(struct d2d_effect *effect, struct d2d_device_context *context)
{
    float dpi_x = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiX;
    float dpi_y = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiY;
    HRESULT hr;

    if (effect->render_dpi_x != dpi_x || effect->render_dpi_y != dpi_y)
        effect->changes |= D2D1_CHANGE_TYPE_CONTEXT;
    if (effect->changes)
    {
        if (FAILED(hr = ID2D1EffectImpl_PrepareForRender(effect->impl, effect->changes))) return hr;
        effect->changes = D2D1_CHANGE_TYPE_NONE;
        effect->render_dpi_x = dpi_x;
        effect->render_dpi_y = dpi_y;
    }
    return d2d_effect_transform_graph_initialize_nodes(effect->graph, context->device);
}

static HRESULT d2d_custom_effect_evaluate(struct d2d_effect *effect, struct d2d_device_context *context,
        const D2D1_RECT_L *region, struct d2d_effect_image *output, BOOL bounds_only, BOOL linkable_output)
{
    struct d2d_transform_node *node = effect->graph->output;
    ID2D1DrawTransform *transform;
    D2D1_RECT_L opaque;
    HRESULT hr;

    if (!node) return S_FALSE;
    if (FAILED(ID2D1TransformNode_QueryInterface(node->object, &IID_ID2D1DrawTransform, (void **)&transform)))
        return S_FALSE;
    ID2D1DrawTransform_Release(transform);
    if (effect->input_count || node->input_count) return S_FALSE;
    /* PrepareForRender is allowed to replace the transform graph. */
    if (FAILED(hr = d2d_effect_prepare(effect, context))) return hr;
    node = effect->graph->output;
    if (!node || node->input_count) return D2DERR_INVALID_GRAPH_CONFIGURATION;
    if (FAILED(hr = ID2D1TransformNode_QueryInterface(node->object, &IID_ID2D1DrawTransform, (void **)&transform)))
        return hr;
    hr = ID2D1DrawTransform_MapInputRectsToOutputRect(transform, NULL, NULL, 0, &output->rect, &opaque);
    ID2D1DrawTransform_Release(transform);
    if (FAILED(hr) || bounds_only) return hr;
    if (region)
    {
        output->rect.left = max(output->rect.left, region->left);
        output->rect.top = max(output->rect.top, region->top);
        output->rect.right = min(output->rect.right, region->right);
        output->rect.bottom = min(output->rect.bottom, region->bottom);
    }
    output->rect.right = max(output->rect.left, output->rect.right);
    output->rect.bottom = max(output->rect.top, output->rect.bottom);
    return d2d_custom_effect_render(context, node->render_info, NULL, 0, linkable_output, output);
}

static HRESULT d2d_custom_node_evaluate(struct d2d_device_context *context, struct d2d_transform_node *node,
        const struct d2d_effect_image *inputs, UINT count, const D2D1_RECT_L *region,
        BOOL bounds_only, BOOL linkable, struct d2d_effect_image *output)
{
    D2D1_RECT_L rects[8], opaque_rects[8] = {{0}}, requested[8], opaque;
    ID2D1DrawTransform *transform;
    unsigned int i;
    HRESULT hr;

    if (count > ARRAY_SIZE(rects)) return E_NOTIMPL;
    if (FAILED(hr = ID2D1TransformNode_QueryInterface(node->object, &IID_ID2D1DrawTransform,
            (void **)&transform))) return E_NOTIMPL;
    for (i = 0; i < count; ++i)
    {
        if (inputs[i].bitmap)
        {
            struct d2d_bitmap *bitmap = unsafe_impl_from_ID2D1Bitmap(inputs[i].bitmap);
            if (bitmap->format.alphaMode == D2D1_ALPHA_MODE_IGNORE) opaque_rects[i] = inputs[i].rect;
        }
        rects[i] = inputs[i].rect;
    }
    hr = ID2D1DrawTransform_MapInputRectsToOutputRect(transform, count ? rects : NULL,
            count ? opaque_rects : NULL, count, &output->rect, &opaque);
    if (FAILED(hr) || bounds_only) goto done;
    if (region)
    {
        output->rect.left = max(output->rect.left, region->left);
        output->rect.top = max(output->rect.top, region->top);
        output->rect.right = min(output->rect.right, region->right);
        output->rect.bottom = min(output->rect.bottom, region->bottom);
    }
    output->rect.right = max(output->rect.left, output->rect.right);
    output->rect.bottom = max(output->rect.top, output->rect.bottom);
    if (FAILED(hr = ID2D1DrawTransform_MapOutputRectToInputRects(transform, &output->rect,
            count ? requested : NULL, count))) goto done;
    hr = d2d_custom_effect_render(context, node->render_info, inputs, count, linkable, output);
done:
    ID2D1DrawTransform_Release(transform);
    return hr;
}

enum d2d_effect_kind
{
    EFFECT_PASSTHROUGH, EFFECT_GRAPH, EFFECT_ALPHA_MASK, EFFECT_CONVOLVE_MATRIX,
    EFFECT_CONTRAST, EFFECT_EMBOSS, EFFECT_OPACITY,
    EFFECT_PREMULTIPLY, EFFECT_UNPREMULTIPLY, EFFECT_WHITE_LEVEL, EFFECT_DRAW_TRANSFORM, EFFECT_OFFSET, EFFECT_INVERT, EFFECT_CROP,
};

struct d2d_evaluation_source
{
    ID2D1Image *image;
    struct d2d_transform_node *node;
    size_t scope;
};

struct d2d_evaluation_frame
{
    struct d2d_effect *effect;
    struct d2d_effect_image inputs[8];
    struct d2d_evaluation_source sources[8];
    struct d2d_transform_node *binding;
    struct d2d_transform_node *draw_node;
    size_t scope;
    enum d2d_effect_kind kind;
    D2D1_POINT_2L offset;
    D2D1_RECT_L region;
    unsigned int count, next;
};

/* A graph input is an evaluation-local binding, not a SetInput on the wrapped
 * effect. Follow outer graph bindings iteratively to support nested wrappers. */
static HRESULT d2d_effect_input_source(struct d2d_effect *effect, struct d2d_transform_node *binding,
        size_t scope, unsigned int index, const struct d2d_evaluation_frame *frames,
        struct d2d_evaluation_source *source)
{
    const struct d2d_evaluation_frame *outer;
    const struct d2d_transform_graph *graph;
    unsigned int i;

    memset(source, 0, sizeof(*source));
    while (binding)
    {
        if (index >= binding->input_count || binding->input_count != effect->input_count)
            return D2DERR_INVALID_GRAPH_CONFIGURATION;
        if (binding->inputs[index])
        {
            source->node = binding->inputs[index];
            source->scope = scope;
            return S_OK;
        }
        outer = &frames[scope];
        graph = outer->effect->graph;
        i = binding->effect_inputs[index];
        if (i >= graph->input_count) return D2DERR_INVALID_GRAPH_CONFIGURATION;
        effect = outer->effect;
        binding = outer->binding;
        scope = outer->scope;
        index = i;
    }
    if (index >= effect->input_count || !effect->inputs[index]) return D2DERR_INVALID_GRAPH_CONFIGURATION;
    source->image = effect->inputs[index];
    return S_OK;
}

/* Intermediate rectangles are in effect pixels and may have negative origins.
 * Frames own completed inputs; graph pointers are borrowed for this evaluation. */
static LONG d2d_offset_coordinate(LONG coordinate, LONG offset, BOOL inverse)
{
    LONGLONG value = coordinate;
    if (coordinate == INT_MIN || coordinate == INT_MAX) return coordinate;
    value += inverse ? -(LONGLONG)offset : (LONGLONG)offset;
    return max(INT_MIN, min(INT_MAX, value));
}

static void d2d_offset_rect(D2D1_RECT_L *rect, D2D1_POINT_2L offset, BOOL inverse)
{
    rect->left = d2d_offset_coordinate(rect->left, offset.x, inverse);
    rect->right = d2d_offset_coordinate(rect->right, offset.x, inverse);
    rect->top = d2d_offset_coordinate(rect->top, offset.y, inverse);
    rect->bottom = d2d_offset_coordinate(rect->bottom, offset.y, inverse);
}

static HRESULT d2d_effect_evaluate(struct d2d_device_context *context, ID2D1Image *image,
        struct d2d_effect_image *output, BOOL bounds_only, const D2D1_RECT_L *region)
{
    enum d2d_effect_kind kind;
    struct d2d_evaluation_frame *frames = NULL, *frame;
    struct d2d_evaluation_source source = {image}, sources[8];
    struct d2d_transform_node *draw_node;
    struct d2d_transform_node *binding;
    struct d2d_effect *effect;
    struct d2d_effect_image result = {0};
    ID2D1OffsetTransform *offset_transform;
    D2D1_POINT_2L offset = {0};
    D2D1_RECT_L active_region;
    BOOL effect_image = image && image->lpVtbl == &d2d_effect_image_vtbl;
    size_t capacity = 0, depth = 0, i;
    unsigned int count, j;
    CLSID clsid;
    HRESULT hr;

    memset(output, 0, sizeof(*output));
    if (!image) return E_INVALIDARG;
    if (region) {active_region = *region; region = &active_region;}
    for (;;)
    {
        draw_node = NULL;
        binding = source.node;
        if (binding)
        {
            if (binding->object->lpVtbl != &d2d_effect_node_vtbl)
            {
                const struct d2d_evaluation_frame *owner = &frames[source.scope];
                const struct d2d_transform_graph *graph = owner->effect->graph;
                unsigned int port;

                draw_node = binding;
                effect = owner->effect;
                count = binding->input_count;
                kind = EFFECT_DRAW_TRANSFORM;
                if (SUCCEEDED(ID2D1TransformNode_QueryInterface(binding->object, &IID_ID2D1OffsetTransform,
                        (void **)&offset_transform)))
                {
                    offset = ID2D1OffsetTransform_GetOffset(offset_transform);
                    ID2D1OffsetTransform_Release(offset_transform);
                    if (count != 1) {hr = D2DERR_INVALID_GRAPH_CONFIGURATION; break;}
                    kind = EFFECT_OFFSET;
                }
                if (count > ARRAY_SIZE(sources)) {hr = E_NOTIMPL; break;}
                for (i = 0; i < depth; ++i)
                    if (frames[i].draw_node == draw_node && frames[i].scope == source.scope) break;
                if (i != depth) {hr = D2DERR_CYCLIC_GRAPH; break;}
                if (!count)
                {
                    hr = d2d_custom_node_evaluate(context, draw_node, NULL, 0, region,
                            bounds_only, TRUE, &result);
                    if (SUCCEEDED(hr)) goto have_result;
                    break;
                }
                for (j = 0; j < count; ++j)
                {
                    memset(&sources[j], 0, sizeof(sources[j]));
                    if (draw_node->inputs[j])
                    {
                        sources[j].node = draw_node->inputs[j];
                        sources[j].scope = source.scope;
                        continue;
                    }
                    port = draw_node->effect_inputs[j];
                    if (port >= graph->input_count) {hr = D2DERR_INVALID_GRAPH_CONFIGURATION; break;}
                    if (FAILED(hr = d2d_effect_input_source(effect, owner->binding, owner->scope,
                            port, frames, &sources[j]))) break;
                }
                if (j != count) break;
                goto push_frame;
            }
            image = &impl_from_effect_node(binding->object)->effect->ID2D1Image_iface;
        }
        else image = source.image;
        if (FAILED(ID2D1Image_QueryInterface(image, &IID_ID2D1Bitmap, (void **)&result.bitmap)))
        {
            if (image->lpVtbl != &d2d_effect_image_vtbl)
            {
                hr = depth ? E_NOTIMPL : S_FALSE;
                break;
            }
            effect = impl_from_ID2D1Image(image);
            for (i = 0; i < depth; ++i)
                if (frames[i].effect == effect) break;
            if (i != depth)
            {
                hr = D2DERR_CYCLIC_GRAPH;
                break;
            }
            if (effect->effect_context->device_context->d3d_device != context->d3d_device)
            {
                hr = D2DERR_WRONG_RESOURCE_DOMAIN;
                break;
            }
            if (FAILED(hr = d2d_effect_get_value(effect, D2D1_PROPERTY_CLSID, D2D1_PROPERTY_TYPE_CLSID,
                    (BYTE *)&clsid, sizeof(clsid)))) break;
            /* Source effects are graph leaves with property-owned input data. */
            if (IsEqualGUID(&clsid, &CLSID_D2D1BitmapSource))
            {
                if (FAILED(hr = d2d_bitmap_source_evaluate(effect, context, &result, bounds_only))) break;
                goto have_result;
            }
            count = 1;
            if (effect->impl->lpVtbl == &opacity_metadata_vtbl) kind = EFFECT_PASSTHROUGH;
            else if (IsEqualGUID(&clsid, &CLSID_D2D1AlphaMask)) { kind = EFFECT_ALPHA_MASK; count = 2; }
            else if (IsEqualGUID(&clsid, &CLSID_D2D1ConvolveMatrix)) kind = EFFECT_CONVOLVE_MATRIX;
            else if (IsEqualGUID(&clsid, &CLSID_D2D1Contrast)) kind = EFFECT_CONTRAST;
            else if (IsEqualGUID(&clsid, &CLSID_D2D1Emboss)) kind = EFFECT_EMBOSS;
            else if (IsEqualGUID(&clsid, &CLSID_D2D1Invert)) kind = EFFECT_INVERT;
            else if (IsEqualGUID(&clsid, &CLSID_D2D1Crop)) kind = EFFECT_CROP;
            else if (IsEqualGUID(&clsid, &CLSID_D2D1Opacity)) kind = EFFECT_OPACITY;
            else if (IsEqualGUID(&clsid, &CLSID_D2D1WhiteLevelAdjustment)) kind = EFFECT_WHITE_LEVEL;
            else if (IsEqualGUID(&clsid, &CLSID_D2D1Premultiply)) kind = EFFECT_PREMULTIPLY;
            else if (IsEqualGUID(&clsid, &CLSID_D2D1UnPremultiply)) kind = EFFECT_UNPREMULTIPLY;
            else
            {
                const D2D1_RECT_L *requested = region;
                BOOL linkable = TRUE;
                if (FAILED(hr = d2d_effect_prepare(effect, context))) break;
                if (effect->graph->passthrough)
                {
                    kind = EFFECT_GRAPH;
                    if (FAILED(hr = d2d_effect_input_source(effect, binding, source.scope,
                            effect->graph->passthrough_input, frames, &sources[0]))) break;
                    goto push_frame;
                }
                if (effect->graph->output && (effect->graph->output->object->lpVtbl == &d2d_effect_node_vtbl
                        || effect->input_count))
                {
                    kind = EFFECT_GRAPH;
                    sources[0].image = NULL;
                    sources[0].node = effect->graph->output;
                    sources[0].scope = depth;
                    goto push_frame;
                }
                for (i = 0; i < depth; ++i)
                {
                    if (frames[i].kind == EFFECT_CONVOLVE_MATRIX || frames[i].kind == EFFECT_EMBOSS)
                        requested = NULL; /* These transforms require expanded input regions. */
                    if (frames[i].kind != EFFECT_PASSTHROUGH && frames[i].kind != EFFECT_GRAPH
                            && frames[i].kind != EFFECT_OPACITY && frames[i].kind != EFFECT_OFFSET)
                        linkable = FALSE;
                }
                hr = d2d_custom_effect_evaluate(effect, context, requested, &result, bounds_only, linkable);
                if (hr == S_OK) goto have_result;
                if (hr == S_FALSE && depth) hr = E_NOTIMPL;
                break;
            }
            if (effect->input_count != count)
            {
                hr = D2DERR_WRONG_STATE;
                break;
            }
            for (j = 0; j < count; ++j)
                if (FAILED(hr = d2d_effect_input_source(effect, binding, source.scope, j, frames, &sources[j]))) break;
            if (j != count) break;
push_frame:
            if (!d2d_array_reserve((void **)&frames, &capacity, depth + 1, sizeof(*frames)))
            {
                hr = E_OUTOFMEMORY;
                break;
            }
            frame = &frames[depth++];
            memset(frame, 0, sizeof(*frame));
            frame->effect = effect;
            frame->kind = kind;
            frame->count = count;
            frame->binding = binding;
            frame->draw_node = draw_node;
            frame->scope = source.scope;
            if (region) frame->region = active_region;
            if (kind == EFFECT_OFFSET)
            {
                frame->offset = offset;
                if (region) d2d_offset_rect(&active_region, offset, TRUE);
            }
            memcpy(frame->sources, sources, count * sizeof(*sources));
            source = sources[0];
            continue;
        }
        {
            D2D1_SIZE_U size = ID2D1Bitmap_GetPixelSize(result.bitmap);
            if (effect_image && (unsafe_impl_from_ID2D1Bitmap(result.bitmap)->options & D2D1_BITMAP_OPTIONS_CANNOT_DRAW))
            {
                hr = D2DERR_INVALID_GRAPH_CONFIGURATION;
                break;
            }
            result.rect.left = result.rect.top = 0;
            result.rect.right = size.width;
            result.rect.bottom = size.height;
        }
have_result:
        for (;;)
        {
            if (!depth)
            {
                if (!bounds_only && effect_image)
                {
                    struct d2d_bitmap *shared, *impl = unsafe_impl_from_ID2D1Bitmap(result.bitmap);
                    D2D1_BITMAP_PROPERTIES1 desc = {impl->format,
                            context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiX,
                            context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 96 : context->desc.dpiY,
                            impl->options, NULL};
                    if (desc.dpiX != impl->dpi_x || desc.dpiY != impl->dpi_y)
                    {
                        hr = d2d_bitmap_create_shared(context, &IID_ID2D1Bitmap, result.bitmap, &desc, &shared);
                        if (FAILED(hr)) goto done;
                        ID2D1Bitmap_Release(result.bitmap);
                        result.bitmap = (ID2D1Bitmap *)&shared->ID2D1Bitmap1_iface;
                    }
                }
                if (bounds_only && result.bitmap)
                {
                    ID2D1Bitmap_Release(result.bitmap);
                    result.bitmap = NULL;
                }
                *output = result;
                free(frames);
                return S_OK;
            }
            frame = &frames[depth - 1];
            if (region) active_region = frame->region;
            frame->inputs[frame->next++] = result;
            result.bitmap = NULL;
            if (frame->next < frame->count)
            {
                source = frame->sources[frame->next];
                break;
            }
            result.rect = frame->inputs[0].rect;
            if (frame->kind == EFFECT_OFFSET)
            {
                d2d_offset_rect(&result.rect, frame->offset, FALSE);
                result.bitmap = frame->inputs[0].bitmap;
                frame->inputs[0].bitmap = NULL;
            }
            else if (frame->kind == EFFECT_DRAW_TRANSFORM)
            {
                BOOL linkable = TRUE;
                for (i = 0; i + 1 < depth; ++i)
                    if (frames[i].kind != EFFECT_GRAPH && frames[i].kind != EFFECT_PASSTHROUGH
                            && frames[i].kind != EFFECT_OPACITY && frames[i].kind != EFFECT_OFFSET) linkable = FALSE;
                if (FAILED(hr = d2d_custom_node_evaluate(context, frame->draw_node, frame->inputs,
                        frame->count, region, bounds_only, linkable, &result))) goto done;
            }
            else if (frame->kind == EFFECT_ALPHA_MASK)
            {
                result.rect.left = max(result.rect.left, frame->inputs[1].rect.left);
                result.rect.top = max(result.rect.top, frame->inputs[1].rect.top);
                result.rect.right = max(result.rect.left, min(result.rect.right, frame->inputs[1].rect.right));
                result.rect.bottom = max(result.rect.top, min(result.rect.bottom, frame->inputs[1].rect.bottom));
                if (!bounds_only && FAILED(hr = d2d_alpha_mask_render(frame->effect, context, frame->inputs, &result)))
                    goto done;
            }
            else if (frame->kind == EFFECT_CONVOLVE_MATRIX)
            {
                if (FAILED(hr = d2d_convolve_matrix_bounds(frame->effect, context, &frame->inputs[0].rect, &result.rect)))
                    goto done;
                if (!bounds_only && FAILED(hr = d2d_convolve_matrix_render(frame->effect, context, frame->inputs, &result)))
                    goto done;
            }
            else if (frame->kind == EFFECT_CONTRAST)
            {
                if (!bounds_only && FAILED(hr = d2d_contrast_render(frame->effect, context, frame->inputs, &result)))
                    goto done;
            }
            else if (frame->kind == EFFECT_PREMULTIPLY || frame->kind == EFFECT_UNPREMULTIPLY)
            {
                if (!bounds_only && FAILED(hr = d2d_alpha_conversion_render(frame->effect, context, frame->inputs,
                        frame->kind == EFFECT_UNPREMULTIPLY, &result))) goto done;
            }
            else if (frame->kind == EFFECT_WHITE_LEVEL)
            {
                if (!bounds_only && FAILED(hr = d2d_white_level_render(frame->effect, context, frame->inputs, &result)))
                    goto done;
            }
            else if (frame->kind == EFFECT_CROP)
            {
                if (FAILED(hr = d2d_crop_bounds(frame->effect, context, &frame->inputs[0].rect, &result.rect)))
                    goto done;
                if (!bounds_only && FAILED(hr = d2d_crop_render(frame->effect, context, frame->inputs, &result)))
                    goto done;
            }
            else if (frame->kind == EFFECT_INVERT)
            {
                if (!bounds_only && FAILED(hr = d2d_invert_render(frame->effect, context, frame->inputs, &result)))
                    goto done;
            }
            else if (frame->kind == EFFECT_OPACITY)
            {
                if (!bounds_only && FAILED(hr = d2d_opacity_render(frame->effect, context, frame->inputs, &result)))
                    goto done;
            }
            else if (frame->kind == EFFECT_EMBOSS)
            {
                if (FAILED(hr = d2d_emboss_bounds(context,&frame->inputs[0].rect,&result.rect))) goto done;
                if (!bounds_only && FAILED(hr = d2d_emboss_render(frame->effect,context,frame->inputs,&result))) goto done;
            }
            else
            {
                result.bitmap = frame->inputs[0].bitmap;
                frame->inputs[0].bitmap = NULL;
            }
            for (j = 0; j < frame->count; ++j)
                if (frame->inputs[j].bitmap) ID2D1Bitmap_Release(frame->inputs[j].bitmap);
            --depth;
        }
    }
done:
    if (result.bitmap) ID2D1Bitmap_Release(result.bitmap);
    for (i = 0; i < depth; ++i)
        for (j = 0; j < frames[i].next; ++j)
            if (frames[i].inputs[j].bitmap) ID2D1Bitmap_Release(frames[i].inputs[j].bitmap);
    free(frames);
    return hr;
}

HRESULT d2d_effect_resolve_image(struct d2d_device_context *context, ID2D1Image *image,
        struct d2d_effect_image *output)
{
    return d2d_effect_evaluate(context, image, output, FALSE, NULL);
}

HRESULT d2d_effect_resolve_image_region(struct d2d_device_context *context, ID2D1Image *image,
        const D2D1_RECT_L *region, struct d2d_effect_image *output)
{
    return d2d_effect_evaluate(context, image, output, FALSE, region);
}

HRESULT d2d_effect_get_image_bounds(struct d2d_device_context *context, ID2D1Image *image, D2D1_RECT_F *bounds)
{
    struct d2d_effect_image result;
    ID2D1Bitmap *bitmap;
    float scale_x, scale_y;
    HRESULT hr;

    if (!image || !bounds) return E_INVALIDARG;
    if (SUCCEEDED(ID2D1Image_QueryInterface(image, &IID_ID2D1Bitmap, (void **)&bitmap)))
    {
        D2D1_SIZE_U pixels = ID2D1Bitmap_GetPixelSize(bitmap);
        D2D1_SIZE_F dips = ID2D1Bitmap_GetSize(bitmap);
        bounds->left = bounds->top = 0;
        bounds->right = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? pixels.width : dips.width;
        bounds->bottom = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? pixels.height : dips.height;
        ID2D1Bitmap_Release(bitmap);
        return S_OK;
    }
    if ((hr = d2d_effect_evaluate(context, image, &result, TRUE, NULL)) != S_OK) return hr;
    scale_x = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 1 : 96.0f / context->desc.dpiX;
    scale_y = context->drawing_state.unitMode == D2D1_UNIT_MODE_PIXELS ? 1 : 96.0f / context->desc.dpiY;
    bounds->left = result.rect.left * scale_x;
    bounds->top = result.rect.top * scale_y;
    bounds->right = result.rect.right * scale_x;
    bounds->bottom = result.rect.bottom * scale_y;
    return S_OK;
}

HRESULT d2d_effect_draw_image(struct d2d_device_context *context, ID2D1Image *image,
        const D2D1_RECT_F *image_rect)
{
    struct d2d_effect *effect;
    CLSID clsid;
    HRESULT hr;

    if (image->lpVtbl != &d2d_effect_image_vtbl)
        return S_FALSE;
    effect = impl_from_ID2D1Image(image);
    if (FAILED(hr = d2d_effect_get_value(effect, D2D1_PROPERTY_CLSID, D2D1_PROPERTY_TYPE_CLSID,
            (BYTE *)&clsid, sizeof(clsid))))
        return hr;
    if (IsEqualGUID(&clsid, &CLSID_D2D1Histogram))
        return d2d_histogram_draw(effect, context, image_rect);
    return S_FALSE;
}

static inline struct d2d_effect_properties *impl_from_ID2D1Properties(ID2D1Properties *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_effect_properties, ID2D1Properties_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_QueryInterface(ID2D1Properties *iface,
        REFIID riid, void **obj)
{
    if (IsEqualGUID(riid, &IID_ID2D1Properties) ||
            IsEqualGUID(riid, &IID_IUnknown))
    {
        *obj = iface;
        ID2D1Properties_AddRef(iface);
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_effect_properties_AddRef(ID2D1Properties *iface)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);

    if (properties->effect)
        return ID2D1Effect_AddRef(&properties->effect->ID2D1Effect_iface);

    return InterlockedIncrement(&properties->refcount);
}

static ULONG STDMETHODCALLTYPE d2d_effect_properties_Release(ID2D1Properties *iface)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    ULONG refcount;

    if (properties->effect)
        return ID2D1Effect_Release(&properties->effect->ID2D1Effect_iface);

    refcount = InterlockedDecrement(&properties->refcount);

    if (!refcount)
    {
        d2d_effect_properties_cleanup(properties);
        free(properties);
    }

    return refcount;
}

static UINT32 STDMETHODCALLTYPE d2d_effect_properties_GetPropertyCount(ID2D1Properties *iface)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);

    TRACE("iface %p.\n", iface);

    return properties->custom_count;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_GetPropertyName(ID2D1Properties *iface,
        UINT32 index, WCHAR *name, UINT32 name_count)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %u, name %p, name_count %u.\n", iface, index, name, name_count);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2DERR_INVALID_PROPERTY;

    return d2d_effect_return_string(prop->name, name, name_count);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_properties_GetPropertyNameLength(ID2D1Properties *iface,
        UINT32 index)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %u.\n", iface, index);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2DERR_INVALID_PROPERTY;

    return wcslen(prop->name);
}

static D2D1_PROPERTY_TYPE STDMETHODCALLTYPE d2d_effect_properties_GetType(ID2D1Properties *iface,
        UINT32 index)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %#x.\n", iface, index);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2D1_PROPERTY_TYPE_UNKNOWN;

    return prop->type;
}

static UINT32 STDMETHODCALLTYPE d2d_effect_properties_GetPropertyIndex(ID2D1Properties *iface,
        const WCHAR *name)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, name %s.\n", iface, debugstr_w(name));

    if (!(prop = d2d_effect_properties_get_property_by_name(properties, name)))
        return D2D1_INVALID_PROPERTY_INDEX;

    return prop->index;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_SetValueByName(ID2D1Properties *iface,
        const WCHAR *name, D2D1_PROPERTY_TYPE type, const BYTE *value, UINT32 value_size)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, name %s, type %u, value %p, value_size %u.\n", iface, debugstr_w(name),
            type, value, value_size);

    if (!(prop = d2d_effect_properties_get_property_by_name(properties, name)))
        return D2DERR_INVALID_PROPERTY;

    return d2d_effect_property_set_value(properties, prop, type, value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_SetValue(ID2D1Properties *iface,
        UINT32 index, D2D1_PROPERTY_TYPE type, const BYTE *value, UINT32 value_size)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %#x, type %u, value %p, value_size %u.\n", iface, index, type, value, value_size);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2DERR_INVALID_PROPERTY;

    return d2d_effect_property_set_value(properties, prop, type, value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_GetValueByName(ID2D1Properties *iface,
        const WCHAR *name, D2D1_PROPERTY_TYPE type, BYTE *value, UINT32 value_size)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, name %s, type %#x, value %p, value_size %u.\n", iface, debugstr_w(name), type,
            value, value_size);

    if (!(prop = d2d_effect_properties_get_property_by_name(properties, name)))
        return D2DERR_INVALID_PROPERTY;

    return d2d_effect_property_get_value(properties, prop, type, value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_GetValue(ID2D1Properties *iface,
        UINT32 index, D2D1_PROPERTY_TYPE type, BYTE *value, UINT32 value_size)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %#x, type %u, value %p, value_size %u.\n", iface, index, type, value, value_size);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
    {
        if (value) memset(value, 0, value_size);
        return D2DERR_INVALID_PROPERTY;
    }

    return d2d_effect_property_get_value(properties, prop, type, value, value_size);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_properties_GetValueSize(ID2D1Properties *iface,
        UINT32 index)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);

    TRACE("iface %p, index %#x.\n", iface, index);

    return d2d_effect_properties_get_value_size(properties, index);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_GetSubProperties(ID2D1Properties *iface,
        UINT32 index, ID2D1Properties **props)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %u, props %p.\n", iface, index, props);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2DERR_INVALID_PROPERTY;

    if (!prop->subproperties) return D2DERR_NO_SUBPROPERTIES;

    *props = &prop->subproperties->ID2D1Properties_iface;
    ID2D1Properties_AddRef(*props);
    return S_OK;
}

static const ID2D1PropertiesVtbl d2d_effect_properties_vtbl =
{
    d2d_effect_properties_QueryInterface,
    d2d_effect_properties_AddRef,
    d2d_effect_properties_Release,
    d2d_effect_properties_GetPropertyCount,
    d2d_effect_properties_GetPropertyName,
    d2d_effect_properties_GetPropertyNameLength,
    d2d_effect_properties_GetType,
    d2d_effect_properties_GetPropertyIndex,
    d2d_effect_properties_SetValueByName,
    d2d_effect_properties_SetValue,
    d2d_effect_properties_GetValueByName,
    d2d_effect_properties_GetValue,
    d2d_effect_properties_GetValueSize,
    d2d_effect_properties_GetSubProperties,
};

void d2d_effect_init_properties(struct d2d_effect *effect,
        struct d2d_effect_properties *properties)
{
    properties->ID2D1Properties_iface.lpVtbl = &d2d_effect_properties_vtbl;
    properties->effect = effect;
    properties->refcount = 1;
}

static struct d2d_render_info *impl_from_ID2D1DrawInfo(ID2D1DrawInfo *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_render_info, ID2D1DrawInfo_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_draw_info_QueryInterface(ID2D1DrawInfo *iface, REFIID iid,
        void **obj)
{
    TRACE("iface %p, iid %s, obj %p.\n", iface, debugstr_guid(iid), obj);

    if (IsEqualGUID(iid, &IID_ID2D1DrawInfo)
            || IsEqualGUID(iid, &IID_ID2D1RenderInfo)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        *obj = iface;
        ID2D1DrawInfo_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported interface %s.\n", debugstr_guid(iid));

    *obj = NULL;

    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_draw_info_AddRef(ID2D1DrawInfo *iface)
{
    struct d2d_render_info *render_info = impl_from_ID2D1DrawInfo(iface);
    ULONG refcount = InterlockedIncrement(&render_info->refcount);

    TRACE("iface %p refcount %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_draw_info_Release(ID2D1DrawInfo *iface)
{
    struct d2d_render_info *render_info = impl_from_ID2D1DrawInfo(iface);
    ULONG refcount = InterlockedDecrement(&render_info->refcount);

    TRACE("iface %p refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        if (render_info->ps) ID3D11PixelShader_Release(render_info->ps);
        if (render_info->vs) ID3D11VertexShader_Release(render_info->vs);
        ID2D1Device6_Release(&render_info->device->ID2D1Device6_iface);
        free(render_info->constants);
        free(render_info);
    }

    return refcount;
}

static HRESULT STDMETHODCALLTYPE d2d_draw_info_SetInputDescription(ID2D1DrawInfo *iface,
        UINT32 index, D2D1_INPUT_DESCRIPTION description)
{
    struct d2d_render_info *info = impl_from_ID2D1DrawInfo(iface);

    TRACE("iface %p, index %u, filter %#x, level count %u.\n", iface, index,
            description.filter, description.levelOfDetailCount);
    if (index >= info->input_count) return E_INVALIDARG;
    if (index >= ARRAY_SIZE(info->input_descriptions)) return E_NOTIMPL;
    if ((description.filter & ~0x15) && description.filter != D2D1_FILTER_ANISOTROPIC)
        return E_INVALIDARG;
    info->input_descriptions[index] = description;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_draw_info_SetOutputBuffer(ID2D1DrawInfo *iface,
        D2D1_BUFFER_PRECISION precision, D2D1_CHANNEL_DEPTH depth)
{
    struct d2d_render_info *info = impl_from_ID2D1DrawInfo(iface);

    TRACE("iface %p, precision %u, depth %u.\n", iface, precision, depth);
    if (precision > D2D1_BUFFER_PRECISION_32BPC_FLOAT
            || (depth != D2D1_CHANNEL_DEPTH_DEFAULT && depth != D2D1_CHANNEL_DEPTH_1
                && depth != D2D1_CHANNEL_DEPTH_4)) return E_INVALIDARG;
    info->precision = precision;
    info->depth = depth;
    return S_OK;
}

static void STDMETHODCALLTYPE d2d_draw_info_SetCached(ID2D1DrawInfo *iface, BOOL is_cached)
{
    struct d2d_render_info *info = impl_from_ID2D1DrawInfo(iface);
    TRACE("iface %p, is_cached %d.\n", iface, is_cached);
    info->cached = !!is_cached;
}

static void STDMETHODCALLTYPE d2d_draw_info_SetInstructionCountHint(ID2D1DrawInfo *iface,
        UINT32 count)
{
    struct d2d_render_info *info = impl_from_ID2D1DrawInfo(iface);
    TRACE("iface %p, count %u.\n", iface, count);
    info->instruction_count = count;
}

static HRESULT STDMETHODCALLTYPE d2d_draw_info_SetPixelShaderConstantBuffer(ID2D1DrawInfo *iface,
        const BYTE *buffer, UINT32 size)
{
    struct d2d_render_info *info = impl_from_ID2D1DrawInfo(iface);
    BYTE *copy = NULL;

    TRACE("iface %p, buffer %p, size %u.\n", iface, buffer, size);
    if (size)
    {
        if (!buffer) return E_INVALIDARG;
        if (!(copy = malloc(size))) return E_OUTOFMEMORY;
        memcpy(copy, buffer, size);
    }
    free(info->constants);
    info->constants = copy;
    info->constants_size = size;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_draw_info_SetResourceTexture(ID2D1DrawInfo *iface,
        UINT32 index, ID2D1ResourceTexture *texture)
{
    FIXME("iface %p, index %u, texture %p stub.\n", iface, index, texture);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_draw_info_SetVertexShaderConstantBuffer(ID2D1DrawInfo *iface,
        const BYTE *buffer, UINT32 size)
{
    FIXME("iface %p, buffer %p, size %u stub.\n", iface, buffer, size);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_draw_info_SetPixelShader(ID2D1DrawInfo *iface,
        REFGUID id, D2D1_PIXEL_OPTIONS options)
{
    struct d2d_render_info *render_info = impl_from_ID2D1DrawInfo(iface);
    IUnknown *object;
    ID3D11PixelShader *shader;
    HRESULT hr;

    TRACE("iface %p, id %s, options %u.\n", iface, debugstr_guid(id), options);

    if (options & ~D2D1_PIXEL_OPTIONS_TRIVIAL_SAMPLING) return E_INVALIDARG;
    if (!d2d_device_get_indexed_object(&render_info->device->shaders, id, &object))
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    hr = IUnknown_QueryInterface(object, &IID_ID3D11PixelShader, (void **)&shader);
    IUnknown_Release(object);
    if (FAILED(hr)) return E_INVALIDARG;
    if (render_info->ps) ID3D11PixelShader_Release(render_info->ps);
    render_info->ps = shader;
    render_info->pixel_options = options;
    render_info->mask |= D2D_RENDER_INFO_PIXEL_SHADER;
    render_info->pixel_shader = *id;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_draw_info_SetVertexProcessing(ID2D1DrawInfo *iface,
        ID2D1VertexBuffer *buffer, D2D1_VERTEX_OPTIONS options,
        const D2D1_BLEND_DESCRIPTION *description, const D2D1_VERTEX_RANGE *range,
        const GUID *shader)
{
    FIXME("iface %p, buffer %p, options %#x, description %p, range %p, shader %s stub.\n",
            iface, buffer, options, description, range, debugstr_guid(shader));

    return E_NOTIMPL;
}

static const ID2D1DrawInfoVtbl d2d_draw_info_vtbl =
{
    d2d_draw_info_QueryInterface,
    d2d_draw_info_AddRef,
    d2d_draw_info_Release,
    d2d_draw_info_SetInputDescription,
    d2d_draw_info_SetOutputBuffer,
    d2d_draw_info_SetCached,
    d2d_draw_info_SetInstructionCountHint,
    d2d_draw_info_SetPixelShaderConstantBuffer,
    d2d_draw_info_SetResourceTexture,
    d2d_draw_info_SetVertexShaderConstantBuffer,
    d2d_draw_info_SetPixelShader,
    d2d_draw_info_SetVertexProcessing,
};

static HRESULT d2d_effect_render_info_create(struct d2d_device *device, struct d2d_render_info **obj)
{
    struct d2d_render_info *object;
    unsigned int i;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->ID2D1DrawInfo_iface.lpVtbl = &d2d_draw_info_vtbl;
    object->refcount = 1;
    object->device = device;
    for (i = 0; i < ARRAY_SIZE(object->input_descriptions); ++i)
        object->input_descriptions[i].filter = D2D1_FILTER_MIN_MAG_MIP_LINEAR;
    ID2D1Device6_AddRef(&device->ID2D1Device6_iface);

    *obj = object;

    return S_OK;
}

static bool d2d_transform_node_needs_render_info(const struct d2d_transform_node *node)
{
    static const GUID *iids[] =
    {
        &IID_ID2D1SourceTransform,
        &IID_ID2D1ComputeTransform,
        &IID_ID2D1DrawTransform,
    };
    unsigned int i;
    IUnknown *obj;

    for (i = 0; i < ARRAY_SIZE(iids); ++i)
    {
        if (SUCCEEDED(ID2D1TransformNode_QueryInterface(node->object, iids[i], (void **)&obj)))
        {
            IUnknown_Release(obj);
            return true;
        }
    }

    return false;
}

static HRESULT d2d_effect_transform_graph_initialize_nodes(struct d2d_transform_graph *graph,
        struct d2d_device *device)
{
    ID2D1DrawTransform *draw_transform;
    ID2D1OffsetTransform *offset_transform;
    struct d2d_transform_node *node;
    HRESULT hr;

    LIST_FOR_EACH_ENTRY(node, &graph->nodes, struct d2d_transform_node, entry)
    {
        if (node->object->lpVtbl == &d2d_effect_node_vtbl) continue;
        if (node->render_info) continue;
        /* Offset nodes change coordinates and reuse their input image. */
        if (SUCCEEDED(ID2D1TransformNode_QueryInterface(node->object, &IID_ID2D1OffsetTransform,
                (void **)&offset_transform)))
        {
            ID2D1OffsetTransform_Release(offset_transform);
            continue;
        }
        if (d2d_transform_node_needs_render_info(node))
        {
            if (FAILED(hr = d2d_effect_render_info_create(device, &node->render_info)))
                return hr;
            node->render_info->input_count = node->input_count;
        }

        if (SUCCEEDED(ID2D1TransformNode_QueryInterface(node->object, &IID_ID2D1DrawTransform,
                (void **)&draw_transform)))
        {
            hr = ID2D1DrawTransform_SetDrawInfo(draw_transform, &node->render_info->ID2D1DrawInfo_iface);
            ID2D1DrawTransform_Release(draw_transform);
            if (FAILED(hr))
            {
                WARN("Failed to set draw info, hr %#lx.\n", hr);
                return hr;
            }
        }
        else
        {
            FIXME("Unsupported node %p.\n", node);
            return E_NOTIMPL;
        }
    }

    return S_OK;
}

HRESULT d2d_effect_create(struct d2d_device_context *context, const CLSID *effect_id,
        ID2D1Effect **effect)
{
    struct d2d_effect_context *effect_context;
    const struct d2d_effect_registration *reg;
    struct d2d_effect *object;
    UINT32 input_count;
    WCHAR clsidW[39];
    HRESULT hr;

    if (!(reg = d2d_factory_get_registered_effect(context->factory, effect_id)))
    {
        WARN("Effect id %s not found.\n", wine_dbgstr_guid(effect_id));
        return D2DERR_EFFECT_IS_NOT_REGISTERED;
    }

    if (!(effect_context = calloc(1, sizeof(*effect_context))))
        return E_OUTOFMEMORY;
    d2d_effect_context_init(effect_context, context);

    if (!(object = calloc(1, sizeof(*object))))
    {
        ID2D1EffectContext1_Release(&effect_context->ID2D1EffectContext1_iface);
        return E_OUTOFMEMORY;
    }

    object->ID2D1Effect_iface.lpVtbl = &d2d_effect_vtbl;
    object->ID2D1Image_iface.lpVtbl = &d2d_effect_image_vtbl;
    object->refcount = 1;
    object->effect_context = effect_context;

    /* Create properties */
    d2d_effect_duplicate_properties(object, &object->properties, reg->properties);

    StringFromGUID2(effect_id, clsidW, ARRAY_SIZE(clsidW));
    d2d_effect_properties_add(&object->properties, L"CLSID", D2D1_PROPERTY_CLSID, D2D1_PROPERTY_TYPE_CLSID, clsidW);
    d2d_effect_properties_add(&object->properties, L"Cached", D2D1_PROPERTY_CACHED, D2D1_PROPERTY_TYPE_BOOL, L"false");
    d2d_effect_properties_add(&object->properties, L"Precision", D2D1_PROPERTY_PRECISION, D2D1_PROPERTY_TYPE_ENUM, L"0");

    /* Sync instance input count with default input count from the description. */
    d2d_effect_get_value(object, D2D1_PROPERTY_INPUTS, D2D1_PROPERTY_TYPE_ARRAY, (BYTE *)&input_count, sizeof(input_count));
    d2d_effect_set_input_count(object, input_count);

    if (FAILED(hr = d2d_transform_graph_create(input_count, &object->graph)))
    {
        ID2D1EffectContext1_Release(&effect_context->ID2D1EffectContext1_iface);
        return hr;
    }

    if (FAILED(hr = reg->factory(&object->impl_unknown)))
    {
        WARN("Failed to create implementation object, hr %#lx.\n", hr);
        ID2D1Effect_Release(&object->ID2D1Effect_iface);
        return hr;
    }

    /* The factory returns IUnknown, which need not share a vtable with the
     * implementation interface. Keep the original pointer for property bindings. */
    if (FAILED(hr = IUnknown_QueryInterface(object->impl_unknown, &IID_ID2D1EffectImpl,
            (void **)&object->impl)))
    {
        WARN("Effect implementation does not support ID2D1EffectImpl, hr %#lx.\n", hr);
        ID2D1Effect_Release(&object->ID2D1Effect_iface);
        return hr;
    }

    if (FAILED(hr = ID2D1EffectImpl_Initialize(object->impl, (ID2D1EffectContext *)&effect_context->ID2D1EffectContext1_iface,
            &object->graph->ID2D1TransformGraph_iface)))
    {
        WARN("Failed to initialize effect, hr %#lx.\n", hr);
        ID2D1Effect_Release(&object->ID2D1Effect_iface);
        return hr;
    }

    if (FAILED(hr = d2d_effect_transform_graph_initialize_nodes(object->graph, context->device)))
    {
        WARN("Failed to initialize graph nodes, hr %#lx.\n", hr);
        ID2D1Effect_Release(&object->ID2D1Effect_iface);
        return hr;
    }

    object->changes = D2D1_CHANGE_TYPE_GRAPH;
    *effect = &object->ID2D1Effect_iface;

    TRACE("Created effect %p.\n", *effect);

    return S_OK;
}

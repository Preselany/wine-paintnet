/* WinRT CoreMessaging Implementation
 *
 * Copyright (C) 2024 Mohamad Al-Jaf
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

#include "initguid.h"
#include "private.h"
#include "dispatcherqueue.h"

WINE_DEFAULT_DEBUG_CHANNEL(messaging);

struct dispatcher_queue_controller_statics
{
    IActivationFactory IActivationFactory_iface;
    IDispatcherQueueControllerStatics IDispatcherQueueControllerStatics_iface;
    IDispatcherQueueStatics IDispatcherQueueStatics_iface;
    LONG ref;
};

static inline struct dispatcher_queue_controller_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct dispatcher_queue_controller_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct dispatcher_queue_controller_statics *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IDispatcherQueueStatics ) && impl->IDispatcherQueueStatics_iface.lpVtbl)
    {
        *out = &impl->IDispatcherQueueStatics_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IDispatcherQueueControllerStatics ) && impl->IDispatcherQueueControllerStatics_iface.lpVtbl)
    {
        *out = &impl->IDispatcherQueueControllerStatics_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct dispatcher_queue_controller_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing ref to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct dispatcher_queue_controller_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing ref to %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    /* IInspectable methods */
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    /* IActivationFactory methods */
    factory_ActivateInstance,
};

DEFINE_IINSPECTABLE( dispatcher_queue_controller_statics, IDispatcherQueueControllerStatics, struct dispatcher_queue_controller_statics, IActivationFactory_iface )

static HRESULT WINAPI dispatcher_queue_controller_statics_CreateOnDedicatedThread( IDispatcherQueueControllerStatics *iface, IDispatcherQueueController **result )
{
    DispatcherQueueOptions options = {sizeof(options), DQTYPE_THREAD_DEDICATED, DQTAT_COM_STA};

    TRACE( "iface %p, result %p.\n", iface, result );
    return CreateDispatcherQueueController( options, result );
}

static const struct IDispatcherQueueControllerStaticsVtbl dispatcher_queue_controller_statics_vtbl =
{
    dispatcher_queue_controller_statics_QueryInterface,
    dispatcher_queue_controller_statics_AddRef,
    dispatcher_queue_controller_statics_Release,
    /* IInspectable methods */
    dispatcher_queue_controller_statics_GetIids,
    dispatcher_queue_controller_statics_GetRuntimeClassName,
    dispatcher_queue_controller_statics_GetTrustLevel,
    /* IDispatcherQueueControllerStatics methods */
    dispatcher_queue_controller_statics_CreateOnDedicatedThread,
};

DEFINE_IINSPECTABLE( dispatcher_queue_statics, IDispatcherQueueStatics, struct dispatcher_queue_controller_statics, IActivationFactory_iface )

static HRESULT WINAPI dispatcher_queue_statics_GetForCurrentThread( IDispatcherQueueStatics *iface, IDispatcherQueue **result )
{
    TRACE( "iface %p, result %p.\n", iface, result );
    return dispatcher_queue_get_for_current_thread( result );
}

static const struct IDispatcherQueueStaticsVtbl dispatcher_queue_statics_vtbl =
{
    dispatcher_queue_statics_QueryInterface,
    dispatcher_queue_statics_AddRef,
    dispatcher_queue_statics_Release,
    dispatcher_queue_statics_GetIids,
    dispatcher_queue_statics_GetRuntimeClassName,
    dispatcher_queue_statics_GetTrustLevel,
    dispatcher_queue_statics_GetForCurrentThread,
};

static struct dispatcher_queue_controller_statics dispatcher_queue_controller_statics =
{
    {&factory_vtbl},
    {&dispatcher_queue_controller_statics_vtbl},
    {NULL},
    1,
};

static struct dispatcher_queue_controller_statics dispatcher_queue_statics =
{
    {&factory_vtbl},
    {NULL},
    {&dispatcher_queue_statics_vtbl},
    1,
};

static IActivationFactory *dispatcher_queue_controller_factory = &dispatcher_queue_controller_statics.IActivationFactory_iface;

HRESULT WINAPI DllGetActivationFactory( HSTRING classid, IActivationFactory **factory )
{
    const WCHAR *name = WindowsGetStringRawBuffer( classid, NULL );

    TRACE( "classid %s, factory %p.\n", debugstr_hstring( classid ), factory );

    *factory = NULL;

    if (!wcscmp( name, RuntimeClass_Windows_System_DispatcherQueueController ))
        IActivationFactory_QueryInterface( dispatcher_queue_controller_factory, &IID_IActivationFactory, (void **)factory );

    if (!wcscmp( name, RuntimeClass_Windows_System_DispatcherQueue ))
        IActivationFactory_QueryInterface( &dispatcher_queue_statics.IActivationFactory_iface,
                &IID_IActivationFactory, (void **)factory );

    if (*factory) return S_OK;
    return CLASS_E_CLASSNOTAVAILABLE;
}

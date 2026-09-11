/* UIAnimation timeline implementation.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __WINE_ANIMATION_PRIVATE_H
#define __WINE_ANIMATION_PRIVATE_H

enum animation_transition_kind
{
    TRANSITION_INSTANT, TRANSITION_LINEAR, TRANSITION_SMOOTH_STOP,
};

HRESULT animation_manager_create(IUnknown *outer, REFIID iid, void **out);
HRESULT animation_transition_create(enum animation_transition_kind kind, double duration,
        double final_value, IUIAnimationTransition **out);
#endif

///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/objc/NSSlider.h
// Purpose:     WXNSSlider class
// Author:      David Elliott
// Modified by:
// Created:     2007/08/10 (move from NSSlider.mm)
// Copyright:   (c) 2007 Software 2000 Ltd.
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_OBJC_NSSLIDER_H__
#define __WX_COCOA_OBJC_NSSLIDER_H__

#include "wx/cocoa/objc/objc_uniquifying.h"

#import <AppKit/NSSlider.h>

// ============================================================================
// @class WXNSSlider
// ============================================================================

@interface WXNSSlider : NSSlider
@end

WX_DECLARE_GET_OBJC_CLASS(WXNSSlider,NSSlider)

// ============================================================================
// @class WXNSSliderCell
// ============================================================================

@interface WXNSSliderCell : NSSliderCell
@end

WX_DECLARE_GET_OBJC_CLASS(WXNSSliderCell,NSSliderCell)

#endif //ndef __WX_COCOA_OBJC_NSSLIDER_H__

/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/graphicsd2d.cpp
// Purpose:     Implementation of Direct2D Render Context
// Author:      Pana Alexandru <astronothing@gmail.com>
// Created:     2014-05-20
// Copyright:   (c) 2014 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_GRAPHICS_DIRECT2D

// Minimum supported client: Windows 8 and Platform Update for Windows 7
#define wxD2D_DEVICE_CONTEXT_SUPPORTED 0

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <list>

// Ensure no previous defines interfere with the Direct2D API headers
#undef GetHwnd

// We load these functions at runtime from the d2d1.dll.
// However, since they are also used inside the d2d1.h header we must provide
// implementations matching the exact declarations. These defines ensures we
// are not violating the ODR rule.
#define D2D1CreateFactory wxD2D1CreateFactory
#define D2D1MakeRotateMatrix wxD2D1MakeRotateMatrix
#define D2D1InvertMatrix wxD2D1InvertMatrix

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>

#if wxD2D_DEVICE_CONTEXT_SUPPORTED
#include <D3D11.h>
#include <D2d1_1.h>
#include <DXGI1_2.h>
#endif

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "wx/graphics.h"
#include "wx/dc.h"
#include "wx/dynlib.h"
#include "wx/image.h"
#include "wx/module.h"
#include "wx/msw/private/comptr.h"
#include "wx/private/graphics.h"
#include "wx/stack.h"
#include "wx/sharedptr.h"

// This must be the last header included to only affect the DEFINE_GUID()
// occurrences below but not any GUIDs declared in the standard files included
// above.
#include <initguid.h>

// Generic error message for a failed direct2d operation
#define wxFAILED_HRESULT_MSG(result) \
    wxString::Format("Direct2D failed with HRESULT %x", (result))

// Checks a HRESULT value for success, otherwise displays an error message and
// returns from the enclosing function.
#define wxCHECK_HRESULT_RET(result) \
    wxCHECK_RET(SUCCEEDED(result), wxFAILED_HRESULT_MSG(result))

#define wxCHECK2_HRESULT_RET(result, returnValue)                             \
    wxCHECK2_MSG(SUCCEEDED(result), return returnValue,                       \
        wxFAILED_HRESULT_MSG(result))

// Variation of wxCHECK_HRESULT_RET for functions which must return a pointer
#define wxCHECK_HRESULT_RET_PTR(result) wxCHECK2_HRESULT_RET(result, NULL)

// Checks the precondition of wxManagedResourceHolder::AcquireResource, namely
// that it is bound to a manager.
#define wxCHECK_RESOURCE_HOLDER_PRE()                                         \
    {                                                                         \
    if (IsResourceAcquired()) return;                                         \
    wxCHECK_RET(IsBound(),                                                    \
        "Cannot acquire a native resource without being bound to a manager"); \
    }

// Checks the postcondition of wxManagedResourceHolder::AcquireResource, namely
// that it was successful in acquiring the native resource.
#define wxCHECK_RESOURCE_HOLDER_POST() \
    wxCHECK_RET(m_nativeResource != NULL, "Could not acquire native resource");


// Helper class used to check for direct2d availability at runtime and to
// dynamically load the required symbols from d2d1.dll and dwrite.dll
class wxDirect2D
{
public:

    enum wxD2DVersion
    {
        wxD2D_VERSION_1_0,
        wxD2D_VERSION_1_1,
        wxD2D_VERSION_NONE
    };

    static bool Initialize()
    {
        if (!m_initialized)
        {
            m_hasDirect2DSupport = LoadLibraries();
            m_initialized = true;
        }

        return m_hasDirect2DSupport;
    }

    static bool HasDirect2DSupport()
    {
        Initialize();

        return m_hasDirect2DSupport;
    }

    static wxD2DVersion GetDirect2DVersion()
    {
        return m_D2DRuntimeVersion;
    }

private:
    static bool LoadLibraries()
    {
        if ( !m_dllDirect2d.Load(wxT("d2d1.dll"), wxDL_VERBATIM | wxDL_QUIET) )
            return false;

        if ( !m_dllDirectWrite.Load(wxT("dwrite.dll"), wxDL_VERBATIM | wxDL_QUIET) )
            return false;

        #define wxLOAD_FUNC(dll, name)                    \
        name = (name##_t)dll.RawGetSymbol(#name);         \
            if ( !name )                                  \
            return false;

        wxLOAD_FUNC(m_dllDirect2d, D2D1CreateFactory);
        wxLOAD_FUNC(m_dllDirect2d, D2D1MakeRotateMatrix);
        wxLOAD_FUNC(m_dllDirect2d, D2D1InvertMatrix);
        wxLOAD_FUNC(m_dllDirectWrite, DWriteCreateFactory);

        m_D2DRuntimeVersion = wxD2D_VERSION_1_0;

        return true;
    }

public:
    typedef HRESULT (WINAPI *D2D1CreateFactory_t)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS*, void**);
    static D2D1CreateFactory_t D2D1CreateFactory;

    typedef void (WINAPI *D2D1MakeRotateMatrix_t)(FLOAT, D2D1_POINT_2F, D2D1_MATRIX_3X2_F*);
    static D2D1MakeRotateMatrix_t D2D1MakeRotateMatrix;

    typedef BOOL (WINAPI *D2D1InvertMatrix_t)(D2D1_MATRIX_3X2_F*);
    static D2D1InvertMatrix_t D2D1InvertMatrix;

    typedef HRESULT (WINAPI *DWriteCreateFactory_t)(DWRITE_FACTORY_TYPE, REFIID, IUnknown**);
    static DWriteCreateFactory_t DWriteCreateFactory;

private:
    static bool m_initialized;
    static bool m_hasDirect2DSupport;
    static wxD2DVersion m_D2DRuntimeVersion;

    static wxDynamicLibrary m_dllDirect2d;
    static wxDynamicLibrary m_dllDirectWrite;
};

// define the members
bool wxDirect2D::m_initialized = false;
bool wxDirect2D::m_hasDirect2DSupport = false;
wxDirect2D::wxD2DVersion wxDirect2D::m_D2DRuntimeVersion = wxD2D_VERSION_NONE;

wxDynamicLibrary wxDirect2D::m_dllDirect2d;
wxDynamicLibrary wxDirect2D::m_dllDirectWrite;

// define the (not yet imported) functions
wxDirect2D::D2D1CreateFactory_t wxDirect2D::D2D1CreateFactory = NULL;
wxDirect2D::D2D1MakeRotateMatrix_t wxDirect2D::D2D1MakeRotateMatrix = NULL;
wxDirect2D::D2D1InvertMatrix_t wxDirect2D::D2D1InvertMatrix = NULL;
wxDirect2D::DWriteCreateFactory_t wxDirect2D::DWriteCreateFactory = NULL;

// define the interface GUIDs
DEFINE_GUID(wxIID_IWICImagingFactory,
            0xec5ec8a9, 0xc395, 0x4314, 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70);

DEFINE_GUID(wxIID_IDWriteFactory,
            0xb859ee5a, 0xd838, 0x4b5b, 0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48);

DEFINE_GUID(wxIID_IWICBitmapSource,
            0x00000120, 0xa8f2, 0x4877, 0xba, 0x0a, 0xfd, 0x2b, 0x66, 0x45, 0xfb, 0x94);

// Implementation of the Direct2D functions
HRESULT WINAPI wxD2D1CreateFactory(
    D2D1_FACTORY_TYPE factoryType,
    REFIID riid,
    CONST D2D1_FACTORY_OPTIONS *pFactoryOptions,
    void **ppIFactory)
{
    if (!wxDirect2D::Initialize())
        return S_FALSE;

    return wxDirect2D::D2D1CreateFactory(
        factoryType,
        riid,
        pFactoryOptions,
        ppIFactory);
}

void WINAPI wxD2D1MakeRotateMatrix(
    FLOAT angle,
    D2D1_POINT_2F center,
    D2D1_MATRIX_3X2_F *matrix)
{
    if (!wxDirect2D::Initialize())
        return;

    wxDirect2D::D2D1MakeRotateMatrix(angle, center, matrix);
}

BOOL WINAPI wxD2D1InvertMatrix(
    D2D1_MATRIX_3X2_F *matrix)
{
    if (!wxDirect2D::Initialize())
        return FALSE;

    return wxDirect2D::D2D1InvertMatrix(matrix);
}

static IWICImagingFactory* gs_WICImagingFactory = NULL;

IWICImagingFactory* wxWICImagingFactory()
{
    if (gs_WICImagingFactory == NULL) {
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            NULL,
            CLSCTX_INPROC_SERVER,
            wxIID_IWICImagingFactory,
            (LPVOID*)&gs_WICImagingFactory);
        wxCHECK_HRESULT_RET_PTR(hr);
    }
    return gs_WICImagingFactory;
}

static IDWriteFactory* gs_IDWriteFactory = NULL;

IDWriteFactory* wxDWriteFactory()
{
    if (!wxDirect2D::Initialize())
        return NULL;

    if (gs_IDWriteFactory == NULL)
    {
        wxDirect2D::DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            wxIID_IDWriteFactory,
            reinterpret_cast<IUnknown**>(&gs_IDWriteFactory)
            );
    }
    return gs_IDWriteFactory;
}

extern WXDLLIMPEXP_DATA_CORE(wxGraphicsPen) wxNullGraphicsPen;
extern WXDLLIMPEXP_DATA_CORE(wxGraphicsBrush) wxNullGraphicsBrush;

// We use the notion of a context supplier because the context
// needed to create Direct2D resources (namely the RenderTarget)
// is itself device-dependent and might change during the lifetime
// of the resources which were created from it.
template <typename C>
class wxContextSupplier
{
public:
    typedef C ContextType;

    virtual C GetContext() = 0;
};

typedef wxContextSupplier<ID2D1RenderTarget*> wxD2DContextSupplier;

// A resource holder manages a generic resource by acquiring
// and releasing it on demand.
class wxResourceHolder
{
public:
    // Acquires the managed resource if necessary (not already acquired)
    virtual void AcquireResource() = 0;

    // Releases the managed resource
    virtual void ReleaseResource() = 0;

    // Checks if the resources was previously acquired
    virtual bool IsResourceAcquired() = 0;

    // Returns the managed resource or NULL if the resources
    // was not previously acquired
    virtual void* GetResource() = 0;

    virtual ~wxResourceHolder(){};
};

class wxD2DResourceManager;

class wxD2DManagedObject
{
public:
    virtual void Bind(wxD2DResourceManager* manager) = NULL;
    virtual void UnBind() = NULL;
    virtual bool IsBound() = NULL;
    virtual wxD2DResourceManager* GetManager() = NULL;

    virtual ~wxD2DManagedObject() {};
};

class wxManagedResourceHolder : public wxResourceHolder, public wxD2DManagedObject
{
public:
    virtual ~wxManagedResourceHolder() {};
};

// A Direct2D resource manager handles the device-dependent
// resource holders attached to it by requesting them to
// release their resources when the API invalidates.
class wxD2DResourceManager: public wxD2DContextSupplier
{
public:
    typedef wxManagedResourceHolder* ElementType;

    // NOTE: We're using a list because we expect to have multiple
    // insertions but very rarely a traversal (if ever).
    typedef std::list<ElementType> ListType;

    void RegisterResourceHolder(ElementType resourceHolder)
    {
        m_resources.push_back(resourceHolder);
    }

    void UnregisterResourceHolder(ElementType resourceHolder)
    {
        m_resources.remove(resourceHolder);
    }

    void ReleaseResources()
    {
        ListType::iterator it;
        for (it = m_resources.begin(); it != m_resources.end(); ++it)
        {
            (*it)->ReleaseResource();
        }

        // Check that all resources were released
        for (it = m_resources.begin(); it != m_resources.end(); ++it)
        {
            wxCHECK_RET(!(*it)->IsResourceAcquired(), "One or more device-dependent resources failed to release");
        }
    }

    virtual ~wxD2DResourceManager()
    {
        while (!m_resources.empty())
        {
            m_resources.front()->ReleaseResource();
            m_resources.front()->UnBind();
        }
    }

private:
    ListType m_resources;
};

// A Direct2D resource holder manages device dependent resources
// by storing any information necessary for acquiring the resource
// and releasing the resource when the API invalidates it.
template<typename T>
class wxD2DResourceHolder: public wxManagedResourceHolder
{
public:
    wxD2DResourceHolder() : m_resourceManager(NULL)
    {
    }

    virtual ~wxD2DResourceHolder()
    {
        UnBind();
        ReleaseResource();
    }

    bool IsResourceAcquired() wxOVERRIDE
    {
        return m_nativeResource != NULL;
    }

    void* GetResource() wxOVERRIDE
    {
        return GetD2DResource();
    }

    wxCOMPtr<T>& GetD2DResource()
    {
        if (!IsResourceAcquired())
        {
            AcquireResource();
        }

        return m_nativeResource;
    }

    void AcquireResource() wxOVERRIDE
    {
        wxCHECK_RESOURCE_HOLDER_PRE();

        DoAcquireResource();

        wxCHECK_RESOURCE_HOLDER_POST();
    }

    void ReleaseResource() wxOVERRIDE
    {
        m_nativeResource.reset();
    }

    wxD2DContextSupplier::ContextType GetContext()
    {
        return m_resourceManager->GetContext();
    }

    void Bind(wxD2DResourceManager* manager) wxOVERRIDE
    {
        if (IsBound())
            return;

        m_resourceManager = manager;
        m_resourceManager->RegisterResourceHolder(this);
    }

    void UnBind() wxOVERRIDE
    {
        if (!IsBound())
            return;

        m_resourceManager->UnregisterResourceHolder(this);
        m_resourceManager = NULL;
    }

    bool IsBound() wxOVERRIDE
    {
        return m_resourceManager != NULL;
    }

    wxD2DResourceManager* GetManager() wxOVERRIDE
    {
        return m_resourceManager;
    }

protected:
    virtual void DoAcquireResource() = 0;

private:
    wxD2DResourceManager* m_resourceManager;

protected:
    wxCOMPtr<T> m_nativeResource;
};

// Used as super class for graphics data objects
// to forward the bindings to their internal resource holder.
class wxD2DManagedGraphicsData : public wxD2DManagedObject
{
public:
    void Bind(wxD2DResourceManager* manager) wxOVERRIDE
    {
        GetManagedObject()->Bind(manager);
    }

    void UnBind() wxOVERRIDE
    {
        GetManagedObject()->UnBind();
    }

    bool IsBound() wxOVERRIDE
    {
        return GetManagedObject()->IsBound();
    }

    wxD2DResourceManager* GetManager() wxOVERRIDE
    {
        return GetManagedObject()->GetManager();
    }

    virtual wxD2DManagedObject* GetManagedObject() = 0;

    ~wxD2DManagedGraphicsData() {};
};

D2D1_CAP_STYLE wxD2DConvertPenCap(wxPenCap cap)
{
    switch (cap)
    {
    case wxCAP_ROUND:
        return D2D1_CAP_STYLE_ROUND;
    case wxCAP_PROJECTING:
        return D2D1_CAP_STYLE_SQUARE;
    case wxCAP_BUTT:
        return D2D1_CAP_STYLE_FLAT;
    case wxCAP_INVALID:
        return D2D1_CAP_STYLE_FLAT;
    }

    wxFAIL_MSG("unknown pen cap");
    return D2D1_CAP_STYLE_FLAT;
}

D2D1_LINE_JOIN wxD2DConvertPenJoin(wxPenJoin join)
{
    switch (join)
    {
    case wxJOIN_BEVEL:
        return D2D1_LINE_JOIN_BEVEL;
    case wxJOIN_MITER:
        return D2D1_LINE_JOIN_MITER;
    case wxJOIN_ROUND:
        return D2D1_LINE_JOIN_ROUND;
    case wxJOIN_INVALID:
        return D2D1_LINE_JOIN_MITER;
    }

    wxFAIL_MSG("unknown pen join");
    return D2D1_LINE_JOIN_MITER;
}

D2D1_DASH_STYLE wxD2DConvertPenStyle(wxPenStyle dashStyle)
{
    switch (dashStyle)
    {
    case wxPENSTYLE_SOLID:
        return D2D1_DASH_STYLE_SOLID;
    case wxPENSTYLE_DOT:
        return D2D1_DASH_STYLE_DOT;
    case wxPENSTYLE_LONG_DASH:
        return D2D1_DASH_STYLE_DASH;
    case wxPENSTYLE_SHORT_DASH:
        return D2D1_DASH_STYLE_DASH;
    case wxPENSTYLE_DOT_DASH:
        return D2D1_DASH_STYLE_DASH_DOT;
    case wxPENSTYLE_USER_DASH:
        return D2D1_DASH_STYLE_CUSTOM;

    // NB: These styles cannot be converted to a D2D1_DASH_STYLE
    // and must be handled separately.
    case wxPENSTYLE_TRANSPARENT:
        wxFALLTHROUGH;
    case wxPENSTYLE_INVALID:
        wxFALLTHROUGH;
    case wxPENSTYLE_STIPPLE_MASK_OPAQUE:
        wxFALLTHROUGH;
    case wxPENSTYLE_STIPPLE_MASK:
        wxFALLTHROUGH;
    case wxPENSTYLE_STIPPLE:
        wxFALLTHROUGH;
    case wxPENSTYLE_BDIAGONAL_HATCH:
        wxFALLTHROUGH;
    case wxPENSTYLE_CROSSDIAG_HATCH:
        wxFALLTHROUGH;
    case wxPENSTYLE_FDIAGONAL_HATCH:
        wxFALLTHROUGH;
    case wxPENSTYLE_CROSS_HATCH:
        wxFALLTHROUGH;
    case wxPENSTYLE_HORIZONTAL_HATCH:
        wxFALLTHROUGH;
    case wxPENSTYLE_VERTICAL_HATCH:
        return D2D1_DASH_STYLE_SOLID;
    }

    wxFAIL_MSG("unknown pen style");
    return D2D1_DASH_STYLE_SOLID;
}

D2D1_COLOR_F wxD2DConvertColour(wxColour colour)
{
    return D2D1::ColorF(
        colour.Red() / 255.0f,
        colour.Green() / 255.0f,
        colour.Blue() / 255.0f,
        colour.Alpha() / 255.0f);
}

D2D1_ANTIALIAS_MODE wxD2DConvertAntialiasMode(wxAntialiasMode antialiasMode)
{
    switch (antialiasMode)
    {
    case wxANTIALIAS_NONE:
        return D2D1_ANTIALIAS_MODE_ALIASED;
    case wxANTIALIAS_DEFAULT:
        return D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
    }

    wxFAIL_MSG("unknown antialias mode");
    return D2D1_ANTIALIAS_MODE_ALIASED;
}

#if wxD2D_DEVICE_CONTEXT_SUPPORTED
bool wxD2DCompositionModeSupported(wxCompositionMode compositionMode)
{
    if (compositionMode == wxCOMPOSITION_CLEAR || compositionMode == wxCOMPOSITION_INVALID)
    {
        return false;
    }

    return true;
}

D2D1_COMPOSITE_MODE wxD2DConvertCompositionMode(wxCompositionMode compositionMode)
{
    switch (compositionMode)
    {
    case wxCOMPOSITION_SOURCE:
        return D2D1_COMPOSITE_MODE_SOURCE_COPY;
    case wxCOMPOSITION_OVER:
        return D2D1_COMPOSITE_MODE_SOURCE_OVER;
    case wxCOMPOSITION_IN:
        return D2D1_COMPOSITE_MODE_SOURCE_IN;
    case wxCOMPOSITION_OUT:
        return D2D1_COMPOSITE_MODE_SOURCE_OUT;
    case wxCOMPOSITION_ATOP:
        return D2D1_COMPOSITE_MODE_SOURCE_ATOP;
    case wxCOMPOSITION_DEST_OVER:
        return D2D1_COMPOSITE_MODE_DESTINATION_OVER;
    case wxCOMPOSITION_DEST_IN:
        return D2D1_COMPOSITE_MODE_DESTINATION_IN;
    case wxCOMPOSITION_DEST_OUT:
        return D2D1_COMPOSITE_MODE_DESTINATION_OUT;
    case wxCOMPOSITION_DEST_ATOP:
        return D2D1_COMPOSITE_MODE_DESTINATION_ATOP;
    case wxCOMPOSITION_XOR:
        return D2D1_COMPOSITE_MODE_XOR;
    case wxCOMPOSITION_ADD:
        return D2D1_COMPOSITE_MODE_PLUS;

    // unsupported composition modes
    case wxCOMPOSITION_DEST:
        wxFALLTHROUGH;
    case wxCOMPOSITION_CLEAR:
        wxFALLTHROUGH;
    case wxCOMPOSITION_INVALID:
        return D2D1_COMPOSITE_MODE_SOURCE_COPY;
    }

    wxFAIL_MSG("unknown composition mode");
    return D2D1_COMPOSITE_MODE_SOURCE_COPY;
}
#endif // wxD2D_DEVICE_CONTEXT_SUPPORTED

// Direct2D 1.1 introduces a new enum for specifying the interpolation quality
// which is only used with the ID2D1DeviceContext::DrawImage method.
#if wxD2D_DEVICE_CONTEXT_SUPPORTED
D2D1_INTERPOLATION_MODE wxD2DConvertInterpolationMode(wxInterpolationQuality interpolationQuality)
{
    switch (interpolationQuality)
    {
    case wxINTERPOLATION_DEFAULT:
        wxFALLTHROUGH;
    case wxINTERPOLATION_NONE:
        wxFALLTHROUGH;
    case wxINTERPOLATION_FAST:
        return D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    case wxINTERPOLATION_GOOD:
        return D2D1_INTERPOLATION_MODE_LINEAR;
    case wxINTERPOLATION_BEST:
        return D2D1_INTERPOLATION_MODE_CUBIC;
    }

    wxFAIL_MSG("unknown interpolation quality");
    return D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
}
#endif // wxD2D_DEVICE_CONTEXT_SUPPORTED

D2D1_BITMAP_INTERPOLATION_MODE wxD2DConvertBitmapInterpolationMode(wxInterpolationQuality interpolationQuality)
{
    switch (interpolationQuality)
    {
    case wxINTERPOLATION_DEFAULT:
        wxFALLTHROUGH;
    case wxINTERPOLATION_NONE:
        wxFALLTHROUGH;
    case wxINTERPOLATION_FAST:
        wxFALLTHROUGH;
    case wxINTERPOLATION_GOOD:
        return D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    case wxINTERPOLATION_BEST:
        return D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
    }

    wxFAIL_MSG("unknown interpolation quality");
    return D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
}

D2D1_RECT_F wxD2DConvertRect(const wxRect& rect)
{
    return D2D1::RectF(rect.GetLeft(), rect.GetTop(), rect.GetRight(), rect.GetBottom());
}

wxCOMPtr<ID2D1Geometry> wxD2DConvertRegionToGeometry(ID2D1Factory* direct2dFactory, const wxRegion& region)
{
    wxRegionIterator regionIterator(region);

    // Count the number of rectangles which compose the region
    int rectCount = 0;
    while(regionIterator++)
        rectCount++;

    // Build the array of geometries
    ID2D1Geometry** geometries = new ID2D1Geometry*[rectCount];
    regionIterator.Reset(region);

    int i = 0;
    while(regionIterator)
    {
        geometries[i] = NULL;

        wxRect rect = regionIterator.GetRect();
        rect.SetWidth(rect.GetWidth() + 1);
        rect.SetHeight(rect.GetHeight() + 1);

        direct2dFactory->CreateRectangleGeometry(
            wxD2DConvertRect(rect),
            (ID2D1RectangleGeometry**)(&geometries[i]));

        i++; regionIterator++;
    }

    // Create a geometry group to hold all the rectangles
    wxCOMPtr<ID2D1GeometryGroup> resultGeometry;
    direct2dFactory->CreateGeometryGroup(
        D2D1_FILL_MODE_WINDING,
        geometries,
        rectCount,
        &resultGeometry);

    // Cleanup temporaries
    for (int i = 0; i < rectCount; ++i)
    {
        geometries[i]->Release();
    }

    delete[] geometries;

    return wxCOMPtr<ID2D1Geometry>(resultGeometry);
}

class wxD2DOffsetHelper
{
public:
    wxD2DOffsetHelper(wxGraphicsContext* g) : m_context(g)
    {
        if (m_context->ShouldOffset())
        {
            m_context->Translate(0.5, 0.5);
        }
    }

    ~wxD2DOffsetHelper()
    {
        if (m_context->ShouldOffset())
        {
            m_context->Translate(-0.5, -0.5);
        }
    }

private:
    wxGraphicsContext* m_context;
};

bool operator==(const D2D1::Matrix3x2F& lhs, const D2D1::Matrix3x2F& rhs)
{
    return
        lhs._11 == rhs._11 && lhs._12 == rhs._12 &&
        lhs._21 == rhs._21 && lhs._22 == rhs._22 &&
        lhs._31 == rhs._31 && lhs._32 == rhs._32;
}

//-----------------------------------------------------------------------------
// wxD2DMatrixData declaration
//-----------------------------------------------------------------------------

class wxD2DMatrixData : public wxGraphicsMatrixData
{
public:
    wxD2DMatrixData(wxGraphicsRenderer* renderer);
    wxD2DMatrixData(wxGraphicsRenderer* renderer, const D2D1::Matrix3x2F& matrix);

    void Concat(const wxGraphicsMatrixData* t) wxOVERRIDE;

    void Set(wxDouble a = 1.0, wxDouble b = 0.0, wxDouble c = 0.0, wxDouble d = 1.0,
        wxDouble tx = 0.0, wxDouble ty = 0.0) wxOVERRIDE;

    void Get(wxDouble* a = NULL, wxDouble* b = NULL,  wxDouble* c = NULL,
        wxDouble* d = NULL, wxDouble* tx = NULL, wxDouble* ty = NULL) const wxOVERRIDE;

    void Invert() wxOVERRIDE;

    bool IsEqual(const wxGraphicsMatrixData* t) const wxOVERRIDE;

    bool IsIdentity() const wxOVERRIDE;

    void Translate(wxDouble dx, wxDouble dy) wxOVERRIDE;

    void Scale(wxDouble xScale, wxDouble yScale) wxOVERRIDE;

    void Rotate(wxDouble angle) wxOVERRIDE;

    void TransformPoint(wxDouble* x, wxDouble* y) const wxOVERRIDE;

    void TransformDistance(wxDouble* dx, wxDouble* dy) const wxOVERRIDE;

    void* GetNativeMatrix() const wxOVERRIDE;

    D2D1::Matrix3x2F GetMatrix3x2F() const;

private:
    D2D1::Matrix3x2F m_matrix;
};

//-----------------------------------------------------------------------------
// wxD2DMatrixData implementation
//-----------------------------------------------------------------------------

wxD2DMatrixData::wxD2DMatrixData(wxGraphicsRenderer* renderer) : wxGraphicsMatrixData(renderer)
{
    m_matrix = D2D1::Matrix3x2F::Identity();
}

wxD2DMatrixData::wxD2DMatrixData(wxGraphicsRenderer* renderer, const D2D1::Matrix3x2F& matrix) :
    wxGraphicsMatrixData(renderer), m_matrix(matrix)
{
}

void wxD2DMatrixData::Concat(const wxGraphicsMatrixData* t)
{
    m_matrix.SetProduct(m_matrix, static_cast<const wxD2DMatrixData*>(t)->m_matrix);
}

void wxD2DMatrixData::Set(wxDouble a, wxDouble b, wxDouble c, wxDouble d, wxDouble tx, wxDouble ty)
{
    m_matrix._11 = a;
    m_matrix._12 = b;
    m_matrix._21 = c;
    m_matrix._22 = d;
    m_matrix._31 = tx;
    m_matrix._32 = ty;
}

void wxD2DMatrixData::Get(wxDouble* a, wxDouble* b,  wxDouble* c, wxDouble* d, wxDouble* tx, wxDouble* ty) const
{
    *a = m_matrix._11;
    *b = m_matrix._12;
    *c = m_matrix._21;
    *d = m_matrix._22;
    *tx = m_matrix._31;
    *ty = m_matrix._32;
}

void wxD2DMatrixData::Invert()
{
    m_matrix.Invert();
}

bool wxD2DMatrixData::IsEqual(const wxGraphicsMatrixData* t) const
{
    return m_matrix == static_cast<const wxD2DMatrixData*>(t)->m_matrix;
}

bool wxD2DMatrixData::IsIdentity() const
{
    return m_matrix.IsIdentity();
}

void wxD2DMatrixData::Translate(wxDouble dx, wxDouble dy)
{
    m_matrix = D2D1::Matrix3x2F::Translation(dx, dy) * m_matrix;
}

void wxD2DMatrixData::Scale(wxDouble xScale, wxDouble yScale)
{
    m_matrix = D2D1::Matrix3x2F::Scale(xScale, yScale) * m_matrix;
}

void wxD2DMatrixData::Rotate(wxDouble angle)
{
    m_matrix = D2D1::Matrix3x2F::Rotation(wxRadToDeg(angle)) * m_matrix;
}

void wxD2DMatrixData::TransformPoint(wxDouble* x, wxDouble* y) const
{
    D2D1_POINT_2F result = m_matrix.TransformPoint(D2D1::Point2F(*x, *y));
    *x = result.x;
    *y = result.y;
}

void wxD2DMatrixData::TransformDistance(wxDouble* dx, wxDouble* dy) const
{
    D2D1::Matrix3x2F noTranslationMatrix = m_matrix;
    noTranslationMatrix._31 = 0;
    noTranslationMatrix._32 = 0;
    D2D1_POINT_2F result = m_matrix.TransformPoint(D2D1::Point2F(*dx, *dy));
    *dx = result.x;
    *dy = result.y;
}

void* wxD2DMatrixData::GetNativeMatrix() const
{
    return (void*)&m_matrix;
}

D2D1::Matrix3x2F wxD2DMatrixData::GetMatrix3x2F() const
{
    return m_matrix;
}

const wxD2DMatrixData* wxGetD2DMatrixData(const wxGraphicsMatrix& matrix)
{
    return static_cast<const wxD2DMatrixData*>(matrix.GetMatrixData());
}

//-----------------------------------------------------------------------------
// wxD2DPathData declaration
//-----------------------------------------------------------------------------

class wxD2DPathData : public wxGraphicsPathData
{
public :

    // ID2D1PathGeometry objects are device-independent resources created
    // from a ID2D1Factory. This means we can safely create the resource outside
    // (the wxD2DRenderer handles this) and store it here since it never gets
    // thrown away by the GPU.
    wxD2DPathData(wxGraphicsRenderer* renderer, ID2D1Factory* d2dFactory);

    ~wxD2DPathData();

    ID2D1PathGeometry* GetPathGeometry();

    // This closes the geometry sink, ensuring all the figures are stored inside
    // the ID2D1PathGeometry. Calling this method is required before any draw operation
    // involving a path.
    void Flush();

    wxGraphicsObjectRefData* Clone() const wxOVERRIDE;

    // begins a new subpath at (x,y)
    void MoveToPoint(wxDouble x, wxDouble y) wxOVERRIDE;

    // adds a straight line from the current point to (x,y)
    void AddLineToPoint(wxDouble x, wxDouble y) wxOVERRIDE;

    // adds a cubic Bezier curve from the current point, using two control points and an end point
    void AddCurveToPoint(wxDouble cx1, wxDouble cy1, wxDouble cx2, wxDouble cy2, wxDouble x, wxDouble y) wxOVERRIDE;

    // adds an arc of a circle centering at (x,y) with radius (r) from startAngle to endAngle
    void AddArc(wxDouble x, wxDouble y, wxDouble r, wxDouble startAngle, wxDouble endAngle, bool clockwise) wxOVERRIDE;

    // gets the last point of the current path, (0,0) if not yet set
    void GetCurrentPoint(wxDouble* x, wxDouble* y) const wxOVERRIDE;

    // adds another path
    void AddPath(const wxGraphicsPathData* path) wxOVERRIDE;

    // closes the current sub-path
    void CloseSubpath() wxOVERRIDE;

    // returns the native path
    void* GetNativePath() const wxOVERRIDE;

    // give the native path returned by GetNativePath() back (there might be some deallocations necessary)
    void UnGetNativePath(void* WXUNUSED(p)) const wxOVERRIDE {};

    // transforms each point of this path by the matrix
    void Transform(const wxGraphicsMatrixData* matrix) wxOVERRIDE;

    // gets the bounding box enclosing all points (possibly including control points)
    void GetBox(wxDouble* x, wxDouble* y, wxDouble* w, wxDouble *h) const wxOVERRIDE;

    bool Contains(wxDouble x, wxDouble y, wxPolygonFillMode fillStyle = wxODDEVEN_RULE) const wxOVERRIDE;

    // appends an ellipsis as a new closed subpath fitting the passed rectangle
    void AddCircle(wxDouble x, wxDouble y, wxDouble r) wxOVERRIDE;

    // appends an ellipse
    void AddEllipse(wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE;

private:
    void EnsureGeometryOpen();

    void EnsureSinkOpen();

    void EnsureFigureOpen(wxDouble x = 0, wxDouble y = 0);

private :
    wxCOMPtr<ID2D1PathGeometry> m_pathGeometry;

    wxCOMPtr<ID2D1GeometrySink> m_geometrySink;

    wxCOMPtr<ID2D1Factory> m_direct2dfactory;

    mutable wxCOMPtr<ID2D1TransformedGeometry> m_transformedGeometry;

    D2D1_POINT_2F m_currentPoint;

    D2D1_MATRIX_3X2_F m_transformMatrix;

    bool m_figureOpened;

    bool m_geometryWritable;
};

//-----------------------------------------------------------------------------
// wxD2DPathData implementation
//-----------------------------------------------------------------------------

wxD2DPathData::wxD2DPathData(wxGraphicsRenderer* renderer, ID2D1Factory* d2dFactory) :
    wxGraphicsPathData(renderer), m_direct2dfactory(d2dFactory),
    m_transformMatrix(D2D1::Matrix3x2F::Identity()),
    m_figureOpened(false), m_geometryWritable(false)
{
    m_direct2dfactory->CreatePathGeometry(&m_pathGeometry);
}

wxD2DPathData::~wxD2DPathData()
{
    Flush();
}

ID2D1PathGeometry* wxD2DPathData::GetPathGeometry()
{
    return m_pathGeometry;
}

wxD2DPathData::wxGraphicsObjectRefData* wxD2DPathData::Clone() const
{
    wxD2DPathData* newPathData = new wxD2DPathData(GetRenderer(), m_direct2dfactory);

    newPathData->EnsureGeometryOpen();
    m_pathGeometry->Stream(newPathData->m_geometrySink);

    return newPathData;
}

void wxD2DPathData::Flush()
{
    if (m_geometrySink != NULL)
    {
        if (m_figureOpened)
        {
            m_geometrySink->EndFigure(D2D1_FIGURE_END_OPEN);
        }

        m_figureOpened = false;
        m_geometrySink->Close();

        m_geometryWritable = false;
    }
}

void wxD2DPathData::EnsureGeometryOpen()
{
    if (!m_geometryWritable) {
        wxCOMPtr<ID2D1PathGeometry> newPathGeometry;
        m_direct2dfactory->CreatePathGeometry(&newPathGeometry);

        m_geometrySink.reset();
        newPathGeometry->Open(&m_geometrySink);

        if (m_pathGeometry != NULL)
        {
            m_pathGeometry->Stream(m_geometrySink);
        }

        m_pathGeometry = newPathGeometry;
        m_geometryWritable = true;
    }
}

void wxD2DPathData::EnsureSinkOpen()
{
    EnsureGeometryOpen();

    if (m_geometrySink == NULL)
    {
        m_geometrySink = NULL;
        m_pathGeometry->Open(&m_geometrySink);
    }
}

void wxD2DPathData::EnsureFigureOpen(wxDouble x, wxDouble y)
{
    EnsureSinkOpen();

    if (!m_figureOpened)
    {
        m_geometrySink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
        m_figureOpened = true;
        m_currentPoint = D2D1::Point2F(x, y);
    }
}

void wxD2DPathData::MoveToPoint(wxDouble x, wxDouble y)
{
    if (m_figureOpened)
    {
        CloseSubpath();
    }

    EnsureFigureOpen(x, y);

    m_currentPoint = D2D1::Point2F(x, y);
}

// adds a straight line from the current point to (x,y)
void wxD2DPathData::AddLineToPoint(wxDouble x, wxDouble y)
{
    EnsureFigureOpen();
    m_geometrySink->AddLine(D2D1::Point2F(x, y));

    m_currentPoint = D2D1::Point2F(x, y);
}

// adds a cubic Bezier curve from the current point, using two control points and an end point
void wxD2DPathData::AddCurveToPoint(wxDouble cx1, wxDouble cy1, wxDouble cx2, wxDouble cy2, wxDouble x, wxDouble y)
{
    EnsureFigureOpen();
    D2D1_BEZIER_SEGMENT bezierSegment = {
        { (FLOAT)cx1, (FLOAT)cy1 },
        { (FLOAT)cx2, (FLOAT)cy2 },
        { (FLOAT)x, (FLOAT)y } };
    m_geometrySink->AddBezier(bezierSegment);

    m_currentPoint = D2D1::Point2F(x, y);
}

// adds an arc of a circle centering at (x,y) with radius (r) from startAngle to endAngle
void wxD2DPathData::AddArc(wxDouble x, wxDouble y, wxDouble r, wxDouble startAngle, wxDouble endAngle, bool clockwise)
{
    wxPoint2DDouble center = wxPoint2DDouble(x, y);
    wxPoint2DDouble start = wxPoint2DDouble(std::cos(startAngle) * r, std::sin(startAngle) * r);
    wxPoint2DDouble end = wxPoint2DDouble(std::cos(endAngle) * r, std::sin(endAngle) * r);

    if (m_figureOpened)
    {
        AddLineToPoint(start.m_x + x, start.m_y + y);
    }
    else
    {
        MoveToPoint(start.m_x + x, start.m_y + y);
    }

    double angle = (end.GetVectorAngle() - start.GetVectorAngle());

    if (!clockwise)
    {
        angle = 360 - angle;
    }

    while (abs(angle) > 360)
    {
        angle -= (angle / abs(angle)) * 360;
    }

    if (angle == 360)
    {
        AddCircle(center.m_x, center.m_y, start.GetVectorLength());
        return;
    }

    D2D1_SWEEP_DIRECTION sweepDirection = clockwise ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;

    D2D1_ARC_SIZE arcSize = angle > 180 ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;

    D2D1_ARC_SEGMENT arcSegment = {
        { (FLOAT)(end.m_x + x), (FLOAT)(end.m_y + y) },     // end point
        { (FLOAT)r, (FLOAT) r },                         // size
        0,                              // rotation
        sweepDirection,                 // sweep direction
        arcSize                         // arc size
    };

    m_geometrySink->AddArc(arcSegment);

    m_currentPoint = D2D1::Point2F(end.m_x + x, end.m_y + y);
}

// appends an ellipsis as a new closed subpath fitting the passed rectangle
void wxD2DPathData::AddCircle(wxDouble x, wxDouble y, wxDouble r)
{
    AddEllipse(x - r, y - r, r * 2, r * 2);
}

// appends an ellipse
void wxD2DPathData::AddEllipse(wxDouble x, wxDouble y, wxDouble w, wxDouble h)
{
    Flush();

    wxCOMPtr<ID2D1EllipseGeometry> ellipseGeometry;
    wxCOMPtr<ID2D1PathGeometry> newPathGeometry;

    D2D1_ELLIPSE ellipse = { { (FLOAT)(x + w / 2), (FLOAT)(y + h / 2) }, (FLOAT)(w / 2), (FLOAT)(h / 2) };

    m_direct2dfactory->CreateEllipseGeometry(ellipse, &ellipseGeometry);

    m_direct2dfactory->CreatePathGeometry(&newPathGeometry);

    m_geometrySink = NULL;
    newPathGeometry->Open(&m_geometrySink);

    m_geometryWritable = true;

    ellipseGeometry->CombineWithGeometry(m_pathGeometry, D2D1_COMBINE_MODE_UNION, NULL, m_geometrySink);

    m_pathGeometry = newPathGeometry;
}

// gets the last point of the current path, (0,0) if not yet set
void wxD2DPathData::GetCurrentPoint(wxDouble* x, wxDouble* y) const
{
    D2D1_POINT_2F transformedPoint = D2D1::Matrix3x2F::ReinterpretBaseType(&m_transformMatrix)->TransformPoint(m_currentPoint);

    if (x != NULL) *x = transformedPoint.x;
    if (y != NULL) *y = transformedPoint.y;
}

// adds another path
void wxD2DPathData::AddPath(const wxGraphicsPathData* path)
{
    const wxD2DPathData* d2dPath = static_cast<const wxD2DPathData*>(path);

    EnsureFigureOpen();

    d2dPath->m_pathGeometry->Stream(m_geometrySink);
}

// closes the current sub-path
void wxD2DPathData::CloseSubpath()
{
    if (m_figureOpened)
    {
        m_geometrySink->EndFigure(D2D1_FIGURE_END_CLOSED);
        m_figureOpened = false;
    }
}

void* wxD2DPathData::GetNativePath() const
{
    m_transformedGeometry.reset();
    m_direct2dfactory->CreateTransformedGeometry(m_pathGeometry, m_transformMatrix, &m_transformedGeometry);
    return m_transformedGeometry;
}

void wxD2DPathData::Transform(const wxGraphicsMatrixData* matrix)
{
    m_transformMatrix = *((D2D1_MATRIX_3X2_F*)(matrix->GetNativeMatrix()));
}

void wxD2DPathData::GetBox(wxDouble* x, wxDouble* y, wxDouble* w, wxDouble *h) const
{
    D2D1_RECT_F bounds;
    m_pathGeometry->GetBounds(D2D1::Matrix3x2F::Identity(), &bounds);
    if (x != NULL) *x = bounds.left;
    if (y != NULL) *y = bounds.top;
    if (w != NULL) *w = bounds.right - bounds.left;
    if (h != NULL) *h = bounds.bottom - bounds.top;
}

bool wxD2DPathData::Contains(wxDouble x, wxDouble y, wxPolygonFillMode WXUNUSED(fillStyle)) const
{
    BOOL result;
    m_pathGeometry->FillContainsPoint(D2D1::Point2F(x, y), D2D1::Matrix3x2F::Identity(), &result);
    return result != 0;
}

wxD2DPathData* wxGetD2DPathData(const wxGraphicsPath& path)
{
    return static_cast<wxD2DPathData*>(path.GetGraphicsData());
}

// This utility class is used to read a color value with the format
// PBGRA from a byte stream and to write a color back to the stream.
// It's used in conjunction with the IWICBitmapSource or IWICBitmap
// pixel data to easily read and write color values.
struct wxPBGRAColor
{
    wxPBGRAColor(BYTE* stream) :
        b(*stream), g(*(stream + 1)), r(*(stream + 2)), a(*(stream + 3))
    {}

    wxPBGRAColor(const wxColor& color) :
        b(color.Blue()), g(color.Green()), r(color.Red()), a(color.Alpha())
    {}

    bool IsBlack() const { return r == 0 && g == 0 && b == 0; }

    void Write(BYTE* stream) const
    {
        *(stream + 0) = b;
        *(stream + 1) = g;
        *(stream + 2) = r;
        *(stream + 3) = a;
    }

    BYTE b, g, r, a;
};

wxCOMPtr<IWICBitmapSource> wxCreateWICBitmap(const WXHBITMAP sourceBitmap, bool hasAlpha = false)
{
    HRESULT hr;

    wxCOMPtr<IWICBitmap> wicBitmap;
    hr = wxWICImagingFactory()->CreateBitmapFromHBITMAP(sourceBitmap, NULL, WICBitmapUseAlpha, &wicBitmap);
    wxCHECK2_HRESULT_RET(hr, wxCOMPtr<IWICBitmapSource>(NULL));

    wxCOMPtr<IWICFormatConverter> converter;
    hr = wxWICImagingFactory()->CreateFormatConverter(&converter);
    wxCHECK2_HRESULT_RET(hr, wxCOMPtr<IWICBitmapSource>(NULL));

    WICPixelFormatGUID pixelFormat = hasAlpha ? GUID_WICPixelFormat32bppPBGRA : GUID_WICPixelFormat32bppBGR;

    hr = converter->Initialize(
        wicBitmap,
        pixelFormat,
        WICBitmapDitherTypeNone, NULL, 0.f,
        WICBitmapPaletteTypeMedianCut);
    wxCHECK2_HRESULT_RET(hr, wxCOMPtr<IWICBitmapSource>(NULL));

    return wxCOMPtr<IWICBitmapSource>(converter);
}

wxCOMPtr<IWICBitmapSource> wxCreateWICBitmap(const wxBitmap& sourceBitmap, bool hasAlpha = false)
{
    return wxCreateWICBitmap(sourceBitmap.GetHBITMAP(), hasAlpha);
}

// WIC Bitmap Source for creating hatch patterned bitmaps
class wxHatchBitmapSource : public IWICBitmapSource
{
public:
    wxHatchBitmapSource(wxBrushStyle brushStyle, const wxColor& color) :
        m_brushStyle(brushStyle), m_color(color), m_refCount(0l)
    {
    }

    HRESULT STDMETHODCALLTYPE GetSize(__RPC__out UINT *width, __RPC__out UINT *height) wxOVERRIDE
    {
        if (width != NULL) *width = 8;
        if (height != NULL) *height = 8;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPixelFormat(__RPC__out WICPixelFormatGUID *pixelFormat) wxOVERRIDE
    {
        if (pixelFormat != NULL) *pixelFormat = GUID_WICPixelFormat32bppPBGRA;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetResolution(__RPC__out double *dpiX, __RPC__out double *dpiY) wxOVERRIDE
    {
        if (dpiX != NULL) *dpiX = 96.0;
        if (dpiY != NULL) *dpiY = 96.0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CopyPalette(__RPC__in_opt IWICPalette *palette) wxOVERRIDE
    {
        palette = NULL;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CopyPixels(
        const WICRect* WXUNUSED(prc),
        UINT WXUNUSED(stride),
        UINT WXUNUSED(bufferSize),
        BYTE *buffer) wxOVERRIDE
    {
        // patterns are encoded in a bit map of size 8 x 8
        static const unsigned char BDIAGONAL_PATTERN[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
        static const unsigned char FDIAGONAL_PATTERN[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
        static const unsigned char CROSSDIAG_PATTERN[8] = { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 };
        static const unsigned char CROSS_PATTERN[8] = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF };
        static const unsigned char HORIZONTAL_PATTERN[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF };
        static const unsigned char VERTICAL_PATTERN[8] = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };

        switch (m_brushStyle)
        {
            case wxBRUSHSTYLE_BDIAGONAL_HATCH:
                CopyPattern(buffer, BDIAGONAL_PATTERN);
                break;
            case wxBRUSHSTYLE_CROSSDIAG_HATCH:
                CopyPattern(buffer, CROSSDIAG_PATTERN);
                break;
            case wxBRUSHSTYLE_FDIAGONAL_HATCH:
                CopyPattern(buffer, FDIAGONAL_PATTERN);
                break;
            case wxBRUSHSTYLE_CROSS_HATCH:
                CopyPattern(buffer, CROSS_PATTERN);
                break;
            case wxBRUSHSTYLE_HORIZONTAL_HATCH:
                CopyPattern(buffer, HORIZONTAL_PATTERN);
                break;
            case wxBRUSHSTYLE_VERTICAL_HATCH:
                CopyPattern(buffer, VERTICAL_PATTERN);
                break;
            default:
                break;
        }

        return S_OK;
    }

    // Implementations adapted from: "Implementing IUnknown in C++"
    // http://msdn.microsoft.com/en-us/library/office/cc839627%28v=office.15%29.aspx

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID referenceId, void** object) wxOVERRIDE
    {
        if (!object)
        {
            return E_INVALIDARG;
        }

        *object = NULL;

        if (referenceId == IID_IUnknown || referenceId == wxIID_IWICBitmapSource)
        {
            *object = (LPVOID)this;
            AddRef();
            return NOERROR;
        }

        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef(void) wxOVERRIDE
    {
        InterlockedIncrement(&m_refCount);
        return m_refCount;
    }

    ULONG STDMETHODCALLTYPE Release(void) wxOVERRIDE
    {
        wxCHECK2_MSG(m_refCount > 0, 0, "Unbalanced number of calls to Release");

        ULONG refCount = InterlockedDecrement(&m_refCount);
        if (m_refCount == 0)
        {
            delete this;
        }
        return refCount;
    }

private:

    // Copies an 8x8 bit pattern to a PBGRA byte buffer
    void CopyPattern(BYTE* buffer, const unsigned char* pattern) const
    {
        static const wxPBGRAColor transparent(wxTransparentColour);

        int k = 0;

        for (int i = 0; i < 8; ++i)
        {
            for (int j = 7; j >= 0; --j)
            {
                bool isColorBit = (pattern[i] & (1 << j)) > 0;
                (isColorBit ? m_color : transparent).Write(buffer + k);
                k += 4;
            }
        }
    }

private:
    // The hatch style produced by this bitmap source
    const wxBrushStyle m_brushStyle;

    // The colour of the hatch
    const wxPBGRAColor m_color;

    // Internally used to implement IUnknown's reference counting
    ULONG m_refCount;
};

// RAII class hosting a WIC bitmap lock used for writing
// pixel data to a WICBitmap
class wxBitmapPixelWriteLock
{
public:
    wxBitmapPixelWriteLock(IWICBitmap* bitmap)
    {
        // Retrieve the size of the bitmap
        UINT w, h;
        bitmap->GetSize(&w, &h);
        WICRect lockSize = {0, 0, (INT)w, (INT)h};

        // Obtain a bitmap lock for exclusive write
        bitmap->Lock(&lockSize, WICBitmapLockWrite, &m_pixelLock);
    }

    IWICBitmapLock* GetLock() { return m_pixelLock; }

private:
    wxCOMPtr<IWICBitmapLock> m_pixelLock;
};

class wxD2DBitmapResourceHolder : public wxD2DResourceHolder<ID2D1Bitmap>
{
public:
    wxD2DBitmapResourceHolder(const wxBitmap& sourceBitmap) :
        m_sourceBitmap(sourceBitmap)
    {
    }

    const wxBitmap& GetSourceBitmap() const { return m_sourceBitmap; }

protected:
    void DoAcquireResource() wxOVERRIDE
    {
        ID2D1RenderTarget* renderTarget = GetContext();

        HRESULT hr;

        if(m_sourceBitmap.GetMask())
        {
            int w = m_sourceBitmap.GetWidth();
            int h = m_sourceBitmap.GetHeight();

            wxCOMPtr<IWICBitmapSource> colorBitmap = wxCreateWICBitmap(m_sourceBitmap);
            wxCOMPtr<IWICBitmapSource> maskBitmap = wxCreateWICBitmap(m_sourceBitmap.GetMask()->GetMaskBitmap());
            wxCOMPtr<IWICBitmap> resultBitmap;

            wxWICImagingFactory()->CreateBitmap(
                w, h,
                GUID_WICPixelFormat32bppPBGRA,
                WICBitmapCacheOnLoad,
                &resultBitmap);

            BYTE* colorBuffer = new BYTE[4 * w * h];
            BYTE* maskBuffer = new BYTE[4 * w * h];
            BYTE* resultBuffer;

            hr = colorBitmap->CopyPixels(NULL, w * 4, 4 * w * h, colorBuffer);
            hr = maskBitmap->CopyPixels(NULL, w * 4, 4 * w * h, maskBuffer);

            {
                wxBitmapPixelWriteLock lock(resultBitmap);

                UINT bufferSize = 0;
                hr = lock.GetLock()->GetDataPointer(&bufferSize, &resultBuffer);

                static const wxPBGRAColor transparentColor(wxTransparentColour);

                // Create the result bitmap
                for (int i = 0; i < w * h * 4; i += 4)
                {
                    wxPBGRAColor color(colorBuffer + i);
                    wxPBGRAColor mask(maskBuffer + i);

                    if (mask.IsBlack())
                    {
                        transparentColor.Write(resultBuffer + i);
                    }
                    else
                    {
                        color.a = 255;
                        color.Write(resultBuffer + i);
                    }
                }
            }

            hr = renderTarget->CreateBitmapFromWicBitmap(resultBitmap, 0, &m_nativeResource);
            wxCHECK_HRESULT_RET(hr);

            delete[] colorBuffer;
            delete[] maskBuffer;
        }
        else
        {
            wxCOMPtr<IWICBitmapSource> bitmapSource = wxCreateWICBitmap(m_sourceBitmap, m_sourceBitmap.HasAlpha());
            hr = renderTarget->CreateBitmapFromWicBitmap(bitmapSource, 0, &m_nativeResource);
        }
    }

private:
    const wxBitmap m_sourceBitmap;
};

//-----------------------------------------------------------------------------
// wxD2DBitmapData declaration
//-----------------------------------------------------------------------------

class wxD2DBitmapData : public wxGraphicsBitmapData, public wxD2DManagedGraphicsData
{
public:
    typedef wxD2DBitmapResourceHolder NativeType;

    wxD2DBitmapData(wxGraphicsRenderer* renderer, const wxBitmap& bitmap) :
        wxGraphicsBitmapData(renderer), m_bitmapHolder(bitmap) {}

    wxD2DBitmapData(wxGraphicsRenderer* renderer, const void* pseudoNativeBitmap) :
        wxGraphicsBitmapData(renderer), m_bitmapHolder(*static_cast<const NativeType*>(pseudoNativeBitmap)) {};

    // returns the native representation
    void* GetNativeBitmap() const wxOVERRIDE;

    wxCOMPtr<ID2D1Bitmap> GetD2DBitmap();

    wxD2DManagedObject* GetManagedObject() wxOVERRIDE
    {
        return &m_bitmapHolder;
    }

private:
    NativeType m_bitmapHolder;
};

//-----------------------------------------------------------------------------
// wxD2DBitmapData implementation
//-----------------------------------------------------------------------------

void* wxD2DBitmapData::GetNativeBitmap() const
{
    return (void*)&m_bitmapHolder;
}

wxCOMPtr<ID2D1Bitmap> wxD2DBitmapData::GetD2DBitmap()
{
    return m_bitmapHolder.GetD2DResource();
}

wxD2DBitmapData* wxGetD2DBitmapData(const wxGraphicsBitmap& bitmap)
{
    return static_cast<wxD2DBitmapData*>(bitmap.GetRefData());
}

// Helper class used to create and safely release a ID2D1GradientStopCollection from wxGraphicsGradientStops
class wxD2DGradientStopsHelper
{
public:
    wxD2DGradientStopsHelper(const wxGraphicsGradientStops& gradientStops, ID2D1RenderTarget* renderTarget)
    {
        int stopCount = gradientStops.GetCount();

        D2D1_GRADIENT_STOP* gradientStopArray = new D2D1_GRADIENT_STOP[stopCount];

        for (int i = 0; i < stopCount; ++i)
        {
            gradientStopArray[i].color = wxD2DConvertColour(gradientStops.Item(i).GetColour());
            gradientStopArray[i].position = gradientStops.Item(i).GetPosition();
        }

        renderTarget->CreateGradientStopCollection(gradientStopArray, stopCount, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &m_gradientStopCollection);

        delete[] gradientStopArray;
    }

    ID2D1GradientStopCollection* GetGradientStopCollection()
    {
        return m_gradientStopCollection;
    }

private:
    wxCOMPtr<ID2D1GradientStopCollection> m_gradientStopCollection;
};

template <typename B>
class wxD2DBrushResourceHolder : public wxD2DResourceHolder<B>
{
public:
    wxD2DBrushResourceHolder(const wxBrush& brush) : m_sourceBrush(brush) {};
    virtual ~wxD2DBrushResourceHolder() {};
protected:
    const wxBrush m_sourceBrush;
};

class wxD2DSolidBrushResourceHolder : public wxD2DBrushResourceHolder<ID2D1SolidColorBrush>
{
public:
    wxD2DSolidBrushResourceHolder(const wxBrush& brush) : wxD2DBrushResourceHolder(brush) {}

protected:
    void DoAcquireResource() wxOVERRIDE
    {
        wxColour colour = m_sourceBrush.GetColour();
        HRESULT hr = GetContext()->CreateSolidColorBrush(wxD2DConvertColour(colour), &m_nativeResource);
        wxCHECK_HRESULT_RET(hr);
    }
};

class wxD2DBitmapBrushResourceHolder : public wxD2DBrushResourceHolder<ID2D1BitmapBrush>
{
public:
    wxD2DBitmapBrushResourceHolder(const wxBrush& brush) : wxD2DBrushResourceHolder(brush) {}

protected:
    void DoAcquireResource() wxOVERRIDE
    {
        // TODO: cache this bitmap
        wxD2DBitmapResourceHolder bitmap(*(m_sourceBrush.GetStipple()));
        bitmap.Bind(GetManager());

        HRESULT result = GetContext()->CreateBitmapBrush(
            bitmap.GetD2DResource(),
            D2D1::BitmapBrushProperties(
            D2D1_EXTEND_MODE_WRAP,
            D2D1_EXTEND_MODE_WRAP,
            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR),
            &m_nativeResource);

        wxCHECK_HRESULT_RET(result);
    }
};

class wxD2DHatchBrushResourceHolder : public wxD2DBrushResourceHolder<ID2D1BitmapBrush>
{
public:
    wxD2DHatchBrushResourceHolder(const wxBrush& brush) : wxD2DBrushResourceHolder(brush) {}

protected:
    void DoAcquireResource() wxOVERRIDE
    {
        wxCOMPtr<wxHatchBitmapSource> hatchBitmapSource(new wxHatchBitmapSource(m_sourceBrush.GetStyle(), m_sourceBrush.GetColour()));

        wxCOMPtr<ID2D1Bitmap> bitmap;

        HRESULT hr = GetContext()->CreateBitmapFromWicBitmap(hatchBitmapSource, &bitmap);
        wxCHECK_HRESULT_RET(hr);

        hr = GetContext()->CreateBitmapBrush(
            bitmap,
            D2D1::BitmapBrushProperties(
            D2D1_EXTEND_MODE_WRAP,
            D2D1_EXTEND_MODE_WRAP,
            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR),
            &m_nativeResource);
        wxCHECK_HRESULT_RET(hr);
    }
};

class wxD2DLinearGradientBrushResourceHolder : public wxD2DResourceHolder<ID2D1LinearGradientBrush>
{
public:
    struct LinearGradientInfo {
        const wxRect direction;
        const wxGraphicsGradientStops stops;
        LinearGradientInfo(wxDouble& x1, wxDouble& y1, wxDouble& x2, wxDouble& y2, const wxGraphicsGradientStops& stops_)
            : direction(x1, y1, x2, y2), stops(stops_) {}
    };

    wxD2DLinearGradientBrushResourceHolder(wxDouble& x1, wxDouble& y1, wxDouble& x2, wxDouble& y2, const wxGraphicsGradientStops& stops)
        : m_linearGradientInfo(x1, y1, x2, y2, stops) {}

protected:
    void DoAcquireResource() wxOVERRIDE
    {
        wxD2DGradientStopsHelper helper(m_linearGradientInfo.stops, GetContext());

        HRESULT hr = GetContext()->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(m_linearGradientInfo.direction.GetX(), m_linearGradientInfo.direction.GetY()),
            D2D1::Point2F(m_linearGradientInfo.direction.GetWidth(), m_linearGradientInfo.direction.GetHeight())),
            helper.GetGradientStopCollection(),
            &m_nativeResource);
        wxCHECK_HRESULT_RET(hr);
    }
private:
    const LinearGradientInfo m_linearGradientInfo;
};

class wxD2DRadialGradientBrushResourceHolder : public wxD2DResourceHolder<ID2D1RadialGradientBrush>
{
public:
    struct RadialGradientInfo {
        const wxRect direction;
        const wxDouble radius;
        const wxGraphicsGradientStops stops;

        RadialGradientInfo(wxDouble x1, wxDouble y1, wxDouble x2, wxDouble y2, wxDouble r, const wxGraphicsGradientStops& stops_)
            : direction(x1, y1, x2, y2), radius(r), stops(stops_) {}
    };

    wxD2DRadialGradientBrushResourceHolder(wxDouble& x1, wxDouble& y1, wxDouble& x2, wxDouble& y2, wxDouble& r, const wxGraphicsGradientStops& stops)
        : m_radialGradientInfo(x1, y1, x2, y2, r, stops) {}

protected:
    void DoAcquireResource() wxOVERRIDE
    {
        wxD2DGradientStopsHelper helper(m_radialGradientInfo.stops, GetContext());

        int xo = m_radialGradientInfo.direction.GetLeft() - m_radialGradientInfo.direction.GetWidth();
        int yo = m_radialGradientInfo.direction.GetTop() - m_radialGradientInfo.direction.GetHeight();

        HRESULT hr = GetContext()->CreateRadialGradientBrush(
            D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(m_radialGradientInfo.direction.GetLeft(), m_radialGradientInfo.direction.GetTop()),
            D2D1::Point2F(xo, yo),
            m_radialGradientInfo.radius, m_radialGradientInfo.radius),
            helper.GetGradientStopCollection(),
            &m_nativeResource);
        wxCHECK_HRESULT_RET(hr);
    }

private:
    const RadialGradientInfo m_radialGradientInfo;
};

//-----------------------------------------------------------------------------
// wxD2DBrushData declaration
//-----------------------------------------------------------------------------

class wxD2DBrushData : public wxGraphicsObjectRefData, public wxD2DManagedGraphicsData
{
public:
    wxD2DBrushData(wxGraphicsRenderer* renderer, const wxBrush brush);

    wxD2DBrushData(wxGraphicsRenderer* renderer);

    void CreateLinearGradientBrush(wxDouble x1, wxDouble y1, wxDouble x2, wxDouble y2, const wxGraphicsGradientStops& stops);

    void CreateRadialGradientBrush(wxDouble xo, wxDouble yo, wxDouble xc, wxDouble yc, wxDouble radius, const wxGraphicsGradientStops& stops);

    ID2D1Brush* GetBrush() const
    {
        return (ID2D1Brush*)(m_brushResourceHolder->GetResource());
    }

    wxD2DManagedObject* GetManagedObject() wxOVERRIDE
    {
        return m_brushResourceHolder.get();
    }

private:
    wxSharedPtr<wxManagedResourceHolder> m_brushResourceHolder;
};

//-----------------------------------------------------------------------------
// wxD2DBrushData implementation
//-----------------------------------------------------------------------------

wxD2DBrushData::wxD2DBrushData(wxGraphicsRenderer* renderer, const wxBrush brush)
    : wxGraphicsObjectRefData(renderer), m_brushResourceHolder(NULL)
{
    if (brush.GetStyle() == wxBRUSHSTYLE_SOLID)
    {
        m_brushResourceHolder = new wxD2DSolidBrushResourceHolder(brush);
    }
    else if (brush.IsHatch())
    {
        m_brushResourceHolder = new wxD2DHatchBrushResourceHolder(brush);
    }
    else
    {
        m_brushResourceHolder = new wxD2DBitmapBrushResourceHolder(brush);
    }
}

wxD2DBrushData::wxD2DBrushData(wxGraphicsRenderer* renderer)
    : wxGraphicsObjectRefData(renderer), m_brushResourceHolder(NULL)
{
}

void wxD2DBrushData::CreateLinearGradientBrush(
    wxDouble x1, wxDouble y1,
    wxDouble x2, wxDouble y2,
    const wxGraphicsGradientStops& stops)
{
    m_brushResourceHolder = new wxD2DLinearGradientBrushResourceHolder(x1, y1, x2, y2, stops);
}

void wxD2DBrushData::CreateRadialGradientBrush(
    wxDouble xo, wxDouble yo,
    wxDouble xc, wxDouble yc,
    wxDouble radius,
    const wxGraphicsGradientStops& stops)
{
    m_brushResourceHolder = new wxD2DRadialGradientBrushResourceHolder(xo, yo, xc, yc, radius, stops);
}

wxD2DBrushData* wxGetD2DBrushData(const wxGraphicsBrush& brush)
{
    return static_cast<wxD2DBrushData*>(brush.GetGraphicsData());
}

bool wxIsHatchPenStyle(wxPenStyle penStyle)
{
    return penStyle >= wxPENSTYLE_FIRST_HATCH && penStyle <= wxPENSTYLE_LAST_HATCH;
}

wxBrushStyle wxConvertPenStyleToBrushStyle(wxPenStyle penStyle)
{
    switch(penStyle)
    {
    case wxPENSTYLE_BDIAGONAL_HATCH:
        return wxBRUSHSTYLE_BDIAGONAL_HATCH;
    case wxPENSTYLE_CROSSDIAG_HATCH:
        return wxBRUSHSTYLE_CROSSDIAG_HATCH;
    case wxPENSTYLE_FDIAGONAL_HATCH:
        return wxBRUSHSTYLE_FDIAGONAL_HATCH;
    case wxPENSTYLE_CROSS_HATCH:
        return wxBRUSHSTYLE_CROSS_HATCH;
    case wxPENSTYLE_HORIZONTAL_HATCH:
        return wxBRUSHSTYLE_HORIZONTAL_HATCH;
    case wxPENSTYLE_VERTICAL_HATCH:
        return wxBRUSHSTYLE_VERTICAL_HATCH;
    default:
        break;
    }

    return wxBRUSHSTYLE_SOLID;
}

//-----------------------------------------------------------------------------
// wxD2DPenData declaration
//-----------------------------------------------------------------------------

class wxD2DPenData : public wxGraphicsObjectRefData, public wxD2DManagedGraphicsData
{
public:
    wxD2DPenData(wxGraphicsRenderer* renderer, ID2D1Factory* direct2dFactory, const wxPen& pen);

    void CreateStrokeStyle(ID2D1Factory* const direct2dfactory);

    ID2D1Brush* GetBrush();

    FLOAT GetWidth();

    ID2D1StrokeStyle* GetStrokeStyle();

    wxD2DManagedObject* GetManagedObject() wxOVERRIDE
    {
        return m_stippleBrush->GetManagedObject();
    }

private:
    // We store the source pen for later when we need to recreate the
    // device-dependent resources.
    const wxPen m_sourcePen;

    // A stroke style is a device-independent resource.
    // Describes the caps, miter limit, line join, and dash information.
    wxCOMPtr<ID2D1StrokeStyle> m_strokeStyle;

    // Drawing outlines with Direct2D requires a brush for the color or stipple.
    wxSharedPtr<wxD2DBrushData> m_stippleBrush;

    // The width of the stroke
    FLOAT m_width;
};

//-----------------------------------------------------------------------------
// wxD2DPenData implementation
//-----------------------------------------------------------------------------

wxD2DPenData::wxD2DPenData(
    wxGraphicsRenderer* renderer,
    ID2D1Factory* direct2dFactory,
    const wxPen& pen)
    : wxGraphicsObjectRefData(renderer), m_sourcePen(pen), m_width(pen.GetWidth())
{
    CreateStrokeStyle(direct2dFactory);

    wxBrush strokeBrush;

    if (m_sourcePen.GetStyle() == wxPENSTYLE_STIPPLE)
    {
        strokeBrush.SetStipple(*(m_sourcePen.GetStipple()));
        strokeBrush.SetStyle(wxBRUSHSTYLE_STIPPLE);
    }
    else if(wxIsHatchPenStyle(m_sourcePen.GetStyle()))
    {
        strokeBrush.SetStyle(wxConvertPenStyleToBrushStyle(m_sourcePen.GetStyle()));
        strokeBrush.SetColour(m_sourcePen.GetColour());
    }
    else
    {
        strokeBrush.SetColour(m_sourcePen.GetColour());
        strokeBrush.SetStyle(wxBRUSHSTYLE_SOLID);
    }

    m_stippleBrush = new wxD2DBrushData(renderer, strokeBrush);
}

void wxD2DPenData::CreateStrokeStyle(ID2D1Factory* const direct2dfactory)
{
    D2D1_CAP_STYLE capStyle = wxD2DConvertPenCap(m_sourcePen.GetCap());
    D2D1_LINE_JOIN lineJoin = wxD2DConvertPenJoin(m_sourcePen.GetJoin());
    D2D1_DASH_STYLE dashStyle = wxD2DConvertPenStyle(m_sourcePen.GetStyle());

    int dashCount = 0;
    FLOAT* dashes = NULL;

    if (dashStyle == D2D1_DASH_STYLE_CUSTOM)
    {
        dashCount = m_sourcePen.GetDashCount();
        dashes = new FLOAT[dashCount];

        for (int i = 0; i < dashCount; ++i)
        {
            dashes[i] = m_sourcePen.GetDash()[i];
        }

    }

    direct2dfactory->CreateStrokeStyle(
        D2D1::StrokeStyleProperties(capStyle, capStyle, capStyle, lineJoin, 0, dashStyle, 0.0f),
        dashes, dashCount,
        &m_strokeStyle);

    delete[] dashes;
}

ID2D1Brush* wxD2DPenData::GetBrush()
{
    return m_stippleBrush->GetBrush();
}

FLOAT wxD2DPenData::GetWidth()
{
    return m_width;
}

ID2D1StrokeStyle* wxD2DPenData::GetStrokeStyle()
{
    return m_strokeStyle;
}

wxD2DPenData* wxGetD2DPenData(const wxGraphicsPen& pen)
{
    return static_cast<wxD2DPenData*>(pen.GetGraphicsData());
}

class wxD2DFontData : public wxGraphicsObjectRefData
{
public:
    wxD2DFontData(wxGraphicsRenderer* renderer, ID2D1Factory* d2d1Factory, const wxFont& font, const wxColor& color);

    wxCOMPtr<IDWriteTextLayout> CreateTextLayout(const wxString& text) const;

    wxD2DBrushData& GetBrushData() { return m_brushData; }

    wxCOMPtr<IDWriteTextFormat> GetTextFormat() const { return m_textFormat; }

    wxCOMPtr<IDWriteFont> GetFont() { return m_font; };

private:
    // The native, device-independent font object
    wxCOMPtr<IDWriteFont> m_font;

    // The native, device-independent font object
    wxCOMPtr<IDWriteTextFormat> m_textFormat;

    // We use a color brush to render the font
    wxD2DBrushData m_brushData;

    bool m_underlined;

    bool m_strikethrough;
};

wxD2DFontData::wxD2DFontData(wxGraphicsRenderer* renderer, ID2D1Factory* d2dFactory, const wxFont& font, const wxColor& color) :
    wxGraphicsObjectRefData(renderer), m_brushData(renderer, wxBrush(color)),
    m_underlined(font.GetUnderlined()), m_strikethrough(font.GetStrikethrough())
{
    HRESULT hr;

    wxCOMPtr<IDWriteGdiInterop> gdiInterop;
    hr = wxDWriteFactory()->GetGdiInterop(&gdiInterop);

    LOGFONTW logfont;
    GetObjectW(font.GetHFONT(), sizeof(logfont), &logfont);

    // Ensure the LOGFONT object contains the correct font face name
    if (logfont.lfFaceName[0] == '\0')
    {
        for (unsigned int i = 0; i < font.GetFaceName().Length(); ++i)
        {
            logfont.lfFaceName[i] = font.GetFaceName().GetChar(i);
        }
    }

    hr = gdiInterop->CreateFontFromLOGFONT(&logfont, &m_font);

    wxCOMPtr<IDWriteFontFamily> fontFamily;
    m_font->GetFontFamily(&fontFamily);

    wxCOMPtr<IDWriteLocalizedStrings> familyNames;
    fontFamily->GetFamilyNames(&familyNames);

    UINT32 length;
    familyNames->GetStringLength(0, &length);

    wchar_t* name = new wchar_t[length+1];
    familyNames->GetString(0, name, length+1);

    FLOAT dpiX, dpiY;
    d2dFactory->GetDesktopDpi(&dpiX, &dpiY);

    hr = wxDWriteFactory()->CreateTextFormat(
        name,
        NULL,
        m_font->GetWeight(),
        m_font->GetStyle(),
        m_font->GetStretch(),
        (FLOAT)(font.GetPixelSize().GetHeight()) / (dpiY / 96.0),
        L"en-us",
        &m_textFormat);

    delete[] name;
}

wxCOMPtr<IDWriteTextLayout> wxD2DFontData::CreateTextLayout(const wxString& text) const
{
    static const FLOAT MAX_WIDTH = FLT_MAX;
    static const FLOAT MAX_HEIGHT = FLT_MAX;

    HRESULT hr;

    wxCOMPtr<IDWriteTextLayout> textLayout;

    hr = wxDWriteFactory()->CreateTextLayout(
        text.c_str(),
        text.length(),
        m_textFormat,
        MAX_WIDTH,
        MAX_HEIGHT,
        &textLayout);

    DWRITE_TEXT_RANGE textRange = { 0, (UINT32) text.length() };

    if (m_underlined)
    {
        textLayout->SetUnderline(true, textRange);
    }

    if (m_strikethrough)
    {
        textLayout->SetStrikethrough(true, textRange);
    }

    return textLayout;
}

wxD2DFontData* wxGetD2DFontData(const wxGraphicsFont& font)
{
    return static_cast<wxD2DFontData*>(font.GetGraphicsData());
}

// A render target resource holder exposes methods relevant
// for native render targets such as resize
class wxD2DRenderTargetResourceHolder : public wxD2DResourceHolder<ID2D1RenderTarget>
{
public:
    // This method is called when an external event signals the underlying DC
    // is resized (e.g. the resizing of a window). Some implementations can leave
    // this method empty, while others must adjust the render target size to match
    // the underlying DC.
    virtual void Resize()
    {
    }

    // We use this method instead of the one provided by the native render target
    // because Direct2D 1.0 render targets do not accept a composition mode
    // parameter, while the device context in Direct2D 1.1 does. This way, we make
    // best use of the capabilities of each render target.
    //
    // The default implementation works for all render targets, but the D2D 1.0
    // render target holders shouldn't need to override it, since none of the
    // 1.0 render targets offer a better version of this method.
    virtual void DrawBitmap(ID2D1Bitmap* bitmap, D2D1_POINT_2F offset,
        D2D1_RECT_F imageRectangle, wxInterpolationQuality interpolationQuality,
        wxCompositionMode WXUNUSED(compositionMode))
    {
        D2D1_RECT_F destinationRectangle = D2D1::RectF(offset.x, offset.y, offset.x + imageRectangle.right, offset.y + imageRectangle.bottom);
        m_nativeResource->DrawBitmap(
            bitmap,
            destinationRectangle,
            1.0f,
            wxD2DConvertBitmapInterpolationMode(interpolationQuality),
            imageRectangle);
    }

    // We use this method instead of the one provided by the native render target
    // because some contexts might require writing to a buffer (e.g. an image
    // context), and some render targets might require additional operations to
    // be executed (e.g. the device context must present the swap chain)
    virtual HRESULT Flush()
    {
        return m_nativeResource->Flush();
    }

    // Composition is not supported at in D2D 1.0, and we only allow for:
    // wxCOMPOSITION_DEST - which is essentially a no-op and is handled
    //                      externally by preventing any draw calls.
    // wxCOMPOSITION_OVER - which copies the source over the destination using
    //                      alpha blending. This is the default way D2D 1.0
    //                      draws images.
    virtual bool SetCompositionMode(wxCompositionMode compositionMode)
    {
        if (compositionMode == wxCOMPOSITION_DEST ||
            compositionMode == wxCOMPOSITION_OVER)
        {
            // There's nothing we can do but notify the caller the composition
            // mode is supported
            return true;
        }

        return false;
    }
};

class wxD2DImageRenderTargetResourceHolder : public wxD2DRenderTargetResourceHolder
{
public:
    wxD2DImageRenderTargetResourceHolder(wxImage* image, ID2D1Factory* factory) :
        m_resultImage(image), m_factory(factory)
    {
    }

    HRESULT Flush() wxOVERRIDE
    {
        HRESULT hr = m_nativeResource->Flush();
        FlushRenderTargetToImage();
        return hr;
    }

    ~wxD2DImageRenderTargetResourceHolder()
    {
        FlushRenderTargetToImage();
    }

protected:
    void DoAcquireResource() wxOVERRIDE
    {
        HRESULT hr;

        // Create a compatible WIC Bitmap
        hr = wxWICImagingFactory()->CreateBitmap(
            m_resultImage->GetWidth(),
            m_resultImage->GetHeight(),
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapCacheOnDemand,
            &m_wicBitmap);
        wxCHECK_HRESULT_RET(hr);

        // Create the render target
        hr = m_factory->CreateWicBitmapRenderTarget(
            m_wicBitmap,
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
            &m_nativeResource);
        wxCHECK_HRESULT_RET(hr);
    }

private:
    void FlushRenderTargetToImage()
    {
        int width = m_resultImage->GetWidth();
        int height = m_resultImage->GetHeight();
        int bufferSize = 4 * width * height;

        BYTE* buffer = new BYTE[bufferSize];
        m_wicBitmap->CopyPixels(NULL, 4 * width, bufferSize, buffer);
        unsigned char* dest = m_resultImage->GetData();

        int k = 0;
        while (k < width * height)
        {
            wxPBGRAColor color = wxPBGRAColor(buffer + k * 4);
            dest[k * 3 + 0] = color.r;
            dest[k * 3 + 1] = color.g;
            dest[k * 3 + 2] = color.b;
            ++k;
        }

        delete[] buffer;
    }

private:
    wxImage* m_resultImage;
    wxCOMPtr<IWICBitmap> m_wicBitmap;

    ID2D1Factory* m_factory;
};

class wxD2DHwndRenderTargetResourceHolder : public wxD2DRenderTargetResourceHolder
{
public:
    typedef ID2D1HwndRenderTarget* ImplementationType;

    wxD2DHwndRenderTargetResourceHolder(HWND hwnd, ID2D1Factory* factory) :
        m_hwnd(hwnd), m_factory(factory)
    {
    }

    void Resize() wxOVERRIDE
    {
        RECT clientRect;
        GetClientRect(m_hwnd, &clientRect);

        D2D1_SIZE_U hwndSize = D2D1::SizeU(
            clientRect.right - clientRect.left,
            clientRect.bottom - clientRect.top);

        D2D1_SIZE_U renderTargetSize = GetRenderTarget()->GetPixelSize();

        if (hwndSize.width != renderTargetSize.width || hwndSize.height != renderTargetSize.height)
        {
            GetRenderTarget()->Resize(hwndSize);
        }
    }

protected:
    void DoAcquireResource() wxOVERRIDE
    {
        wxCOMPtr<ID2D1HwndRenderTarget> renderTarget;

        HRESULT result;

        RECT clientRect;
        GetClientRect(m_hwnd, &clientRect);

        D2D1_SIZE_U size = D2D1::SizeU(
            clientRect.right - clientRect.left,
            clientRect.bottom - clientRect.top);

        result = m_factory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &renderTarget);

        if (FAILED(result))
        {
            wxFAIL_MSG("Could not create Direct2D render target");
        }

        renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

        m_nativeResource = renderTarget;
    }

private:
    // Converts the underlying resource pointer of type
    // ID2D1RenderTarget* to the actual implementation type
    ImplementationType GetRenderTarget()
    {
        return static_cast<ImplementationType>(GetD2DResource().get());
    }

private:
    HWND m_hwnd;
    ID2D1Factory* m_factory;
};

#if wxD2D_DEVICE_CONTEXT_SUPPORTED
class wxD2DDeviceContextResourceHolder : public wxD2DRenderTargetResourceHolder
{
public:
    wxD2DDeviceContextResourceHolder(ID2D1Factory* factory, HWND hwnd) :
        m_factory(NULL), m_hwnd(hwnd)
    {
        HRESULT hr = factory->QueryInterface(IID_ID2D1Factory1, (void**)&m_factory);
        wxCHECK_HRESULT_RET(hr);
    }

    void DrawBitmap(ID2D1Image* image, D2D1_POINT_2F offset,
        D2D1_RECT_F imageRectangle, wxInterpolationQuality interpolationQuality,
        wxCompositionMode compositionMode) wxOVERRIDE
    {
        m_context->DrawImage(image,
            offset,
            imageRectangle,
            wxD2DConvertInterpolationMode(interpolationQuality),
            wxD2DConvertCompositionMode(compositionMode));
    }

    HRESULT Flush() wxOVERRIDE
    {
        HRESULT hr = m_nativeResource->Flush();
        DXGI_PRESENT_PARAMETERS params = { 0 };
        m_swapChain->Present1(1, 0, &params);
        return hr;
    }

protected:

    // Adapted from http://msdn.microsoft.com/en-us/library/windows/desktop/hh780339%28v=vs.85%29.aspx
    void DoAcquireResource() wxOVERRIDE
    {
        HRESULT hr;

        // This flag adds support for surfaces with a different color channel ordering than the API default.
        // You need it for compatibility with Direct2D.
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

        // This array defines the set of DirectX hardware feature levels this app  supports.
        // The ordering is important and you should  preserve it.
        // Don't forget to declare your app's minimum required feature level in its
        // description.  All apps are assumed to support 9.1 unless otherwise stated.
        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };

        // Create the DX11 API device object, and get a corresponding context.
        wxCOMPtr<ID3D11Device> device;
        wxCOMPtr<ID3D11DeviceContext> context;

        hr = D3D11CreateDevice(
            NULL,                    // specify null to use the default adapter
            D3D_DRIVER_TYPE_HARDWARE,
            0,
            creationFlags,              // optionally set debug and Direct2D compatibility flags
            featureLevels,              // list of feature levels this app can support
            ARRAYSIZE(featureLevels),   // number of possible feature levels
            D3D11_SDK_VERSION,
            &device,                    // returns the Direct3D device created
            &m_featureLevel,            // returns feature level of device created
            &context);                  // returns the device immediate context
        wxCHECK_HRESULT_RET(hr);

        // Obtain the underlying DXGI device of the Direct3D11 device.
        hr = device->QueryInterface(IID_IDXGIDevice, (void**)&m_dxgiDevice);
        wxCHECK_HRESULT_RET(hr);

        // Obtain the Direct2D device for 2-D rendering.
        hr = m_factory->CreateDevice(m_dxgiDevice, &m_device);
        wxCHECK_HRESULT_RET(hr);

        // Get Direct2D device's corresponding device context object.
        hr = m_device->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            &m_context);
        wxCHECK_HRESULT_RET(hr);

        m_nativeResource = m_context;

        AttachSurface();
    }

private:
    void AttachSurface()
    {
        HRESULT hr;

        // Allocate a descriptor.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};
        swapChainDesc.Width = 0;
        swapChainDesc.Height = 0;
        swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDesc.Stereo = false;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapChainDesc.Flags = 0;

        // Identify the physical adapter (GPU or card) this device is runs on.
        wxCOMPtr<IDXGIAdapter> dxgiAdapter;
        hr = m_dxgiDevice->GetAdapter(&dxgiAdapter);
        wxCHECK_HRESULT_RET(hr);

        // Get the factory object that created the DXGI device.
        wxCOMPtr<IDXGIFactory2> dxgiFactory;
        hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
        wxCHECK_HRESULT_RET(hr);

        // Get the final swap chain for this window from the DXGI factory.
        hr = dxgiFactory->CreateSwapChainForHwnd(
            m_dxgiDevice,
            m_hwnd,
            &swapChainDesc,
            NULL,    // allow on all displays
            NULL,
            &m_swapChain);
        wxCHECK_HRESULT_RET(hr);

        // Ensure that DXGI doesn't queue more than one frame at a time.
        hr = m_dxgiDevice->SetMaximumFrameLatency(1);
        wxCHECK_HRESULT_RET(hr);

        // Get the backbuffer for this window which is be the final 3D render target.
        wxCOMPtr<ID3D11Texture2D> backBuffer;
        hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        wxCHECK_HRESULT_RET(hr);

        FLOAT dpiX, dpiY;
        m_factory->GetDesktopDpi(&dpiX, &dpiY);

        // Now we set up the Direct2D render target bitmap linked to the swapchain.
        // Whenever we render to this bitmap, it is directly rendered to the
        // swap chain associated with the window.
        D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
            dpiX, dpiY);

        // Direct2D needs the dxgi version of the backbuffer surface pointer.
        wxCOMPtr<IDXGISurface> dxgiBackBuffer;
        hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer));
        wxCHECK_HRESULT_RET(hr);

        // Get a D2D surface from the DXGI back buffer to use as the D2D render target.
        hr = m_context->CreateBitmapFromDxgiSurface(
            dxgiBackBuffer.get(),
            &bitmapProperties,
            &m_targetBitmap);
        wxCHECK_HRESULT_RET(hr);

        // Now we can set the Direct2D render target.
        m_context->SetTarget(m_targetBitmap);
    }

    ~wxD2DDeviceContextResourceHolder()
    {
        DXGI_PRESENT_PARAMETERS params = { 0 };
        m_swapChain->Present1(1, 0, &params);
    }

private:
    ID2D1Factory1* m_factory;

    HWND m_hwnd;

    D3D_FEATURE_LEVEL m_featureLevel;
    wxCOMPtr<IDXGIDevice1> m_dxgiDevice;
    wxCOMPtr<ID2D1Device> m_device;
    wxCOMPtr<ID2D1DeviceContext> m_context;
    wxCOMPtr<ID2D1Bitmap1> m_targetBitmap;
    wxCOMPtr<IDXGISwapChain1> m_swapChain;
};
#endif

class wxD2DDCRenderTargetResourceHolder : public wxD2DRenderTargetResourceHolder
{
public:
    wxD2DDCRenderTargetResourceHolder(ID2D1Factory* factory, HDC hdc, const wxSize dcSize) :
        m_factory(factory), m_hdc(hdc)
    {
        m_dcSize.left = 0;
        m_dcSize.top = 0;
        m_dcSize.right = dcSize.GetWidth();
        m_dcSize.bottom = dcSize.GetHeight();
    }

protected:
    void DoAcquireResource()
    {
        wxCOMPtr<ID2D1DCRenderTarget> renderTarget;
        D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

        HRESULT hr = m_factory->CreateDCRenderTarget(
            &renderTargetProperties,
            &renderTarget);
        wxCHECK_HRESULT_RET(hr);

        hr = renderTarget->BindDC(m_hdc, &m_dcSize);
        wxCHECK_HRESULT_RET(hr);

        m_nativeResource = renderTarget;
    }

private:
    ID2D1Factory* m_factory;
    HDC m_hdc;
    RECT m_dcSize;
};

// The null context has no state of its own and does nothing.
// It is only used as a base class for the lightweight
// measuring context. The measuring context cannot inherit from
// the default implementation wxD2DContext, because some methods
// from wxD2DContext require the presence of a "context"
// (render target) in order to acquire various device-dependent
// resources. Without a proper context, those methods would fail.
// The methods implemented in the null context are fundamentally no-ops.
class wxNullContext : public wxGraphicsContext
{
public:
    wxNullContext(wxGraphicsRenderer* renderer) : wxGraphicsContext(renderer) {}
    void GetTextExtent(const wxString&, wxDouble*, wxDouble*, wxDouble*, wxDouble*) const wxOVERRIDE {}
    void GetPartialTextExtents(const wxString&, wxArrayDouble&) const wxOVERRIDE {}
    void Clip(const wxRegion&) wxOVERRIDE {}
    void Clip(wxDouble, wxDouble, wxDouble, wxDouble) wxOVERRIDE {}
    void ResetClip() wxOVERRIDE {}
    void* GetNativeContext() wxOVERRIDE { return NULL; }
    bool SetAntialiasMode(wxAntialiasMode) wxOVERRIDE { return false; }
    bool SetInterpolationQuality(wxInterpolationQuality) wxOVERRIDE { return false; }
    bool SetCompositionMode(wxCompositionMode) wxOVERRIDE { return false; }
    void BeginLayer(wxDouble) wxOVERRIDE {}
    void EndLayer() wxOVERRIDE {}
    void Translate(wxDouble, wxDouble) wxOVERRIDE {}
    void Scale(wxDouble, wxDouble) wxOVERRIDE {}
    void Rotate(wxDouble) wxOVERRIDE {}
    void ConcatTransform(const wxGraphicsMatrix&) wxOVERRIDE {}
    void SetTransform(const wxGraphicsMatrix&) wxOVERRIDE {}
    wxGraphicsMatrix GetTransform() const wxOVERRIDE { return wxNullGraphicsMatrix; }
    void StrokePath(const wxGraphicsPath&) wxOVERRIDE {}
    void FillPath(const wxGraphicsPath&, wxPolygonFillMode) wxOVERRIDE {}
    void DrawBitmap(const wxGraphicsBitmap&, wxDouble, wxDouble, wxDouble, wxDouble) wxOVERRIDE {}
    void DrawBitmap(const wxBitmap&, wxDouble, wxDouble, wxDouble, wxDouble) wxOVERRIDE {}
    void DrawIcon(const wxIcon&, wxDouble, wxDouble, wxDouble, wxDouble) wxOVERRIDE {}
    void PushState() wxOVERRIDE {}
    void PopState() wxOVERRIDE {}
    void Flush() wxOVERRIDE {}

protected:
    void DoDrawText(const wxString&, wxDouble, wxDouble) wxOVERRIDE {}
};

class wxD2DMeasuringContext : public wxNullContext
{
public:
    wxD2DMeasuringContext(wxGraphicsRenderer* renderer) : wxNullContext(renderer) {}

    void GetTextExtent(const wxString& str, wxDouble* width, wxDouble* height, wxDouble* descent, wxDouble* externalLeading) const wxOVERRIDE
    {
        GetTextExtent(wxGetD2DFontData(m_font), str, width, height, descent, externalLeading);
    }

    void GetPartialTextExtents(const wxString& text, wxArrayDouble& widths) const wxOVERRIDE
    {
        GetPartialTextExtents(wxGetD2DFontData(m_font), text, widths);
    }

    static void GetPartialTextExtents(wxD2DFontData* fontData, const wxString& text, wxArrayDouble& widths)
    {
        for (unsigned int i = 0; i < text.Length(); ++i)
        {
            wxDouble width;
            GetTextExtent(fontData, text.SubString(0, i), &width, NULL, NULL, NULL);
            widths.push_back(width);
        }
    }

    static void GetTextExtent(wxD2DFontData* fontData, const wxString& str, wxDouble* width, wxDouble* height, wxDouble* descent, wxDouble* externalLeading)
    {
        wxCOMPtr<IDWriteTextLayout> textLayout = fontData->CreateTextLayout(str);
        wxCOMPtr<IDWriteFont> font = fontData->GetFont();

        DWRITE_TEXT_METRICS textMetrics;
        textLayout->GetMetrics(&textMetrics);

        DWRITE_FONT_METRICS fontMetrics;
        font->GetMetrics(&fontMetrics);

        FLOAT ratio = fontData->GetTextFormat()->GetFontSize() / (FLOAT)fontMetrics.designUnitsPerEm;

        if (width != NULL) *width = textMetrics.widthIncludingTrailingWhitespace;
        if (height != NULL) *height = textMetrics.height;

        if (descent != NULL) *descent = fontMetrics.descent * ratio;
        if (externalLeading != NULL) *externalLeading = std::max(0.0f, (fontMetrics.ascent + fontMetrics.descent) * ratio - textMetrics.height);
    }
};

//-----------------------------------------------------------------------------
// wxD2DContext declaration
//-----------------------------------------------------------------------------

class wxD2DContext : public wxGraphicsContext, wxD2DResourceManager
{
public:
    wxD2DContext(wxGraphicsRenderer* renderer, ID2D1Factory* direct2dFactory, HWND hwnd);

    wxD2DContext(wxGraphicsRenderer* renderer, ID2D1Factory* direct2dFactory, HDC hdc, const wxSize& dcSize);

    wxD2DContext(wxGraphicsRenderer* renderer, ID2D1Factory* direct2dFactory, wxImage& image);

    wxD2DContext(wxGraphicsRenderer* renderer, ID2D1Factory* direct2dFactory, void* nativeContext);

    ~wxD2DContext();

    void Clip(const wxRegion& region) wxOVERRIDE;

    void Clip(wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE;

    void ResetClip() wxOVERRIDE;

    // The native context used by wxD2DContext is a Direct2D render target.
    void* GetNativeContext() wxOVERRIDE;

    bool SetAntialiasMode(wxAntialiasMode antialias) wxOVERRIDE;

    bool SetInterpolationQuality(wxInterpolationQuality interpolation) wxOVERRIDE;

    bool SetCompositionMode(wxCompositionMode op) wxOVERRIDE;

    void BeginLayer(wxDouble opacity) wxOVERRIDE;

    void EndLayer() wxOVERRIDE;

    void Translate(wxDouble dx, wxDouble dy) wxOVERRIDE;

    void Scale(wxDouble xScale, wxDouble yScale) wxOVERRIDE;

    void Rotate(wxDouble angle) wxOVERRIDE;

    void ConcatTransform(const wxGraphicsMatrix& matrix) wxOVERRIDE;

    void SetTransform(const wxGraphicsMatrix& matrix) wxOVERRIDE;

    wxGraphicsMatrix GetTransform() const wxOVERRIDE;

    void StrokePath(const wxGraphicsPath& p) wxOVERRIDE;

    void FillPath(const wxGraphicsPath& p , wxPolygonFillMode fillStyle = wxODDEVEN_RULE) wxOVERRIDE;

    void DrawRectangle(wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE;

    void DrawRoundedRectangle(wxDouble x, wxDouble y, wxDouble w, wxDouble h, wxDouble radius) wxOVERRIDE;

    void DrawEllipse(wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE;

    void DrawBitmap(const wxGraphicsBitmap& bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE;

    void DrawBitmap(const wxBitmap& bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE;

    void DrawIcon(const wxIcon& icon, wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE;

    void PushState() wxOVERRIDE;

    void PopState() wxOVERRIDE;

    void GetTextExtent(
        const wxString& str,
        wxDouble* width,
        wxDouble* height,
        wxDouble* descent,
        wxDouble* externalLeading) const wxOVERRIDE;

    void GetPartialTextExtents(const wxString& text, wxArrayDouble& widths) const wxOVERRIDE;

    bool ShouldOffset() const wxOVERRIDE;

    void SetPen(const wxGraphicsPen& pen) wxOVERRIDE;

    void Flush() wxOVERRIDE;

    void GetDPI(wxDouble* dpiX, wxDouble* dpiY) wxOVERRIDE;

    wxD2DContextSupplier::ContextType GetContext() wxOVERRIDE
    {
        return GetRenderTarget();
    }

private:
    void Init();

    void DoDrawText(const wxString& str, wxDouble x, wxDouble y) wxOVERRIDE;

    void EnsureInitialized();

    HRESULT CreateRenderTarget();

    void AdjustRenderTargetSize();

    void ReleaseDeviceDependentResources();

    ID2D1RenderTarget* GetRenderTarget() const;

private:
    enum ClipMode
    {
        CLIP_MODE_NONE,
        CLIP_MODE_AXIS_ALIGNED_RECTANGLE,
        CLIP_MODE_GEOMETRY
    };

private:
    ID2D1Factory* m_direct2dFactory;

    wxSharedPtr<wxD2DRenderTargetResourceHolder> m_renderTargetHolder;

    // A ID2D1DrawingStateBlock represents the drawing state of a render target:
    // the anti aliasing mode, transform, tags, and text-rendering options.
    // The context owns these pointers and is responsible for releasing them.
    wxStack<wxCOMPtr<ID2D1DrawingStateBlock>> m_stateStack;

    ClipMode m_clipMode;

    bool m_clipLayerAcquired;

    // A direct2d layer is a device-dependent resource.
    wxCOMPtr<ID2D1Layer> m_clipLayer;

    wxStack<wxCOMPtr<ID2D1Layer>> m_layers;

    ID2D1RenderTarget* m_cachedRenderTarget;

private:
    wxDECLARE_NO_COPY_CLASS(wxD2DContext);
};

//-----------------------------------------------------------------------------
// wxD2DContext implementation
//-----------------------------------------------------------------------------

wxD2DContext::wxD2DContext(wxGraphicsRenderer* renderer, ID2D1Factory* direct2dFactory, HWND hwnd) :
    wxGraphicsContext(renderer), m_direct2dFactory(direct2dFactory),
#if wxD2D_DEVICE_CONTEXT_SUPPORTED
    m_renderTargetHolder(new wxD2DDeviceContextResourceHolder(direct2dFactory, hwnd))
#else
    m_renderTargetHolder(new wxD2DHwndRenderTargetResourceHolder(hwnd, direct2dFactory))
#endif
{
    Init();
}

wxD2DContext::wxD2DContext(wxGraphicsRenderer* renderer, ID2D1Factory* direct2dFactory, HDC hdc, const wxSize& dcSize) :
    wxGraphicsContext(renderer), m_direct2dFactory(direct2dFactory),
    m_renderTargetHolder(new wxD2DDCRenderTargetResourceHolder(direct2dFactory, hdc, dcSize))
{
    Init();
}

wxD2DContext::wxD2DContext(wxGraphicsRenderer* renderer, ID2D1Factory* direct2dFactory, wxImage& image) :
    wxGraphicsContext(renderer), m_direct2dFactory(direct2dFactory),
    m_renderTargetHolder(new wxD2DImageRenderTargetResourceHolder(&image, direct2dFactory))
{
    Init();
}

wxD2DContext::wxD2DContext(wxGraphicsRenderer* renderer, ID2D1Factory* direct2dFactory, void* nativeContext) :
    wxGraphicsContext(renderer), m_direct2dFactory(direct2dFactory)
{
    m_renderTargetHolder = *((wxSharedPtr<wxD2DRenderTargetResourceHolder>*)nativeContext);
    Init();
}

void wxD2DContext::Init()
{
    m_cachedRenderTarget = NULL;
    m_clipMode = CLIP_MODE_NONE;
    m_composition = wxCOMPOSITION_OVER;
    m_clipLayerAcquired = false;
    m_renderTargetHolder->Bind(this);
    m_enableOffset = true;
    EnsureInitialized();
}

wxD2DContext::~wxD2DContext()
{
    ResetClip();

    while (!m_layers.empty())
    {
        EndLayer();
    }

    HRESULT result = GetRenderTarget()->EndDraw();
    wxCHECK_HRESULT_RET(result);

    ReleaseResources();
}

ID2D1RenderTarget* wxD2DContext::GetRenderTarget() const
{
    return m_cachedRenderTarget;
}

void wxD2DContext::Clip(const wxRegion& region)
{
    GetRenderTarget()->Flush();
    ResetClip();

    wxCOMPtr<ID2D1Geometry> clipGeometry = wxD2DConvertRegionToGeometry(m_direct2dFactory, region);

    if (!m_clipLayerAcquired)
    {
        GetRenderTarget()->CreateLayer(&m_clipLayer);
        m_clipLayerAcquired = true;
    }

    GetRenderTarget()->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), clipGeometry), m_clipLayer);

    m_clipMode = CLIP_MODE_GEOMETRY;
}

void wxD2DContext::Clip(wxDouble x, wxDouble y, wxDouble w, wxDouble h)
{
    GetRenderTarget()->Flush();
    ResetClip();

    GetRenderTarget()->PushAxisAlignedClip(
        D2D1::RectF(x, y, x + w, y + h),
        D2D1_ANTIALIAS_MODE_ALIASED);

    m_clipMode = CLIP_MODE_AXIS_ALIGNED_RECTANGLE;
}

void wxD2DContext::ResetClip()
{
    if (m_clipMode == CLIP_MODE_AXIS_ALIGNED_RECTANGLE)
    {
        GetRenderTarget()->PopAxisAlignedClip();
    }

    if (m_clipMode == CLIP_MODE_GEOMETRY)
    {
        GetRenderTarget()->PopLayer();
    }

    m_clipMode = CLIP_MODE_NONE;
}

void* wxD2DContext::GetNativeContext()
{
    return &m_renderTargetHolder;
}

void wxD2DContext::StrokePath(const wxGraphicsPath& p)
{
    if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxD2DOffsetHelper helper(this);

    EnsureInitialized();
    AdjustRenderTargetSize();

    wxD2DPathData* pathData = wxGetD2DPathData(p);
    pathData->Flush();

    if (!m_pen.IsNull())
    {
        wxD2DPenData* penData = wxGetD2DPenData(m_pen);
        penData->Bind(this);
        ID2D1Brush* nativeBrush = penData->GetBrush();
        GetRenderTarget()->DrawGeometry((ID2D1Geometry*)pathData->GetNativePath(), nativeBrush, penData->GetWidth(), penData->GetStrokeStyle());
    }
}

void wxD2DContext::FillPath(const wxGraphicsPath& p , wxPolygonFillMode WXUNUSED(fillStyle))
{
    if (m_composition == wxCOMPOSITION_DEST)
        return;

    EnsureInitialized();
    AdjustRenderTargetSize();

    wxD2DPathData* pathData = wxGetD2DPathData(p);
    pathData->Flush();

    if (!m_brush.IsNull())
    {
        wxD2DBrushData* brushData = wxGetD2DBrushData(m_brush);
        brushData->Bind(this);
        GetRenderTarget()->FillGeometry((ID2D1Geometry*)pathData->GetNativePath(), brushData->GetBrush());
    }
}

bool wxD2DContext::SetAntialiasMode(wxAntialiasMode antialias)
{
    if (m_antialias == antialias)
    {
        return true;
    }

    GetRenderTarget()->SetAntialiasMode(wxD2DConvertAntialiasMode(antialias));

    m_antialias = antialias;
    return true;
}

bool wxD2DContext::SetInterpolationQuality(wxInterpolationQuality interpolation)
{
    // Since different versions of Direct2D have different enumerations for
    // interpolation quality, we deffer the conversion to the method which
    // does the actual drawing.

    m_interpolation = interpolation;
    return true;
}

bool wxD2DContext::SetCompositionMode(wxCompositionMode compositionMode)
{
    if (m_composition == compositionMode)
        return true;

    if (m_renderTargetHolder->SetCompositionMode(compositionMode))
    {
        // the composition mode is supported by the render target
        m_composition = compositionMode;
        return true;
    }

    return false;
}

void wxD2DContext::BeginLayer(wxDouble opacity)
{
    wxCOMPtr<ID2D1Layer> layer;
    GetRenderTarget()->CreateLayer(&layer);
    m_layers.push(layer);

    GetRenderTarget()->PushLayer(
        D2D1::LayerParameters(D2D1::InfiniteRect(),
            NULL,
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
            D2D1::IdentityMatrix(), opacity),
        layer);
}

void wxD2DContext::EndLayer()
{
    if (!m_layers.empty())
    {
        wxCOMPtr<ID2D1Layer> topLayer = m_layers.top();

        GetRenderTarget()->PopLayer();

        HRESULT hr = GetRenderTarget()->Flush();
        wxCHECK_HRESULT_RET(hr);

        m_layers.pop();
    }
}

void wxD2DContext::Translate(wxDouble dx, wxDouble dy)
{
    wxGraphicsMatrix translationMatrix = CreateMatrix();
    translationMatrix.Translate(dx, dy);
    ConcatTransform(translationMatrix);
}

void wxD2DContext::Scale(wxDouble xScale, wxDouble yScale)
{
    wxGraphicsMatrix scaleMatrix = CreateMatrix();
    scaleMatrix.Scale(xScale, yScale);
    ConcatTransform(scaleMatrix);
}

void wxD2DContext::Rotate(wxDouble angle)
{
    wxGraphicsMatrix rotationMatrix = CreateMatrix();
    rotationMatrix.Rotate(angle);
    ConcatTransform(rotationMatrix);
}

void wxD2DContext::ConcatTransform(const wxGraphicsMatrix& matrix)
{
    D2D1::Matrix3x2F localMatrix = wxGetD2DMatrixData(GetTransform())->GetMatrix3x2F();
    D2D1::Matrix3x2F concatMatrix = wxGetD2DMatrixData(matrix)->GetMatrix3x2F();

    D2D1::Matrix3x2F resultMatrix;
    resultMatrix.SetProduct(concatMatrix, localMatrix);

    wxGraphicsMatrix resultTransform;
    resultTransform.SetRefData(new wxD2DMatrixData(GetRenderer(), resultMatrix));

    SetTransform(resultTransform);
}

void wxD2DContext::SetTransform(const wxGraphicsMatrix& matrix)
{
    EnsureInitialized();

    GetRenderTarget()->SetTransform(wxGetD2DMatrixData(matrix)->GetMatrix3x2F());
}

wxGraphicsMatrix wxD2DContext::GetTransform() const
{
    D2D1::Matrix3x2F transformMatrix;

    if (GetRenderTarget() != NULL)
    {
        GetRenderTarget()->GetTransform(&transformMatrix);
    }
    else
    {
        transformMatrix = D2D1::Matrix3x2F::Identity();
    }

    wxD2DMatrixData* matrixData = new wxD2DMatrixData(GetRenderer(), transformMatrix);

    wxGraphicsMatrix matrix;
    matrix.SetRefData(matrixData);

    return matrix;
}

void wxD2DContext::DrawBitmap(const wxGraphicsBitmap& bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h)
{
    if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxD2DBitmapData* bitmapData = wxGetD2DBitmapData(bmp);
    bitmapData->Bind(this);

    m_renderTargetHolder->DrawBitmap(
        bitmapData->GetD2DBitmap(),
        D2D1::Point2F(x, y),
        D2D1::RectF(0, 0, w, h),
        GetInterpolationQuality(),
        GetCompositionMode());
}

void wxD2DContext::DrawBitmap(const wxBitmap& bmp, wxDouble x, wxDouble y, wxDouble w, wxDouble h)
{
    wxGraphicsBitmap graphicsBitmap = CreateBitmap(bmp);
    DrawBitmap(graphicsBitmap, x, y, w, h);
}

void wxD2DContext::DrawIcon(const wxIcon& icon, wxDouble x, wxDouble y, wxDouble w, wxDouble h)
{
    DrawBitmap(wxBitmap(icon), x, y, w, h);
}

void wxD2DContext::PushState()
{
    ID2D1Factory* wxGetD2DFactory(wxGraphicsRenderer* renderer);

    wxCOMPtr<ID2D1DrawingStateBlock> drawStateBlock;
    wxGetD2DFactory(GetRenderer())->CreateDrawingStateBlock(&drawStateBlock);
    GetRenderTarget()->SaveDrawingState(drawStateBlock);

    m_stateStack.push(drawStateBlock);
}

void wxD2DContext::PopState()
{
    wxCHECK_RET(!m_stateStack.empty(), wxT("No state to pop"));

    wxCOMPtr<ID2D1DrawingStateBlock> drawStateBlock = m_stateStack.top();
    m_stateStack.pop();

    GetRenderTarget()->RestoreDrawingState(drawStateBlock);
}

void wxD2DContext::GetTextExtent(
    const wxString& str,
    wxDouble* width,
    wxDouble* height,
    wxDouble* descent,
    wxDouble* externalLeading) const
{
    wxD2DMeasuringContext::GetTextExtent(
        wxGetD2DFontData(m_font), str, width, height, descent, externalLeading);
}

void wxD2DContext::GetPartialTextExtents(const wxString& text, wxArrayDouble& widths) const
{
    return wxD2DMeasuringContext::GetPartialTextExtents(
        wxGetD2DFontData(m_font), text, widths);
}

bool wxD2DContext::ShouldOffset() const
{
    if (!m_enableOffset)
    {
        return false;
    }

    int penWidth = 0;
    if (!m_pen.IsNull())
    {
        penWidth = wxGetD2DPenData(m_pen)->GetWidth();
        penWidth = std::max(penWidth, 1);
    }

    return (penWidth % 2) == 1;
}

void wxD2DContext::DoDrawText(const wxString& str, wxDouble x, wxDouble y)
{
    wxCHECK_RET(!m_font.IsNull(),
        wxT("wxGDIPlusContext::DrawText - no valid font set"));

    if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxD2DFontData* fontData = wxGetD2DFontData(m_font);
    fontData->GetBrushData().Bind(this);

    wxCOMPtr<IDWriteTextLayout> textLayout = fontData->CreateTextLayout(str);

    // Render the text
    GetRenderTarget()->DrawTextLayout(
        D2D1::Point2F(x, y),
        textLayout,
        fontData->GetBrushData().GetBrush());
}

void wxD2DContext::EnsureInitialized()
{
    if (!m_renderTargetHolder->IsResourceAcquired())
    {
        m_cachedRenderTarget = m_renderTargetHolder->GetD2DResource();
        GetRenderTarget()->SetTransform(D2D1::Matrix3x2F::Identity());
        GetRenderTarget()->BeginDraw();
    }
    else
    {
        m_cachedRenderTarget = m_renderTargetHolder->GetD2DResource();
    }
}

void wxD2DContext::SetPen(const wxGraphicsPen& pen)
{
    wxGraphicsContext::SetPen(pen);

    if (!m_pen.IsNull())
    {
        EnsureInitialized();

        wxD2DPenData* penData = wxGetD2DPenData(pen);
        penData->Bind(this);
    }
}

void wxD2DContext::AdjustRenderTargetSize()
{
    m_renderTargetHolder->Resize();
    D2D1_SIZE_F renderTargetSize = m_renderTargetHolder->GetD2DResource()->GetSize();
    m_width = renderTargetSize.width;
    m_height =  renderTargetSize.height;
}

void wxD2DContext::ReleaseDeviceDependentResources()
{
    ReleaseResources();

    m_clipLayer.reset();
    m_clipLayerAcquired = false;
}

void wxD2DContext::DrawRectangle(wxDouble x, wxDouble y, wxDouble w, wxDouble h)
{
    if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxD2DOffsetHelper helper(this);

    EnsureInitialized();
    AdjustRenderTargetSize();

    D2D1_RECT_F rect = { (FLOAT)x, (FLOAT)y, (FLOAT)(x + w), (FLOAT)(y + h) };


    if (!m_brush.IsNull())
    {
        wxD2DBrushData* brushData = wxGetD2DBrushData(m_brush);
        brushData->Bind(this);
        GetRenderTarget()->FillRectangle(rect, brushData->GetBrush());
    }

    if (!m_pen.IsNull())
    {
        wxD2DPenData* penData = wxGetD2DPenData(m_pen);
        penData->Bind(this);
        GetRenderTarget()->DrawRectangle(rect, penData->GetBrush(), penData->GetWidth(), penData->GetStrokeStyle());
    }
}

void wxD2DContext::DrawRoundedRectangle(wxDouble x, wxDouble y, wxDouble w, wxDouble h, wxDouble radius)
{
    if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxD2DOffsetHelper helper(this);

    EnsureInitialized();
    AdjustRenderTargetSize();

    D2D1_RECT_F rect = { (FLOAT)x, (FLOAT)y, (FLOAT)(x + w), (FLOAT)(y + h) };

    D2D1_ROUNDED_RECT roundedRect = { rect, (FLOAT)radius, (FLOAT)radius };

    if (!m_brush.IsNull())
    {
        wxD2DBrushData* brushData = wxGetD2DBrushData(m_brush);
        brushData->Bind(this);
        GetRenderTarget()->FillRoundedRectangle(roundedRect, brushData->GetBrush());
    }

    if (!m_pen.IsNull())
    {
        wxD2DPenData* penData = wxGetD2DPenData(m_pen);
        penData->Bind(this);
        GetRenderTarget()->DrawRoundedRectangle(roundedRect, penData->GetBrush(), penData->GetWidth(), penData->GetStrokeStyle());
    }
}

void wxD2DContext::DrawEllipse(wxDouble x, wxDouble y, wxDouble w, wxDouble h)
{
    if (m_composition == wxCOMPOSITION_DEST)
        return;

    wxD2DOffsetHelper helper(this);

    EnsureInitialized();
    AdjustRenderTargetSize();

    D2D1_ELLIPSE ellipse = {
        { (FLOAT)(x + w / 2), (FLOAT)(y + h / 2) }, // center point
        (FLOAT)(w / 2),                      // radius x
        (FLOAT)(h / 2)                       // radius y
    };

    if (!m_brush.IsNull())
    {
        wxD2DBrushData* brushData = wxGetD2DBrushData(m_brush);
        brushData->Bind(this);
        GetRenderTarget()->FillEllipse(ellipse, brushData->GetBrush());
    }

    if (!m_pen.IsNull())
    {
        wxD2DPenData* penData = wxGetD2DPenData(m_pen);
        penData->Bind(this);
        GetRenderTarget()->DrawEllipse(ellipse, penData->GetBrush(), penData->GetWidth(), penData->GetStrokeStyle());
    }
}

void wxD2DContext::Flush()
{
    HRESULT result = m_renderTargetHolder->Flush();

    if (result == D2DERR_RECREATE_TARGET)
    {
        ReleaseDeviceDependentResources();
    }
}

void wxD2DContext::GetDPI(wxDouble* dpiX, wxDouble* dpiY)
{
    FLOAT x, y;
    GetRenderTarget()->GetDpi(&x, &y);
    if (dpiX != NULL) *dpiX = x;
    if (dpiY != NULL) *dpiY = y;
}

//-----------------------------------------------------------------------------
// wxD2DRenderer declaration
//-----------------------------------------------------------------------------

class wxD2DRenderer : public wxGraphicsRenderer
{
public :
    wxD2DRenderer();

    virtual ~wxD2DRenderer();

    wxGraphicsContext* CreateContext(const wxWindowDC& dc) wxOVERRIDE;

    wxGraphicsContext* CreateContext(const wxMemoryDC& dc) wxOVERRIDE;

#if wxUSE_PRINTING_ARCHITECTURE
    wxGraphicsContext* CreateContext(const wxPrinterDC& dc) wxOVERRIDE;
#endif

#if wxUSE_ENH_METAFILE
    wxGraphicsContext* CreateContext(const wxEnhMetaFileDC& dc) wxOVERRIDE;
#endif

    wxGraphicsContext* CreateContextFromNativeContext(void* context) wxOVERRIDE;

    wxGraphicsContext* CreateContextFromNativeWindow(void* window) wxOVERRIDE;

    wxGraphicsContext* CreateContext(wxWindow* window) wxOVERRIDE;

#if wxUSE_IMAGE
    wxGraphicsContext* CreateContextFromImage(wxImage& image) wxOVERRIDE;
#endif // wxUSE_IMAGE

    wxGraphicsContext* CreateMeasuringContext() wxOVERRIDE;

    wxGraphicsPath CreatePath() wxOVERRIDE;

    wxGraphicsMatrix CreateMatrix(
        wxDouble a = 1.0, wxDouble b = 0.0, wxDouble c = 0.0, wxDouble d = 1.0,
        wxDouble tx = 0.0, wxDouble ty = 0.0) wxOVERRIDE;

    wxGraphicsPen CreatePen(const wxPen& pen) wxOVERRIDE;

    wxGraphicsBrush CreateBrush(const wxBrush& brush) wxOVERRIDE;

    wxGraphicsBrush CreateLinearGradientBrush(
        wxDouble x1, wxDouble y1,
        wxDouble x2, wxDouble y2,
        const wxGraphicsGradientStops& stops) wxOVERRIDE;

    wxGraphicsBrush CreateRadialGradientBrush(
        wxDouble xo, wxDouble yo,
        wxDouble xc, wxDouble yc,
        wxDouble radius,
        const wxGraphicsGradientStops& stops) wxOVERRIDE;

    // create a native bitmap representation
    wxGraphicsBitmap CreateBitmap(const wxBitmap& bitmap) wxOVERRIDE;

#if wxUSE_IMAGE
    wxGraphicsBitmap CreateBitmapFromImage(const wxImage& image) wxOVERRIDE;
    wxImage CreateImageFromBitmap(const wxGraphicsBitmap& bmp) wxOVERRIDE;
#endif

    wxGraphicsFont CreateFont(const wxFont& font, const wxColour& col) wxOVERRIDE;

    wxGraphicsFont CreateFont(
        double size, const wxString& facename,
        int flags = wxFONTFLAG_DEFAULT,
        const wxColour& col = *wxBLACK) wxOVERRIDE;

    // create a graphics bitmap from a native bitmap
    wxGraphicsBitmap CreateBitmapFromNativeBitmap(void* bitmap) wxOVERRIDE;

    // create a sub-image from a native image representation
    wxGraphicsBitmap CreateSubBitmap(const wxGraphicsBitmap& bitmap, wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE;

    wxString GetName() const wxOVERRIDE;
    void GetVersion(int* major, int* minor, int* micro) const wxOVERRIDE;

    ID2D1Factory* GetD2DFactory();

private:
    wxCOMPtr<ID2D1Factory> m_direct2dFactory;

private :
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxD2DRenderer);
};

//-----------------------------------------------------------------------------
// wxD2DRenderer implementation
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxD2DRenderer,wxGraphicsRenderer);

static wxD2DRenderer *gs_D2DRenderer = NULL;

wxGraphicsRenderer* wxGraphicsRenderer::GetDirect2DRenderer()
{
    if (!wxDirect2D::Initialize())
        return NULL;

    if (!gs_D2DRenderer)
    {
        gs_D2DRenderer = new wxD2DRenderer();
    }

    return gs_D2DRenderer;
}

wxD2DRenderer::wxD2DRenderer()
{

    HRESULT result;
    result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_direct2dFactory);

    if (FAILED(result))
        wxFAIL_MSG("Could not create Direct2D Factory.");
}

wxD2DRenderer::~wxD2DRenderer()
{
    m_direct2dFactory.reset();
}

wxGraphicsContext* wxD2DRenderer::CreateContext(const wxWindowDC& dc)
{
    int width, height;
    dc.GetSize(&width, &height);

    return new wxD2DContext(this, m_direct2dFactory, dc.GetHDC(), wxSize(width, height));
}

wxGraphicsContext* wxD2DRenderer::CreateContext(const wxMemoryDC& dc)
{
    int width, height;
    dc.GetSize(&width, &height);

    return new wxD2DContext(this, m_direct2dFactory, dc.GetHDC(), wxSize(width, height));
}

#if wxUSE_PRINTING_ARCHITECTURE
wxGraphicsContext* wxD2DRenderer::CreateContext(const wxPrinterDC& WXUNUSED(dc))
{
    wxFAIL_MSG("not implemented");
    return NULL;
}
#endif

#if wxUSE_ENH_METAFILE
wxGraphicsContext* wxD2DRenderer::CreateContext(const wxEnhMetaFileDC& WXUNUSED(dc))
{
    wxFAIL_MSG("not implemented");
    return NULL;
}
#endif

wxGraphicsContext* wxD2DRenderer::CreateContextFromNativeContext(void* nativeContext)
{
    return new wxD2DContext(this, m_direct2dFactory, nativeContext);
}

wxGraphicsContext* wxD2DRenderer::CreateContextFromNativeWindow(void* window)
{
    return new wxD2DContext(this, m_direct2dFactory, (HWND)window);
}

wxGraphicsContext* wxD2DRenderer::CreateContext(wxWindow* window)
{
    return new wxD2DContext(this, m_direct2dFactory, (HWND)window->GetHWND());
}

#if wxUSE_IMAGE
wxGraphicsContext* wxD2DRenderer::CreateContextFromImage(wxImage& image)
{
    return new wxD2DContext(this, m_direct2dFactory, image);
}
#endif // wxUSE_IMAGE

wxGraphicsContext* wxD2DRenderer::CreateMeasuringContext()
{
    return new wxD2DMeasuringContext(this);
}

wxGraphicsPath wxD2DRenderer::CreatePath()
{
    wxGraphicsPath p;
    p.SetRefData(new wxD2DPathData(this, m_direct2dFactory));

    return p;
}

wxGraphicsMatrix wxD2DRenderer::CreateMatrix(
    wxDouble a, wxDouble b, wxDouble c, wxDouble d,
    wxDouble tx, wxDouble ty)
{
    wxD2DMatrixData* matrixData = new wxD2DMatrixData(this);
    matrixData->Set(a, b, c, d, tx, ty);

    wxGraphicsMatrix matrix;
    matrix.SetRefData(matrixData);

    return matrix;
}

wxGraphicsPen wxD2DRenderer::CreatePen(const wxPen& pen)
{
    if ( !pen.IsOk() || pen.GetStyle() == wxPENSTYLE_TRANSPARENT )
    {
        return wxNullGraphicsPen;
    }
    else
    {
        wxGraphicsPen p;
        wxD2DPenData* penData = new wxD2DPenData(this, m_direct2dFactory, pen);
        p.SetRefData(penData);
        return p;
    }
}

wxGraphicsBrush wxD2DRenderer::CreateBrush(const wxBrush& brush)
{
    if ( !brush.IsOk() || brush.GetStyle() == wxBRUSHSTYLE_TRANSPARENT )
    {
        return wxNullGraphicsBrush;
    }
    else
    {
        wxGraphicsBrush b;
        b.SetRefData(new wxD2DBrushData(this, brush));
        return b;
    }
}

wxGraphicsBrush wxD2DRenderer::CreateLinearGradientBrush(
    wxDouble x1, wxDouble y1,
    wxDouble x2, wxDouble y2,
    const wxGraphicsGradientStops& stops)
{
    wxD2DBrushData* brushData = new wxD2DBrushData(this);
    brushData->CreateLinearGradientBrush(x1, y1, x2, y2, stops);

    wxGraphicsBrush brush;
    brush.SetRefData(brushData);

    return brush;
}

wxGraphicsBrush wxD2DRenderer::CreateRadialGradientBrush(
    wxDouble xo, wxDouble yo,
    wxDouble xc, wxDouble yc,
    wxDouble radius,
    const wxGraphicsGradientStops& stops)
{
    wxD2DBrushData* brushData = new wxD2DBrushData(this);
    brushData->CreateRadialGradientBrush(xo, yo, xc, yc, radius, stops);

    wxGraphicsBrush brush;
    brush.SetRefData(brushData);

    return brush;
}

// create a native bitmap representation
wxGraphicsBitmap wxD2DRenderer::CreateBitmap(const wxBitmap& bitmap)
{
    wxD2DBitmapData* bitmapData = new wxD2DBitmapData(this, bitmap);

    wxGraphicsBitmap graphicsBitmap;
    graphicsBitmap.SetRefData(bitmapData);

    return graphicsBitmap;
}

// create a graphics bitmap from a native bitmap
wxGraphicsBitmap wxD2DRenderer::CreateBitmapFromNativeBitmap(void* bitmap)
{
    wxD2DBitmapData* bitmapData = new wxD2DBitmapData(this, bitmap);

    wxGraphicsBitmap graphicsBitmap;
    graphicsBitmap.SetRefData(bitmapData);

    return graphicsBitmap;
}

#if wxUSE_IMAGE
wxGraphicsBitmap wxD2DRenderer::CreateBitmapFromImage(const wxImage& image)
{
    return CreateBitmap(wxBitmap(image));
}

wxImage wxD2DRenderer::CreateImageFromBitmap(const wxGraphicsBitmap& bmp)
{
    return static_cast<wxD2DBitmapData::NativeType*>(bmp.GetNativeBitmap())
        ->GetSourceBitmap().ConvertToImage();
}
#endif

wxGraphicsFont wxD2DRenderer::CreateFont(const wxFont& font, const wxColour& col)
{
    wxD2DFontData* fontData = new wxD2DFontData(this, GetD2DFactory(), font, col);

    wxGraphicsFont graphicsFont;
    graphicsFont.SetRefData(fontData);

    return graphicsFont;
}

wxGraphicsFont wxD2DRenderer::CreateFont(
    double size, const wxString& facename,
    int flags,
    const wxColour& col)
{
    return CreateFont(
        wxFontInfo(size).AllFlags(flags).FaceName(facename),
        col);
}

// create a sub-image from a native image representation
wxGraphicsBitmap wxD2DRenderer::CreateSubBitmap(const wxGraphicsBitmap& bitmap, wxDouble x, wxDouble y, wxDouble w, wxDouble h)
{
    typedef wxD2DBitmapData::NativeType* NativeBitmap;
    wxBitmap sourceBitmap = static_cast<NativeBitmap>(bitmap.GetNativeBitmap())->GetSourceBitmap();
    return CreateBitmap(sourceBitmap.GetSubBitmap(wxRect(x, y, w, h)));
}

wxString wxD2DRenderer::GetName() const
{
    return "direct2d";
}

void wxD2DRenderer::GetVersion(int* major, int* minor, int* micro) const
{
    if (wxDirect2D::HasDirect2DSupport())
    {
        if (major)
            *major = 1;

        if (minor)
        {
            switch(wxDirect2D::GetDirect2DVersion())
            {
            case wxDirect2D::wxD2D_VERSION_1_0:
                *minor = 0;
                break;
            case wxDirect2D::wxD2D_VERSION_1_1:
                *minor = 1;
                break;
            }
        }

        if (micro)
            *micro = 0;
    }
}

ID2D1Factory* wxD2DRenderer::GetD2DFactory()
{
    return m_direct2dFactory;
}

ID2D1Factory* wxGetD2DFactory(wxGraphicsRenderer* renderer)
{
    return static_cast<wxD2DRenderer*>(renderer)->GetD2DFactory();
}

// ----------------------------------------------------------------------------
// Module ensuring all global/singleton objects are destroyed on shutdown.
// ----------------------------------------------------------------------------

class wxDirect2DModule : public wxModule
{
public:
    wxDirect2DModule()
    {
    }

    virtual bool OnInit() wxOVERRIDE
    {
        return true;
    }

    virtual void OnExit() wxOVERRIDE
    {
        if ( gs_WICImagingFactory )
        {
            gs_WICImagingFactory->Release();
            gs_WICImagingFactory = NULL;
        }

        if ( gs_IDWriteFactory )
        {
            gs_IDWriteFactory->Release();
            gs_IDWriteFactory = NULL;
        }

        if ( gs_D2DRenderer )
        {
            delete gs_D2DRenderer;
            gs_D2DRenderer = NULL;
        }
    }

private:
    wxDECLARE_DYNAMIC_CLASS(wxDirect2DModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxDirect2DModule, wxModule);


#endif // wxUSE_GRAPHICS_DIRECT2D

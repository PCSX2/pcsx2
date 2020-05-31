///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/gdiplus.cpp
// Purpose:     implements wrappers for GDI+ flat API
// Author:      Vadim Zeitlin
// Created:     2007-03-14
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_GRAPHICS_CONTEXT

#ifndef WX_PRECOMP
    #include "wx/cpp.h"
    #include "wx/log.h"
    #include "wx/module.h"
    #include "wx/string.h"
#endif // WX_PRECOMP

#include "wx/dynload.h"

#include "wx/msw/wrapgdip.h"

// w32api headers used by both MinGW and Cygwin wrongly define UINT16 inside
// Gdiplus namespace in gdiplus.h which results in ambiguity errors when using
// this type as UINT16 is also defined in global scope by windows.h (or rather
// basetsd.h included from it), so we redefine it to work around this problem.
#if defined(__CYGWIN__) || defined(__MINGW32__)
    #define UINT16 unsigned short
#endif

// ----------------------------------------------------------------------------
// helper macros
// ----------------------------------------------------------------------------

// return the name we use for the type of the function with the given name
// (without "Gdip" prefix)
#define wxGDIPLUS_FUNC_T(name) Gdip##name##_t

// to avoid repeating all (several hundreds) of GDI+ functions names several
// times in this file, we define a macro which allows us to apply another macro
// to all (or almost all, as we sometimes have to handle functions not
// returning GpStatus separately) these functions at once

// this macro expands into an invocation of the given macro m for all GDI+
// functions returning standard GpStatus
//
// m is called with the name of the function without "Gdip" prefix as the first
// argument, the list of function parameters with their names as the second one
// and the list of just the parameter names as the third one
#define wxFOR_ALL_GDIPLUS_STATUS_FUNCS(m) \
    m(CreatePath, (GpFillMode brushMode, GpPath **path), (brushMode, path)) \
    m(CreatePath2, (GDIPCONST GpPointF* a1, GDIPCONST BYTE* a2, INT a3, GpFillMode a4, GpPath **path), (a1, a2, a3, a4, path)) \
    m(CreatePath2I, (GDIPCONST GpPoint* a1, GDIPCONST BYTE* a2, INT a3, GpFillMode a4, GpPath **path), (a1, a2, a3, a4, path)) \
    m(ClonePath, (GpPath* path, GpPath **clonePath), (path, clonePath)) \
    m(DeletePath, (GpPath* path), (path)) \
    m(ResetPath, (GpPath* path), (path)) \
    m(GetPointCount, (GpPath* path, INT* count), (path, count)) \
    m(GetPathTypes, (GpPath* path, BYTE* types, INT count), (path, types, count)) \
    m(GetPathPoints, (GpPath* a1, GpPointF* points, INT count), (a1, points, count)) \
    m(GetPathPointsI, (GpPath* a1, GpPoint* points, INT count), (a1, points, count)) \
    m(GetPathFillMode, (GpPath *path, GpFillMode *fillmode), (path, fillmode)) \
    m(SetPathFillMode, (GpPath *path, GpFillMode fillmode), (path, fillmode)) \
    m(GetPathData, (GpPath *path, GpPathData* pathData), (path, pathData)) \
    m(StartPathFigure, (GpPath *path), (path)) \
    m(ClosePathFigure, (GpPath *path), (path)) \
    m(ClosePathFigures, (GpPath *path), (path)) \
    m(SetPathMarker, (GpPath* path), (path)) \
    m(ClearPathMarkers, (GpPath* path), (path)) \
    m(ReversePath, (GpPath* path), (path)) \
    m(GetPathLastPoint, (GpPath* path, GpPointF* lastPoint), (path, lastPoint)) \
    m(AddPathLine, (GpPath *path, REAL x1, REAL y1, REAL x2, REAL y2), (path, x1, y1, x2, y2)) \
    m(AddPathLine2, (GpPath *path, GDIPCONST GpPointF *points, INT count), (path, points, count)) \
    m(AddPathArc, (GpPath *path, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle), (path, x, y, width, height, startAngle, sweepAngle)) \
    m(AddPathBezier, (GpPath *path, REAL x1, REAL y1, REAL x2, REAL y2, REAL x3, REAL y3, REAL x4, REAL y4), (path, x1, y1, x2, y2, x3, y3, x4, y4)) \
    m(AddPathBeziers, (GpPath *path, GDIPCONST GpPointF *points, INT count), (path, points, count)) \
    m(AddPathCurve, (GpPath *path, GDIPCONST GpPointF *points, INT count), (path, points, count)) \
    m(AddPathCurve2, (GpPath *path, GDIPCONST GpPointF *points, INT count, REAL tension), (path, points, count, tension)) \
    m(AddPathCurve3, (GpPath *path, GDIPCONST GpPointF *points, INT count, INT offset, INT numberOfSegments, REAL tension), (path, points, count, offset, numberOfSegments, tension)) \
    m(AddPathClosedCurve, (GpPath *path, GDIPCONST GpPointF *points, INT count), (path, points, count)) \
    m(AddPathClosedCurve2, (GpPath *path, GDIPCONST GpPointF *points, INT count, REAL tension), (path, points, count, tension)) \
    m(AddPathRectangle, (GpPath *path, REAL x, REAL y, REAL width, REAL height), (path, x, y, width, height)) \
    m(AddPathRectangles, (GpPath *path, GDIPCONST GpRectF *rects, INT count), (path, rects, count)) \
    m(AddPathEllipse, (GpPath *path, REAL x, REAL y, REAL width, REAL height), (path, x, y, width, height)) \
    m(AddPathPie, (GpPath *path, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle), (path, x, y, width, height, startAngle, sweepAngle)) \
    m(AddPathPolygon, (GpPath *path, GDIPCONST GpPointF *points, INT count), (path, points, count)) \
    m(AddPathPath, (GpPath *path, GDIPCONST GpPath* addingPath, BOOL connect), (path, addingPath, connect)) \
    m(AddPathString, (GpPath *path, GDIPCONST WCHAR *string, INT length, GDIPCONST GpFontFamily *family, INT style, REAL emSize, GDIPCONST RectF *layoutRect, GDIPCONST GpStringFormat *format), (path, string, length, family, style, emSize, layoutRect, format)) \
    m(AddPathStringI, (GpPath *path, GDIPCONST WCHAR *string, INT length, GDIPCONST GpFontFamily *family, INT style, REAL emSize, GDIPCONST Rect *layoutRect, GDIPCONST GpStringFormat *format), (path, string, length, family, style, emSize, layoutRect, format)) \
    m(AddPathLineI, (GpPath *path, INT x1, INT y1, INT x2, INT y2), (path, x1, y1, x2, y2)) \
    m(AddPathLine2I, (GpPath *path, GDIPCONST GpPoint *points, INT count), (path, points, count)) \
    m(AddPathArcI, (GpPath *path, INT x, INT y, INT width, INT height, REAL startAngle, REAL sweepAngle), (path, x, y, width, height, startAngle, sweepAngle)) \
    m(AddPathBezierI, (GpPath *path, INT x1, INT y1, INT x2, INT y2, INT x3, INT y3, INT x4, INT y4), (path, x1, y1, x2, y2, x3, y3, x4, y4)) \
    m(AddPathBeziersI, (GpPath *path, GDIPCONST GpPoint *points, INT count), (path, points, count)) \
    m(AddPathCurveI, (GpPath *path, GDIPCONST GpPoint *points, INT count), (path, points, count)) \
    m(AddPathCurve2I, (GpPath *path, GDIPCONST GpPoint *points, INT count, REAL tension), (path, points, count, tension)) \
    m(AddPathCurve3I, (GpPath *path, GDIPCONST GpPoint *points, INT count, INT offset, INT numberOfSegments, REAL tension), (path, points, count, offset, numberOfSegments, tension)) \
    m(AddPathClosedCurveI, (GpPath *path, GDIPCONST GpPoint *points, INT count), (path, points, count)) \
    m(AddPathClosedCurve2I, (GpPath *path, GDIPCONST GpPoint *points, INT count, REAL tension), (path, points, count, tension)) \
    m(AddPathRectangleI, (GpPath *path, INT x, INT y, INT width, INT height), (path, x, y, width, height)) \
    m(AddPathRectanglesI, (GpPath *path, GDIPCONST GpRect *rects, INT count), (path, rects, count)) \
    m(AddPathEllipseI, (GpPath *path, INT x, INT y, INT width, INT height), (path, x, y, width, height)) \
    m(AddPathPieI, (GpPath *path, INT x, INT y, INT width, INT height, REAL startAngle, REAL sweepAngle), (path, x, y, width, height, startAngle, sweepAngle)) \
    m(AddPathPolygonI, (GpPath *path, GDIPCONST GpPoint *points, INT count), (path, points, count)) \
    m(FlattenPath, (GpPath *path, GpMatrix* matrix, REAL flatness), (path, matrix, flatness)) \
    m(WindingModeOutline, (GpPath *path, GpMatrix *matrix, REAL flatness), (path, matrix, flatness)) \
    m(WidenPath, (GpPath *nativePath, GpPen *pen, GpMatrix *matrix, REAL flatness), (nativePath, pen, matrix, flatness)) \
    m(WarpPath, (GpPath *path, GpMatrix* matrix, GDIPCONST GpPointF *points, INT count, REAL srcx, REAL srcy, REAL srcwidth, REAL srcheight, WarpMode warpMode, REAL flatness), (path, matrix, points, count, srcx, srcy, srcwidth, srcheight, warpMode, flatness)) \
    m(TransformPath, (GpPath* path, GpMatrix* matrix), (path, matrix)) \
    m(GetPathWorldBounds, (GpPath* path, GpRectF* bounds, GDIPCONST GpMatrix *matrix, GDIPCONST GpPen *pen), (path, bounds, matrix, pen)) \
    m(GetPathWorldBoundsI, (GpPath* path, GpRect* bounds, GDIPCONST GpMatrix *matrix, GDIPCONST GpPen *pen), (path, bounds, matrix, pen)) \
    m(IsVisiblePathPoint, (GpPath* path, REAL x, REAL y, GpGraphics *graphics, BOOL *result), (path, x, y, graphics, result)) \
    m(IsVisiblePathPointI, (GpPath* path, INT x, INT y, GpGraphics *graphics, BOOL *result), (path, x, y, graphics, result)) \
    m(IsOutlineVisiblePathPoint, (GpPath* path, REAL x, REAL y, GpPen *pen, GpGraphics *graphics, BOOL *result), (path, x, y, pen, graphics, result)) \
    m(IsOutlineVisiblePathPointI, (GpPath* path, INT x, INT y, GpPen *pen, GpGraphics *graphics, BOOL *result), (path, x, y, pen, graphics, result)) \
    m(CreatePathIter, (GpPathIterator **iterator, GpPath* path), (iterator, path)) \
    m(DeletePathIter, (GpPathIterator *iterator), (iterator)) \
    m(PathIterNextSubpath, (GpPathIterator* iterator, INT *resultCount, INT* startIndex, INT* endIndex, BOOL* isClosed), (iterator, resultCount, startIndex, endIndex, isClosed)) \
    m(PathIterNextSubpathPath, (GpPathIterator* iterator, INT* resultCount, GpPath* path, BOOL* isClosed), (iterator, resultCount, path, isClosed)) \
    m(PathIterNextPathType, (GpPathIterator* iterator, INT* resultCount, BYTE* pathType, INT* startIndex, INT* endIndex), (iterator, resultCount, pathType, startIndex, endIndex)) \
    m(PathIterNextMarker, (GpPathIterator* iterator, INT *resultCount, INT* startIndex, INT* endIndex), (iterator, resultCount, startIndex, endIndex)) \
    m(PathIterNextMarkerPath, (GpPathIterator* iterator, INT* resultCount, GpPath* path), (iterator, resultCount, path)) \
    m(PathIterGetCount, (GpPathIterator* iterator, INT* count), (iterator, count)) \
    m(PathIterGetSubpathCount, (GpPathIterator* iterator, INT* count), (iterator, count)) \
    m(PathIterIsValid, (GpPathIterator* iterator, BOOL* valid), (iterator, valid)) \
    m(PathIterHasCurve, (GpPathIterator* iterator, BOOL* hasCurve), (iterator, hasCurve)) \
    m(PathIterRewind, (GpPathIterator* iterator), (iterator)) \
    m(PathIterEnumerate, (GpPathIterator* iterator, INT* resultCount, GpPointF *points, BYTE *types, INT count), (iterator, resultCount, points, types, count)) \
    m(PathIterCopyData, (GpPathIterator* iterator, INT* resultCount, GpPointF* points, BYTE* types, INT startIndex, INT endIndex), (iterator, resultCount, points, types, startIndex, endIndex)) \
    m(CreateMatrix, (GpMatrix **matrix), (matrix)) \
    m(CreateMatrix2, (REAL m11, REAL m12, REAL m21, REAL m22, REAL dx, REAL dy, GpMatrix **matrix), (m11, m12, m21, m22, dx, dy, matrix)) \
    m(CreateMatrix3, (GDIPCONST GpRectF *rect, GDIPCONST GpPointF *dstplg, GpMatrix **matrix), (rect, dstplg, matrix)) \
    m(CreateMatrix3I, (GDIPCONST GpRect *rect, GDIPCONST GpPoint *dstplg, GpMatrix **matrix), (rect, dstplg, matrix)) \
    m(CloneMatrix, (GpMatrix *matrix, GpMatrix **cloneMatrix), (matrix, cloneMatrix)) \
    m(DeleteMatrix, (GpMatrix *matrix), (matrix)) \
    m(SetMatrixElements, (GpMatrix *matrix, REAL m11, REAL m12, REAL m21, REAL m22, REAL dx, REAL dy), (matrix, m11, m12, m21, m22, dx, dy)) \
    m(MultiplyMatrix, (GpMatrix *matrix, GpMatrix* matrix2, GpMatrixOrder order), (matrix, matrix2, order)) \
    m(TranslateMatrix, (GpMatrix *matrix, REAL offsetX, REAL offsetY, GpMatrixOrder order), (matrix, offsetX, offsetY, order)) \
    m(ScaleMatrix, (GpMatrix *matrix, REAL scaleX, REAL scaleY, GpMatrixOrder order), (matrix, scaleX, scaleY, order)) \
    m(RotateMatrix, (GpMatrix *matrix, REAL angle, GpMatrixOrder order), (matrix, angle, order)) \
    m(ShearMatrix, (GpMatrix *matrix, REAL shearX, REAL shearY, GpMatrixOrder order), (matrix, shearX, shearY, order)) \
    m(InvertMatrix, (GpMatrix *matrix), (matrix)) \
    m(TransformMatrixPoints, (GpMatrix *matrix, GpPointF *pts, INT count), (matrix, pts, count)) \
    m(TransformMatrixPointsI, (GpMatrix *matrix, GpPoint *pts, INT count), (matrix, pts, count)) \
    m(VectorTransformMatrixPoints, (GpMatrix *matrix, GpPointF *pts, INT count), (matrix, pts, count)) \
    m(VectorTransformMatrixPointsI, (GpMatrix *matrix, GpPoint *pts, INT count), (matrix, pts, count)) \
    m(GetMatrixElements, (GDIPCONST GpMatrix *matrix, REAL *matrixOut), (matrix, matrixOut)) \
    m(IsMatrixInvertible, (GDIPCONST GpMatrix *matrix, BOOL *result), (matrix, result)) \
    m(IsMatrixIdentity, (GDIPCONST GpMatrix *matrix, BOOL *result), (matrix, result)) \
    m(IsMatrixEqual, (GDIPCONST GpMatrix *matrix, GDIPCONST GpMatrix *matrix2, BOOL *result), (matrix, matrix2, result)) \
    m(CreateRegion, (GpRegion **region), (region)) \
    m(CreateRegionRect, (GDIPCONST GpRectF *rect, GpRegion **region), (rect, region)) \
    m(CreateRegionRectI, (GDIPCONST GpRect *rect, GpRegion **region), (rect, region)) \
    m(CreateRegionPath, (GpPath *path, GpRegion **region), (path, region)) \
    m(CreateRegionRgnData, (GDIPCONST BYTE *regionData, INT size, GpRegion **region), (regionData, size, region)) \
    m(CreateRegionHrgn, (HRGN hRgn, GpRegion **region), (hRgn, region)) \
    m(CloneRegion, (GpRegion *region, GpRegion **cloneRegion), (region, cloneRegion)) \
    m(DeleteRegion, (GpRegion *region), (region)) \
    m(SetInfinite, (GpRegion *region), (region)) \
    m(SetEmpty, (GpRegion *region), (region)) \
    m(CombineRegionRect, (GpRegion *region, GDIPCONST GpRectF *rect, CombineMode combineMode), (region, rect, combineMode)) \
    m(CombineRegionRectI, (GpRegion *region, GDIPCONST GpRect *rect, CombineMode combineMode), (region, rect, combineMode)) \
    m(CombineRegionPath, (GpRegion *region, GpPath *path, CombineMode combineMode), (region, path, combineMode)) \
    m(CombineRegionRegion, (GpRegion *region, GpRegion *region2, CombineMode combineMode), (region, region2, combineMode)) \
    m(TranslateRegion, (GpRegion *region, REAL dx, REAL dy), (region, dx, dy)) \
    m(TranslateRegionI, (GpRegion *region, INT dx, INT dy), (region, dx, dy)) \
    m(TransformRegion, (GpRegion *region, GpMatrix *matrix), (region, matrix)) \
    m(GetRegionBounds, (GpRegion *region, GpGraphics *graphics, GpRectF *rect), (region, graphics, rect)) \
    m(GetRegionBoundsI, (GpRegion *region, GpGraphics *graphics, GpRect *rect), (region, graphics, rect)) \
    m(GetRegionHRgn, (GpRegion *region, GpGraphics *graphics, HRGN *hRgn), (region, graphics, hRgn)) \
    m(IsEmptyRegion, (GpRegion *region, GpGraphics *graphics, BOOL *result), (region, graphics, result)) \
    m(IsInfiniteRegion, (GpRegion *region, GpGraphics *graphics, BOOL *result), (region, graphics, result)) \
    m(IsEqualRegion, (GpRegion *region, GpRegion *region2, GpGraphics *graphics, BOOL *result), (region, region2, graphics, result)) \
    m(GetRegionDataSize, (GpRegion *region, UINT *bufferSize), (region, bufferSize)) \
    m(GetRegionData, (GpRegion *region, BYTE *buffer, UINT bufferSize, UINT *sizeFilled), (region, buffer, bufferSize, sizeFilled)) \
    m(IsVisibleRegionPoint, (GpRegion *region, REAL x, REAL y, GpGraphics *graphics, BOOL *result), (region, x, y, graphics, result)) \
    m(IsVisibleRegionPointI, (GpRegion *region, INT x, INT y, GpGraphics *graphics, BOOL *result), (region, x, y, graphics, result)) \
    m(IsVisibleRegionRect, (GpRegion *region, REAL x, REAL y, REAL width, REAL height, GpGraphics *graphics, BOOL *result), (region, x, y, width, height, graphics, result)) \
    m(IsVisibleRegionRectI, (GpRegion *region, INT x, INT y, INT width, INT height, GpGraphics *graphics, BOOL *result), (region, x, y, width, height, graphics, result)) \
    m(GetRegionScansCount, (GpRegion *region, UINT* count, GpMatrix* matrix), (region, count, matrix)) \
    m(GetRegionScans, (GpRegion *region, GpRectF* rects, INT* count, GpMatrix* matrix), (region, rects, count, matrix)) \
    m(GetRegionScansI, (GpRegion *region, GpRect* rects, INT* count, GpMatrix* matrix), (region, rects, count, matrix)) \
    m(CloneBrush, (GpBrush *brush, GpBrush **cloneBrush), (brush, cloneBrush)) \
    m(DeleteBrush, (GpBrush *brush), (brush)) \
    m(GetBrushType, (GpBrush *brush, GpBrushType *type), (brush, type)) \
    m(CreateHatchBrush, (GpHatchStyle hatchstyle, ARGB forecol, ARGB backcol, GpHatch **brush), (hatchstyle, forecol, backcol, brush)) \
    m(GetHatchStyle, (GpHatch *brush, GpHatchStyle *hatchstyle), (brush, hatchstyle)) \
    m(GetHatchForegroundColor, (GpHatch *brush, ARGB* forecol), (brush, forecol)) \
    m(GetHatchBackgroundColor, (GpHatch *brush, ARGB* backcol), (brush, backcol)) \
    m(CreateTexture, (GpImage *image, GpWrapMode wrapmode, GpTexture **texture), (image, wrapmode, texture)) \
    m(CreateTexture2, (GpImage *image, GpWrapMode wrapmode, REAL x, REAL y, REAL width, REAL height, GpTexture **texture), (image, wrapmode, x, y, width, height, texture)) \
    m(CreateTextureIA, (GpImage *image, GDIPCONST GpImageAttributes *imageAttributes, REAL x, REAL y, REAL width, REAL height, GpTexture **texture), (image, imageAttributes, x, y, width, height, texture)) \
    m(CreateTexture2I, (GpImage *image, GpWrapMode wrapmode, INT x, INT y, INT width, INT height, GpTexture **texture), (image, wrapmode, x, y, width, height, texture)) \
    m(CreateTextureIAI, (GpImage *image, GDIPCONST GpImageAttributes *imageAttributes, INT x, INT y, INT width, INT height, GpTexture **texture), (image, imageAttributes, x, y, width, height, texture)) \
    m(GetTextureTransform, (GpTexture *brush, GpMatrix *matrix), (brush, matrix)) \
    m(SetTextureTransform, (GpTexture *brush, GDIPCONST GpMatrix *matrix), (brush, matrix)) \
    m(ResetTextureTransform, (GpTexture* brush), (brush)) \
    m(MultiplyTextureTransform, (GpTexture* brush, GDIPCONST GpMatrix *matrix, GpMatrixOrder order), (brush, matrix, order)) \
    m(TranslateTextureTransform, (GpTexture* brush, REAL dx, REAL dy, GpMatrixOrder order), (brush, dx, dy, order)) \
    m(ScaleTextureTransform, (GpTexture* brush, REAL sx, REAL sy, GpMatrixOrder order), (brush, sx, sy, order)) \
    m(RotateTextureTransform, (GpTexture* brush, REAL angle, GpMatrixOrder order), (brush, angle, order)) \
    m(SetTextureWrapMode, (GpTexture *brush, GpWrapMode wrapmode), (brush, wrapmode)) \
    m(GetTextureWrapMode, (GpTexture *brush, GpWrapMode *wrapmode), (brush, wrapmode)) \
    m(GetTextureImage, (GpTexture *brush, GpImage **image), (brush, image)) \
    m(CreateSolidFill, (ARGB color, GpSolidFill **brush), (color, brush)) \
    m(SetSolidFillColor, (GpSolidFill *brush, ARGB color), (brush, color)) \
    m(GetSolidFillColor, (GpSolidFill *brush, ARGB *color), (brush, color)) \
    m(CreateLineBrush, (GDIPCONST GpPointF* point1, GDIPCONST GpPointF* point2, ARGB color1, ARGB color2, GpWrapMode wrapMode, GpLineGradient **lineGradient), (point1, point2, color1, color2, wrapMode, lineGradient)) \
    m(CreateLineBrushI, (GDIPCONST GpPoint* point1, GDIPCONST GpPoint* point2, ARGB color1, ARGB color2, GpWrapMode wrapMode, GpLineGradient **lineGradient), (point1, point2, color1, color2, wrapMode, lineGradient)) \
    m(CreateLineBrushFromRect, (GDIPCONST GpRectF* rect, ARGB color1, ARGB color2, LinearGradientMode mode, GpWrapMode wrapMode, GpLineGradient **lineGradient), (rect, color1, color2, mode, wrapMode, lineGradient)) \
    m(CreateLineBrushFromRectI, (GDIPCONST GpRect* rect, ARGB color1, ARGB color2, LinearGradientMode mode, GpWrapMode wrapMode, GpLineGradient **lineGradient), (rect, color1, color2, mode, wrapMode, lineGradient)) \
    m(CreateLineBrushFromRectWithAngle, (GDIPCONST GpRectF* rect, ARGB color1, ARGB color2, REAL angle, BOOL isAngleScalable, GpWrapMode wrapMode, GpLineGradient **lineGradient), (rect, color1, color2, angle, isAngleScalable, wrapMode, lineGradient)) \
    m(CreateLineBrushFromRectWithAngleI, (GDIPCONST GpRect* rect, ARGB color1, ARGB color2, REAL angle, BOOL isAngleScalable, GpWrapMode wrapMode, GpLineGradient **lineGradient), (rect, color1, color2, angle, isAngleScalable, wrapMode, lineGradient)) \
    m(SetLineColors, (GpLineGradient *brush, ARGB color1, ARGB color2), (brush, color1, color2)) \
    m(GetLineColors, (GpLineGradient *brush, ARGB* colors), (brush, colors)) \
    m(GetLineRect, (GpLineGradient *brush, GpRectF *rect), (brush, rect)) \
    m(GetLineRectI, (GpLineGradient *brush, GpRect *rect), (brush, rect)) \
    m(SetLineGammaCorrection, (GpLineGradient *brush, BOOL useGammaCorrection), (brush, useGammaCorrection)) \
    m(GetLineGammaCorrection, (GpLineGradient *brush, BOOL *useGammaCorrection), (brush, useGammaCorrection)) \
    m(GetLineBlendCount, (GpLineGradient *brush, INT *count), (brush, count)) \
    m(GetLineBlend, (GpLineGradient *brush, REAL *blend, REAL* positions, INT count), (brush, blend, positions, count)) \
    m(SetLineBlend, (GpLineGradient *brush, GDIPCONST REAL *blend, GDIPCONST REAL* positions, INT count), (brush, blend, positions, count)) \
    m(GetLinePresetBlendCount, (GpLineGradient *brush, INT *count), (brush, count)) \
    m(GetLinePresetBlend, (GpLineGradient *brush, ARGB *blend, REAL* positions, INT count), (brush, blend, positions, count)) \
    m(SetLinePresetBlend, (GpLineGradient *brush, GDIPCONST ARGB *blend, GDIPCONST REAL* positions, INT count), (brush, blend, positions, count)) \
    m(SetLineSigmaBlend, (GpLineGradient *brush, REAL focus, REAL scale), (brush, focus, scale)) \
    m(SetLineLinearBlend, (GpLineGradient *brush, REAL focus, REAL scale), (brush, focus, scale)) \
    m(SetLineWrapMode, (GpLineGradient *brush, GpWrapMode wrapmode), (brush, wrapmode)) \
    m(GetLineWrapMode, (GpLineGradient *brush, GpWrapMode *wrapmode), (brush, wrapmode)) \
    m(GetLineTransform, (GpLineGradient *brush, GpMatrix *matrix), (brush, matrix)) \
    m(SetLineTransform, (GpLineGradient *brush, GDIPCONST GpMatrix *matrix), (brush, matrix)) \
    m(ResetLineTransform, (GpLineGradient* brush), (brush)) \
    m(MultiplyLineTransform, (GpLineGradient* brush, GDIPCONST GpMatrix *matrix, GpMatrixOrder order), (brush, matrix, order)) \
    m(TranslateLineTransform, (GpLineGradient* brush, REAL dx, REAL dy, GpMatrixOrder order), (brush, dx, dy, order)) \
    m(ScaleLineTransform, (GpLineGradient* brush, REAL sx, REAL sy, GpMatrixOrder order), (brush, sx, sy, order)) \
    m(RotateLineTransform, (GpLineGradient* brush, REAL angle, GpMatrixOrder order), (brush, angle, order)) \
    m(CreatePathGradient, (GDIPCONST GpPointF* points, INT count, GpWrapMode wrapMode, GpPathGradient **polyGradient), (points, count, wrapMode, polyGradient)) \
    m(CreatePathGradientI, (GDIPCONST GpPoint* points, INT count, GpWrapMode wrapMode, GpPathGradient **polyGradient), (points, count, wrapMode, polyGradient)) \
    m(CreatePathGradientFromPath, (GDIPCONST GpPath* path, GpPathGradient **polyGradient), (path, polyGradient)) \
    m(GetPathGradientCenterColor, (GpPathGradient *brush, ARGB* colors), (brush, colors)) \
    m(SetPathGradientCenterColor, (GpPathGradient *brush, ARGB colors), (brush, colors)) \
    m(GetPathGradientSurroundColorsWithCount, (GpPathGradient *brush, ARGB* color, INT* count), (brush, color, count)) \
    m(SetPathGradientSurroundColorsWithCount, (GpPathGradient *brush, GDIPCONST ARGB* color, INT* count), (brush, color, count)) \
    m(GetPathGradientPath, (GpPathGradient *brush, GpPath *path), (brush, path)) \
    m(SetPathGradientPath, (GpPathGradient *brush, GDIPCONST GpPath *path), (brush, path)) \
    m(GetPathGradientCenterPoint, (GpPathGradient *brush, GpPointF* points), (brush, points)) \
    m(GetPathGradientCenterPointI, (GpPathGradient *brush, GpPoint* points), (brush, points)) \
    m(SetPathGradientCenterPoint, (GpPathGradient *brush, GDIPCONST GpPointF* points), (brush, points)) \
    m(SetPathGradientCenterPointI, (GpPathGradient *brush, GDIPCONST GpPoint* points), (brush, points)) \
    m(GetPathGradientRect, (GpPathGradient *brush, GpRectF *rect), (brush, rect)) \
    m(GetPathGradientRectI, (GpPathGradient *brush, GpRect *rect), (brush, rect)) \
    m(GetPathGradientPointCount, (GpPathGradient *brush, INT* count), (brush, count)) \
    m(GetPathGradientSurroundColorCount, (GpPathGradient *brush, INT* count), (brush, count)) \
    m(SetPathGradientGammaCorrection, (GpPathGradient *brush, BOOL useGammaCorrection), (brush, useGammaCorrection)) \
    m(GetPathGradientGammaCorrection, (GpPathGradient *brush, BOOL *useGammaCorrection), (brush, useGammaCorrection)) \
    m(GetPathGradientBlendCount, (GpPathGradient *brush, INT *count), (brush, count)) \
    m(GetPathGradientBlend, (GpPathGradient *brush, REAL *blend, REAL *positions, INT count), (brush, blend, positions, count)) \
    m(SetPathGradientBlend, (GpPathGradient *brush, GDIPCONST REAL *blend, GDIPCONST REAL *positions, INT count), (brush, blend, positions, count)) \
    m(GetPathGradientPresetBlendCount, (GpPathGradient *brush, INT *count), (brush, count)) \
    m(GetPathGradientPresetBlend, (GpPathGradient *brush, ARGB *blend, REAL* positions, INT count), (brush, blend, positions, count)) \
    m(SetPathGradientPresetBlend, (GpPathGradient *brush, GDIPCONST ARGB *blend, GDIPCONST REAL* positions, INT count), (brush, blend, positions, count)) \
    m(SetPathGradientSigmaBlend, (GpPathGradient *brush, REAL focus, REAL scale), (brush, focus, scale)) \
    m(SetPathGradientLinearBlend, (GpPathGradient *brush, REAL focus, REAL scale), (brush, focus, scale)) \
    m(GetPathGradientWrapMode, (GpPathGradient *brush, GpWrapMode *wrapmode), (brush, wrapmode)) \
    m(SetPathGradientWrapMode, (GpPathGradient *brush, GpWrapMode wrapmode), (brush, wrapmode)) \
    m(GetPathGradientTransform, (GpPathGradient *brush, GpMatrix *matrix), (brush, matrix)) \
    m(SetPathGradientTransform, (GpPathGradient *brush, GpMatrix *matrix), (brush, matrix)) \
    m(ResetPathGradientTransform, (GpPathGradient* brush), (brush)) \
    m(MultiplyPathGradientTransform, (GpPathGradient* brush, GDIPCONST GpMatrix *matrix, GpMatrixOrder order), (brush, matrix, order)) \
    m(TranslatePathGradientTransform, (GpPathGradient* brush, REAL dx, REAL dy, GpMatrixOrder order), (brush, dx, dy, order)) \
    m(ScalePathGradientTransform, (GpPathGradient* brush, REAL sx, REAL sy, GpMatrixOrder order), (brush, sx, sy, order)) \
    m(RotatePathGradientTransform, (GpPathGradient* brush, REAL angle, GpMatrixOrder order), (brush, angle, order)) \
    m(GetPathGradientFocusScales, (GpPathGradient *brush, REAL* xScale, REAL* yScale), (brush, xScale, yScale)) \
    m(SetPathGradientFocusScales, (GpPathGradient *brush, REAL xScale, REAL yScale), (brush, xScale, yScale)) \
    m(CreatePen1, (ARGB color, REAL width, GpUnit unit, GpPen **pen), (color, width, unit, pen)) \
    m(CreatePen2, (GpBrush *brush, REAL width, GpUnit unit, GpPen **pen), (brush, width, unit, pen)) \
    m(ClonePen, (GpPen *pen, GpPen **clonepen), (pen, clonepen)) \
    m(DeletePen, (GpPen *pen), (pen)) \
    m(SetPenWidth, (GpPen *pen, REAL width), (pen, width)) \
    m(GetPenWidth, (GpPen *pen, REAL *width), (pen, width)) \
    m(SetPenUnit, (GpPen *pen, GpUnit unit), (pen, unit)) \
    m(GetPenUnit, (GpPen *pen, GpUnit *unit), (pen, unit)) \
    m(SetPenLineCap197819, (GpPen *pen, GpLineCap startCap, GpLineCap endCap, GpDashCap dashCap), (pen, startCap, endCap, dashCap)) \
    m(SetPenStartCap, (GpPen *pen, GpLineCap startCap), (pen, startCap)) \
    m(SetPenEndCap, (GpPen *pen, GpLineCap endCap), (pen, endCap)) \
    m(SetPenDashCap197819, (GpPen *pen, GpDashCap dashCap), (pen, dashCap)) \
    m(GetPenStartCap, (GpPen *pen, GpLineCap *startCap), (pen, startCap)) \
    m(GetPenEndCap, (GpPen *pen, GpLineCap *endCap), (pen, endCap)) \
    m(GetPenDashCap197819, (GpPen *pen, GpDashCap *dashCap), (pen, dashCap)) \
    m(SetPenLineJoin, (GpPen *pen, GpLineJoin lineJoin), (pen, lineJoin)) \
    m(GetPenLineJoin, (GpPen *pen, GpLineJoin *lineJoin), (pen, lineJoin)) \
    m(SetPenCustomStartCap, (GpPen *pen, GpCustomLineCap* customCap), (pen, customCap)) \
    m(GetPenCustomStartCap, (GpPen *pen, GpCustomLineCap** customCap), (pen, customCap)) \
    m(SetPenCustomEndCap, (GpPen *pen, GpCustomLineCap* customCap), (pen, customCap)) \
    m(GetPenCustomEndCap, (GpPen *pen, GpCustomLineCap** customCap), (pen, customCap)) \
    m(SetPenMiterLimit, (GpPen *pen, REAL miterLimit), (pen, miterLimit)) \
    m(GetPenMiterLimit, (GpPen *pen, REAL *miterLimit), (pen, miterLimit)) \
    m(SetPenMode, (GpPen *pen, GpPenAlignment penMode), (pen, penMode)) \
    m(GetPenMode, (GpPen *pen, GpPenAlignment *penMode), (pen, penMode)) \
    m(SetPenTransform, (GpPen *pen, GpMatrix *matrix), (pen, matrix)) \
    m(GetPenTransform, (GpPen *pen, GpMatrix *matrix), (pen, matrix)) \
    m(ResetPenTransform, (GpPen *pen), (pen)) \
    m(MultiplyPenTransform, (GpPen *pen, GDIPCONST GpMatrix *matrix, GpMatrixOrder order), (pen, matrix, order)) \
    m(TranslatePenTransform, (GpPen *pen, REAL dx, REAL dy, GpMatrixOrder order), (pen, dx, dy, order)) \
    m(ScalePenTransform, (GpPen *pen, REAL sx, REAL sy, GpMatrixOrder order), (pen, sx, sy, order)) \
    m(RotatePenTransform, (GpPen *pen, REAL angle, GpMatrixOrder order), (pen, angle, order)) \
    m(SetPenColor, (GpPen *pen, ARGB argb), (pen, argb)) \
    m(GetPenColor, (GpPen *pen, ARGB *argb), (pen, argb)) \
    m(SetPenBrushFill, (GpPen *pen, GpBrush *brush), (pen, brush)) \
    m(GetPenBrushFill, (GpPen *pen, GpBrush **brush), (pen, brush)) \
    m(GetPenFillType, (GpPen *pen, GpPenType* type), (pen, type)) \
    m(GetPenDashStyle, (GpPen *pen, GpDashStyle *dashstyle), (pen, dashstyle)) \
    m(SetPenDashStyle, (GpPen *pen, GpDashStyle dashstyle), (pen, dashstyle)) \
    m(GetPenDashOffset, (GpPen *pen, REAL *offset), (pen, offset)) \
    m(SetPenDashOffset, (GpPen *pen, REAL offset), (pen, offset)) \
    m(GetPenDashCount, (GpPen *pen, INT *count), (pen, count)) \
    m(SetPenDashArray, (GpPen *pen, GDIPCONST REAL *dash, INT count), (pen, dash, count)) \
    m(GetPenDashArray, (GpPen *pen, REAL *dash, INT count), (pen, dash, count)) \
    m(GetPenCompoundCount, (GpPen *pen, INT *count), (pen, count)) \
    m(SetPenCompoundArray, (GpPen *pen, GDIPCONST REAL *dash, INT count), (pen, dash, count)) \
    m(GetPenCompoundArray, (GpPen *pen, REAL *dash, INT count), (pen, dash, count)) \
    m(CreateCustomLineCap, (GpPath* fillPath, GpPath* strokePath, GpLineCap baseCap, REAL baseInset, GpCustomLineCap **customCap), (fillPath, strokePath, baseCap, baseInset, customCap)) \
    m(DeleteCustomLineCap, (GpCustomLineCap* customCap), (customCap)) \
    m(CloneCustomLineCap, (GpCustomLineCap* customCap, GpCustomLineCap** clonedCap), (customCap, clonedCap)) \
    m(GetCustomLineCapType, (GpCustomLineCap* customCap, CustomLineCapType* capType), (customCap, capType)) \
    m(SetCustomLineCapStrokeCaps, (GpCustomLineCap* customCap, GpLineCap startCap, GpLineCap endCap), (customCap, startCap, endCap)) \
    m(GetCustomLineCapStrokeCaps, (GpCustomLineCap* customCap, GpLineCap* startCap, GpLineCap* endCap), (customCap, startCap, endCap)) \
    m(SetCustomLineCapStrokeJoin, (GpCustomLineCap* customCap, GpLineJoin lineJoin), (customCap, lineJoin)) \
    m(GetCustomLineCapStrokeJoin, (GpCustomLineCap* customCap, GpLineJoin* lineJoin), (customCap, lineJoin)) \
    m(SetCustomLineCapBaseCap, (GpCustomLineCap* customCap, GpLineCap baseCap), (customCap, baseCap)) \
    m(GetCustomLineCapBaseCap, (GpCustomLineCap* customCap, GpLineCap* baseCap), (customCap, baseCap)) \
    m(SetCustomLineCapBaseInset, (GpCustomLineCap* customCap, REAL inset), (customCap, inset)) \
    m(GetCustomLineCapBaseInset, (GpCustomLineCap* customCap, REAL* inset), (customCap, inset)) \
    m(SetCustomLineCapWidthScale, (GpCustomLineCap* customCap, REAL widthScale), (customCap, widthScale)) \
    m(GetCustomLineCapWidthScale, (GpCustomLineCap* customCap, REAL* widthScale), (customCap, widthScale)) \
    m(CreateAdjustableArrowCap, (REAL height, REAL width, BOOL isFilled, GpAdjustableArrowCap **cap), (height, width, isFilled, cap)) \
    m(SetAdjustableArrowCapHeight, (GpAdjustableArrowCap* cap, REAL height), (cap, height)) \
    m(GetAdjustableArrowCapHeight, (GpAdjustableArrowCap* cap, REAL* height), (cap, height)) \
    m(SetAdjustableArrowCapWidth, (GpAdjustableArrowCap* cap, REAL width), (cap, width)) \
    m(GetAdjustableArrowCapWidth, (GpAdjustableArrowCap* cap, REAL* width), (cap, width)) \
    m(SetAdjustableArrowCapMiddleInset, (GpAdjustableArrowCap* cap, REAL middleInset), (cap, middleInset)) \
    m(GetAdjustableArrowCapMiddleInset, (GpAdjustableArrowCap* cap, REAL* middleInset), (cap, middleInset)) \
    m(SetAdjustableArrowCapFillState, (GpAdjustableArrowCap* cap, BOOL fillState), (cap, fillState)) \
    m(GetAdjustableArrowCapFillState, (GpAdjustableArrowCap* cap, BOOL* fillState), (cap, fillState)) \
    m(LoadImageFromStream, (IStream* stream, GpImage **image), (stream, image)) \
    m(LoadImageFromFile, (GDIPCONST WCHAR* filename, GpImage **image), (filename, image)) \
    m(LoadImageFromStreamICM, (IStream* stream, GpImage **image), (stream, image)) \
    m(LoadImageFromFileICM, (GDIPCONST WCHAR* filename, GpImage **image), (filename, image)) \
    m(CloneImage, (GpImage *image, GpImage **cloneImage), (image, cloneImage)) \
    m(DisposeImage, (GpImage *image), (image)) \
    m(SaveImageToFile, (GpImage *image, GDIPCONST WCHAR* filename, GDIPCONST CLSID* clsidEncoder, GDIPCONST EncoderParameters* encoderParams), (image, filename, clsidEncoder, encoderParams)) \
    m(SaveImageToStream, (GpImage *image, IStream* stream, GDIPCONST CLSID* clsidEncoder, GDIPCONST EncoderParameters* encoderParams), (image, stream, clsidEncoder, encoderParams)) \
    m(SaveAdd, (GpImage *image, GDIPCONST EncoderParameters* encoderParams), (image, encoderParams)) \
    m(SaveAddImage, (GpImage *image, GpImage* newImage, GDIPCONST EncoderParameters* encoderParams), (image, newImage, encoderParams)) \
    m(GetImageGraphicsContext, (GpImage *image, GpGraphics **graphics), (image, graphics)) \
    m(GetImageBounds, (GpImage *image, GpRectF *srcRect, GpUnit *srcUnit), (image, srcRect, srcUnit)) \
    m(GetImageDimension, (GpImage *image, REAL *width, REAL *height), (image, width, height)) \
    m(GetImageType, (GpImage *image, ImageType *type), (image, type)) \
    m(GetImageWidth, (GpImage *image, UINT *width), (image, width)) \
    m(GetImageHeight, (GpImage *image, UINT *height), (image, height)) \
    m(GetImageHorizontalResolution, (GpImage *image, REAL *resolution), (image, resolution)) \
    m(GetImageVerticalResolution, (GpImage *image, REAL *resolution), (image, resolution)) \
    m(GetImageFlags, (GpImage *image, UINT *flags), (image, flags)) \
    m(GetImageRawFormat, (GpImage *image, GUID *format), (image, format)) \
    m(GetImagePixelFormat, (GpImage *image, PixelFormat *format), (image, format)) \
    m(GetImageThumbnail, (GpImage *image, UINT thumbWidth, UINT thumbHeight, GpImage **thumbImage, GetThumbnailImageAbort callback, VOID *callbackData), (image, thumbWidth, thumbHeight, thumbImage, callback, callbackData)) \
    m(GetEncoderParameterListSize, (GpImage *image, GDIPCONST CLSID* clsidEncoder, UINT* size), (image, clsidEncoder, size)) \
    m(GetEncoderParameterList, (GpImage *image, GDIPCONST CLSID* clsidEncoder, UINT size, EncoderParameters* buffer), (image, clsidEncoder, size, buffer)) \
    m(ImageGetFrameDimensionsCount, (GpImage* image, UINT* count), (image, count)) \
    m(ImageGetFrameDimensionsList, (GpImage* image, GUID* dimensionIDs, UINT count), (image, dimensionIDs, count)) \
    m(ImageGetFrameCount, (GpImage *image, GDIPCONST GUID* dimensionID, UINT* count), (image, dimensionID, count)) \
    m(ImageSelectActiveFrame, (GpImage *image, GDIPCONST GUID* dimensionID, UINT frameIndex), (image, dimensionID, frameIndex)) \
    m(ImageRotateFlip, (GpImage *image, RotateFlipType rfType), (image, rfType)) \
    m(GetImagePalette, (GpImage *image, ColorPalette *palette, INT size), (image, palette, size)) \
    m(SetImagePalette, (GpImage *image, GDIPCONST ColorPalette *palette), (image, palette)) \
    m(GetImagePaletteSize, (GpImage *image, INT *size), (image, size)) \
    m(GetPropertyCount, (GpImage *image, UINT* numOfProperty), (image, numOfProperty)) \
    m(GetPropertyIdList, (GpImage *image, UINT numOfProperty, PROPID* list), (image, numOfProperty, list)) \
    m(GetPropertyItemSize, (GpImage *image, PROPID propId, UINT* size), (image, propId, size)) \
    m(GetPropertyItem, (GpImage *image, PROPID propId,UINT propSize, PropertyItem* buffer), (image, propId, propSize, buffer)) \
    m(GetPropertySize, (GpImage *image, UINT* totalBufferSize, UINT* numProperties), (image, totalBufferSize, numProperties)) \
    m(GetAllPropertyItems, (GpImage *image, UINT totalBufferSize, UINT numProperties, PropertyItem* allItems), (image, totalBufferSize, numProperties, allItems)) \
    m(RemovePropertyItem, (GpImage *image, PROPID propId), (image, propId)) \
    m(SetPropertyItem, (GpImage *image, GDIPCONST PropertyItem* item), (image, item)) \
    m(ImageForceValidation, (GpImage *image), (image)) \
    m(CreateBitmapFromStream, (IStream* stream, GpBitmap **bitmap), (stream, bitmap)) \
    m(CreateBitmapFromFile, (GDIPCONST WCHAR* filename, GpBitmap **bitmap), (filename, bitmap)) \
    m(CreateBitmapFromStreamICM, (IStream* stream, GpBitmap **bitmap), (stream, bitmap)) \
    m(CreateBitmapFromFileICM, (GDIPCONST WCHAR* filename, GpBitmap **bitmap), (filename, bitmap)) \
    m(CreateBitmapFromScan0, (INT width, INT height, INT stride, PixelFormat format, BYTE* scan0, GpBitmap** bitmap), (width, height, stride, format, scan0, bitmap)) \
    m(CreateBitmapFromGraphics, (INT width, INT height, GpGraphics* target, GpBitmap** bitmap), (width, height, target, bitmap)) \
    m(CreateBitmapFromDirectDrawSurface, (IDirectDrawSurface7* surface, GpBitmap** bitmap), (surface, bitmap)) \
    m(CreateBitmapFromGdiDib, (GDIPCONST BITMAPINFO* gdiBitmapInfo, VOID* gdiBitmapData, GpBitmap** bitmap), (gdiBitmapInfo, gdiBitmapData, bitmap)) \
    m(CreateBitmapFromHBITMAP, (HBITMAP hbm, HPALETTE hpal, GpBitmap** bitmap), (hbm, hpal, bitmap)) \
    m(CreateHBITMAPFromBitmap, (GpBitmap* bitmap, HBITMAP* hbmReturn, ARGB background), (bitmap, hbmReturn, background)) \
    m(CreateBitmapFromHICON, (HICON hicon, GpBitmap** bitmap), (hicon, bitmap)) \
    m(CreateHICONFromBitmap, (GpBitmap* bitmap, HICON* hbmReturn), (bitmap, hbmReturn)) \
    m(CreateBitmapFromResource, (HINSTANCE hInstance, GDIPCONST WCHAR* lpBitmapName, GpBitmap** bitmap), (hInstance, lpBitmapName, bitmap)) \
    m(CloneBitmapArea, (REAL x, REAL y, REAL width, REAL height, PixelFormat format, GpBitmap *srcBitmap, GpBitmap **dstBitmap), (x, y, width, height, format, srcBitmap, dstBitmap)) \
    m(CloneBitmapAreaI, (INT x, INT y, INT width, INT height, PixelFormat format, GpBitmap *srcBitmap, GpBitmap **dstBitmap), (x, y, width, height, format, srcBitmap, dstBitmap)) \
    m(BitmapLockBits, (GpBitmap* bitmap, GDIPCONST GpRect* rect, UINT flags, PixelFormat format, BitmapData* lockedBitmapData), (bitmap, rect, flags, format, lockedBitmapData)) \
    m(BitmapUnlockBits, (GpBitmap* bitmap, BitmapData* lockedBitmapData), (bitmap, lockedBitmapData)) \
    m(BitmapGetPixel, (GpBitmap* bitmap, INT x, INT y, ARGB *color), (bitmap, x, y, color)) \
    m(BitmapSetPixel, (GpBitmap* bitmap, INT x, INT y, ARGB color), (bitmap, x, y, color)) \
    m(BitmapSetResolution, (GpBitmap* bitmap, REAL xdpi, REAL ydpi), (bitmap, xdpi, ydpi)) \
    m(CreateImageAttributes, (GpImageAttributes **imageattr), (imageattr)) \
    m(CloneImageAttributes, (GDIPCONST GpImageAttributes *imageattr, GpImageAttributes **cloneImageattr), (imageattr, cloneImageattr)) \
    m(DisposeImageAttributes, (GpImageAttributes *imageattr), (imageattr)) \
    m(SetImageAttributesToIdentity, (GpImageAttributes *imageattr, ColorAdjustType type), (imageattr, type)) \
    m(ResetImageAttributes, (GpImageAttributes *imageattr, ColorAdjustType type), (imageattr, type)) \
    m(SetImageAttributesColorMatrix, (GpImageAttributes *imageattr, ColorAdjustType type, BOOL enableFlag, GDIPCONST ColorMatrix* colorMatrix, GDIPCONST ColorMatrix* grayMatrix, ColorMatrixFlags flags), (imageattr, type, enableFlag, colorMatrix, grayMatrix, flags)) \
    m(SetImageAttributesThreshold, (GpImageAttributes *imageattr, ColorAdjustType type, BOOL enableFlag, REAL threshold), (imageattr, type, enableFlag, threshold)) \
    m(SetImageAttributesGamma, (GpImageAttributes *imageattr, ColorAdjustType type, BOOL enableFlag, REAL gamma), (imageattr, type, enableFlag, gamma)) \
    m(SetImageAttributesNoOp, (GpImageAttributes *imageattr, ColorAdjustType type, BOOL enableFlag), (imageattr, type, enableFlag)) \
    m(SetImageAttributesColorKeys, (GpImageAttributes *imageattr, ColorAdjustType type, BOOL enableFlag, ARGB colorLow, ARGB colorHigh), (imageattr, type, enableFlag, colorLow, colorHigh)) \
    m(SetImageAttributesOutputChannel, (GpImageAttributes *imageattr, ColorAdjustType type, BOOL enableFlag, ColorChannelFlags channelFlags), (imageattr, type, enableFlag, channelFlags)) \
    m(SetImageAttributesOutputChannelColorProfile, (GpImageAttributes *imageattr, ColorAdjustType type, BOOL enableFlag, GDIPCONST WCHAR *colorProfileFilename), (imageattr, type, enableFlag, colorProfileFilename)) \
    m(SetImageAttributesRemapTable, (GpImageAttributes *imageattr, ColorAdjustType type, BOOL enableFlag, UINT mapSize, GDIPCONST ColorMap *map), (imageattr, type, enableFlag, mapSize, map)) \
    m(SetImageAttributesWrapMode, (GpImageAttributes *imageAttr, WrapMode wrap, ARGB argb, BOOL clamp), (imageAttr, wrap, argb, clamp)) \
    m(GetImageAttributesAdjustedPalette, (GpImageAttributes *imageAttr, ColorPalette *colorPalette, ColorAdjustType colorAdjustType), (imageAttr, colorPalette, colorAdjustType)) \
    m(Flush, (GpGraphics *graphics, GpFlushIntention intention), (graphics, intention)) \
    m(CreateFromHDC, (HDC hdc, GpGraphics **graphics), (hdc, graphics)) \
    m(CreateFromHDC2, (HDC hdc, HANDLE hDevice, GpGraphics **graphics), (hdc, hDevice, graphics)) \
    m(CreateFromHWND, (HWND hwnd, GpGraphics **graphics), (hwnd, graphics)) \
    m(CreateFromHWNDICM, (HWND hwnd, GpGraphics **graphics), (hwnd, graphics)) \
    m(DeleteGraphics, (GpGraphics *graphics), (graphics)) \
    m(GetDC, (GpGraphics* graphics, HDC *hdc), (graphics, hdc)) \
    m(ReleaseDC, (GpGraphics* graphics, HDC hdc), (graphics, hdc)) \
    m(SetCompositingMode, (GpGraphics *graphics, CompositingMode compositingMode), (graphics, compositingMode)) \
    m(GetCompositingMode, (GpGraphics *graphics, CompositingMode *compositingMode), (graphics, compositingMode)) \
    m(SetRenderingOrigin, (GpGraphics *graphics, INT x, INT y), (graphics, x, y)) \
    m(GetRenderingOrigin, (GpGraphics *graphics, INT *x, INT *y), (graphics, x, y)) \
    m(SetCompositingQuality, (GpGraphics *graphics, CompositingQuality compositingQuality), (graphics, compositingQuality)) \
    m(GetCompositingQuality, (GpGraphics *graphics, CompositingQuality *compositingQuality), (graphics, compositingQuality)) \
    m(SetSmoothingMode, (GpGraphics *graphics, SmoothingMode smoothingMode), (graphics, smoothingMode)) \
    m(GetSmoothingMode, (GpGraphics *graphics, SmoothingMode *smoothingMode), (graphics, smoothingMode)) \
    m(SetPixelOffsetMode, (GpGraphics* graphics, PixelOffsetMode pixelOffsetMode), (graphics, pixelOffsetMode)) \
    m(GetPixelOffsetMode, (GpGraphics *graphics, PixelOffsetMode *pixelOffsetMode), (graphics, pixelOffsetMode)) \
    m(SetTextRenderingHint, (GpGraphics *graphics, TextRenderingHint mode), (graphics, mode)) \
    m(GetTextRenderingHint, (GpGraphics *graphics, TextRenderingHint *mode), (graphics, mode)) \
    m(SetTextContrast, (GpGraphics *graphics, UINT contrast), (graphics, contrast)) \
    m(GetTextContrast, (GpGraphics *graphics, UINT *contrast), (graphics, contrast)) \
    m(SetInterpolationMode, (GpGraphics *graphics, InterpolationMode interpolationMode), (graphics, interpolationMode)) \
    m(GetInterpolationMode, (GpGraphics *graphics, InterpolationMode *interpolationMode), (graphics, interpolationMode)) \
    m(SetWorldTransform, (GpGraphics *graphics, GpMatrix *matrix), (graphics, matrix)) \
    m(ResetWorldTransform, (GpGraphics *graphics), (graphics)) \
    m(MultiplyWorldTransform, (GpGraphics *graphics, GDIPCONST GpMatrix *matrix, GpMatrixOrder order), (graphics, matrix, order)) \
    m(TranslateWorldTransform, (GpGraphics *graphics, REAL dx, REAL dy, GpMatrixOrder order), (graphics, dx, dy, order)) \
    m(ScaleWorldTransform, (GpGraphics *graphics, REAL sx, REAL sy, GpMatrixOrder order), (graphics, sx, sy, order)) \
    m(RotateWorldTransform, (GpGraphics *graphics, REAL angle, GpMatrixOrder order), (graphics, angle, order)) \
    m(GetWorldTransform, (GpGraphics *graphics, GpMatrix *matrix), (graphics, matrix)) \
    m(ResetPageTransform, (GpGraphics *graphics), (graphics)) \
    m(GetPageUnit, (GpGraphics *graphics, GpUnit *unit), (graphics, unit)) \
    m(GetPageScale, (GpGraphics *graphics, REAL *scale), (graphics, scale)) \
    m(SetPageUnit, (GpGraphics *graphics, GpUnit unit), (graphics, unit)) \
    m(SetPageScale, (GpGraphics *graphics, REAL scale), (graphics, scale)) \
    m(GetDpiX, (GpGraphics *graphics, REAL* dpi), (graphics, dpi)) \
    m(GetDpiY, (GpGraphics *graphics, REAL* dpi), (graphics, dpi)) \
    m(TransformPoints, (GpGraphics *graphics, GpCoordinateSpace destSpace, GpCoordinateSpace srcSpace, GpPointF *points, INT count), (graphics, destSpace, srcSpace, points, count)) \
    m(TransformPointsI, (GpGraphics *graphics, GpCoordinateSpace destSpace, GpCoordinateSpace srcSpace, GpPoint *points, INT count), (graphics, destSpace, srcSpace, points, count)) \
    m(GetNearestColor, (GpGraphics *graphics, ARGB* argb), (graphics, argb)) \
    m(DrawLine, (GpGraphics *graphics, GpPen *pen, REAL x1, REAL y1, REAL x2, REAL y2), (graphics, pen, x1, y1, x2, y2)) \
    m(DrawLineI, (GpGraphics *graphics, GpPen *pen, INT x1, INT y1, INT x2, INT y2), (graphics, pen, x1, y1, x2, y2)) \
    m(DrawLines, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPointF *points, INT count), (graphics, pen, points, count)) \
    m(DrawLinesI, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPoint *points, INT count), (graphics, pen, points, count)) \
    m(DrawArc, (GpGraphics *graphics, GpPen *pen, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle), (graphics, pen, x, y, width, height, startAngle, sweepAngle)) \
    m(DrawArcI, (GpGraphics *graphics, GpPen *pen, INT x, INT y, INT width, INT height, REAL startAngle, REAL sweepAngle), (graphics, pen, x, y, width, height, startAngle, sweepAngle)) \
    m(DrawBezier, (GpGraphics *graphics, GpPen *pen, REAL x1, REAL y1, REAL x2, REAL y2, REAL x3, REAL y3, REAL x4, REAL y4), (graphics, pen, x1, y1, x2, y2, x3, y3, x4, y4)) \
    m(DrawBezierI, (GpGraphics *graphics, GpPen *pen, INT x1, INT y1, INT x2, INT y2, INT x3, INT y3, INT x4, INT y4), (graphics, pen, x1, y1, x2, y2, x3, y3, x4, y4)) \
    m(DrawBeziers, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPointF *points, INT count), (graphics, pen, points, count)) \
    m(DrawBeziersI, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPoint *points, INT count), (graphics, pen, points, count)) \
    m(DrawRectangle, (GpGraphics *graphics, GpPen *pen, REAL x, REAL y, REAL width, REAL height), (graphics, pen, x, y, width, height)) \
    m(DrawRectangleI, (GpGraphics *graphics, GpPen *pen, INT x, INT y, INT width, INT height), (graphics, pen, x, y, width, height)) \
    m(DrawRectangles, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpRectF *rects, INT count), (graphics, pen, rects, count)) \
    m(DrawRectanglesI, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpRect *rects, INT count), (graphics, pen, rects, count)) \
    m(DrawEllipse, (GpGraphics *graphics, GpPen *pen, REAL x, REAL y, REAL width, REAL height), (graphics, pen, x, y, width, height)) \
    m(DrawEllipseI, (GpGraphics *graphics, GpPen *pen, INT x, INT y, INT width, INT height), (graphics, pen, x, y, width, height)) \
    m(DrawPie, (GpGraphics *graphics, GpPen *pen, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle), (graphics, pen, x, y, width, height, startAngle, sweepAngle)) \
    m(DrawPieI, (GpGraphics *graphics, GpPen *pen, INT x, INT y, INT width, INT height, REAL startAngle, REAL sweepAngle), (graphics, pen, x, y, width, height, startAngle, sweepAngle)) \
    m(DrawPolygon, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPointF *points, INT count), (graphics, pen, points, count)) \
    m(DrawPolygonI, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPoint *points, INT count), (graphics, pen, points, count)) \
    m(DrawPath, (GpGraphics *graphics, GpPen *pen, GpPath *path), (graphics, pen, path)) \
    m(DrawCurve, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPointF *points, INT count), (graphics, pen, points, count)) \
    m(DrawCurveI, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPoint *points, INT count), (graphics, pen, points, count)) \
    m(DrawCurve2, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPointF *points, INT count, REAL tension), (graphics, pen, points, count, tension)) \
    m(DrawCurve2I, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPoint *points, INT count, REAL tension), (graphics, pen, points, count, tension)) \
    m(DrawCurve3, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPointF *points, INT count, INT offset, INT numberOfSegments, REAL tension), (graphics, pen, points, count, offset, numberOfSegments, tension)) \
    m(DrawCurve3I, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPoint *points, INT count, INT offset, INT numberOfSegments, REAL tension), (graphics, pen, points, count, offset, numberOfSegments, tension)) \
    m(DrawClosedCurve, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPointF *points, INT count), (graphics, pen, points, count)) \
    m(DrawClosedCurveI, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPoint *points, INT count), (graphics, pen, points, count)) \
    m(DrawClosedCurve2, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPointF *points, INT count, REAL tension), (graphics, pen, points, count, tension)) \
    m(DrawClosedCurve2I, (GpGraphics *graphics, GpPen *pen, GDIPCONST GpPoint *points, INT count, REAL tension), (graphics, pen, points, count, tension)) \
    m(GraphicsClear, (GpGraphics *graphics, ARGB color), (graphics, color)) \
    m(FillRectangle, (GpGraphics *graphics, GpBrush *brush, REAL x, REAL y, REAL width, REAL height), (graphics, brush, x, y, width, height)) \
    m(FillRectangleI, (GpGraphics *graphics, GpBrush *brush, INT x, INT y, INT width, INT height), (graphics, brush, x, y, width, height)) \
    m(FillRectangles, (GpGraphics *graphics, GpBrush *brush, GDIPCONST GpRectF *rects, INT count), (graphics, brush, rects, count)) \
    m(FillRectanglesI, (GpGraphics *graphics, GpBrush *brush, GDIPCONST GpRect *rects, INT count), (graphics, brush, rects, count)) \
    m(FillPolygon, (GpGraphics *graphics, GpBrush *brush, GDIPCONST GpPointF *points, INT count, GpFillMode fillMode), (graphics, brush, points, count, fillMode)) \
    m(FillPolygonI, (GpGraphics *graphics, GpBrush *brush, GDIPCONST GpPoint *points, INT count, GpFillMode fillMode), (graphics, brush, points, count, fillMode)) \
    m(FillPolygon2, (GpGraphics *graphics, GpBrush *brush, GDIPCONST GpPointF *points, INT count), (graphics, brush, points, count)) \
    m(FillPolygon2I, (GpGraphics *graphics, GpBrush *brush, GDIPCONST GpPoint *points, INT count), (graphics, brush, points, count)) \
    m(FillEllipse, (GpGraphics *graphics, GpBrush *brush, REAL x, REAL y, REAL width, REAL height), (graphics, brush, x, y, width, height)) \
    m(FillEllipseI, (GpGraphics *graphics, GpBrush *brush, INT x, INT y, INT width, INT height), (graphics, brush, x, y, width, height)) \
    m(FillPie, (GpGraphics *graphics, GpBrush *brush, REAL x, REAL y, REAL width, REAL height, REAL startAngle, REAL sweepAngle), (graphics, brush, x, y, width, height, startAngle, sweepAngle)) \
    m(FillPieI, (GpGraphics *graphics, GpBrush *brush, INT x, INT y, INT width, INT height, REAL startAngle, REAL sweepAngle), (graphics, brush, x, y, width, height, startAngle, sweepAngle)) \
    m(FillPath, (GpGraphics *graphics, GpBrush *brush, GpPath *path), (graphics, brush, path)) \
    m(FillClosedCurve, (GpGraphics *graphics, GpBrush *brush, GDIPCONST GpPointF *points, INT count), (graphics, brush, points, count)) \
    m(FillClosedCurveI, (GpGraphics *graphics, GpBrush *brush, GDIPCONST GpPoint *points, INT count), (graphics, brush, points, count)) \
    m(FillClosedCurve2, (GpGraphics *graphics, GpBrush *brush, GDIPCONST GpPointF *points, INT count, REAL tension, GpFillMode fillMode), (graphics, brush, points, count, tension, fillMode)) \
    m(FillClosedCurve2I, (GpGraphics *graphics, GpBrush *brush, GDIPCONST GpPoint *points, INT count, REAL tension, GpFillMode fillMode), (graphics, brush, points, count, tension, fillMode)) \
    m(FillRegion, (GpGraphics *graphics, GpBrush *brush, GpRegion *region), (graphics, brush, region)) \
    m(DrawImage, (GpGraphics *graphics, GpImage *image, REAL x, REAL y), (graphics, image, x, y)) \
    m(DrawImageI, (GpGraphics *graphics, GpImage *image, INT x, INT y), (graphics, image, x, y)) \
    m(DrawImageRect, (GpGraphics *graphics, GpImage *image, REAL x, REAL y, REAL width, REAL height), (graphics, image, x, y, width, height)) \
    m(DrawImageRectI, (GpGraphics *graphics, GpImage *image, INT x, INT y, INT width, INT height), (graphics, image, x, y, width, height)) \
    m(DrawImagePoints, (GpGraphics *graphics, GpImage *image, GDIPCONST GpPointF *dstpoints, INT count), (graphics, image, dstpoints, count)) \
    m(DrawImagePointsI, (GpGraphics *graphics, GpImage *image, GDIPCONST GpPoint *dstpoints, INT count), (graphics, image, dstpoints, count)) \
    m(DrawImagePointRect, (GpGraphics *graphics, GpImage *image, REAL x, REAL y, REAL srcx, REAL srcy, REAL srcwidth, REAL srcheight, GpUnit srcUnit), (graphics, image, x, y, srcx, srcy, srcwidth, srcheight, srcUnit)) \
    m(DrawImagePointRectI, (GpGraphics *graphics, GpImage *image, INT x, INT y, INT srcx, INT srcy, INT srcwidth, INT srcheight, GpUnit srcUnit), (graphics, image, x, y, srcx, srcy, srcwidth, srcheight, srcUnit)) \
    m(DrawImageRectRect, (GpGraphics *graphics, GpImage *image, REAL dstx, REAL dsty, REAL dstwidth, REAL dstheight, REAL srcx, REAL srcy, REAL srcwidth, REAL srcheight, GpUnit srcUnit, GDIPCONST GpImageAttributes* imageAttributes, DrawImageAbort callback, VOID *callbackData), (graphics, image, dstx, dsty, dstwidth, dstheight, srcx, srcy, srcwidth, srcheight, srcUnit, imageAttributes, callback, callbackData)) \
    m(DrawImageRectRectI, (GpGraphics *graphics, GpImage *image, INT dstx, INT dsty, INT dstwidth, INT dstheight, INT srcx, INT srcy, INT srcwidth, INT srcheight, GpUnit srcUnit, GDIPCONST GpImageAttributes* imageAttributes, DrawImageAbort callback, VOID *callbackData), (graphics, image, dstx, dsty, dstwidth, dstheight, srcx, srcy, srcwidth, srcheight, srcUnit, imageAttributes, callback, callbackData)) \
    m(DrawImagePointsRect, (GpGraphics *graphics, GpImage *image, GDIPCONST GpPointF *points, INT count, REAL srcx, REAL srcy, REAL srcwidth, REAL srcheight, GpUnit srcUnit, GDIPCONST GpImageAttributes* imageAttributes, DrawImageAbort callback, VOID *callbackData), (graphics, image, points, count, srcx, srcy, srcwidth, srcheight, srcUnit, imageAttributes, callback, callbackData)) \
    m(DrawImagePointsRectI, (GpGraphics *graphics, GpImage *image, GDIPCONST GpPoint *points, INT count, INT srcx, INT srcy, INT srcwidth, INT srcheight, GpUnit srcUnit, GDIPCONST GpImageAttributes* imageAttributes, DrawImageAbort callback, VOID *callbackData), (graphics, image, points, count, srcx, srcy, srcwidth, srcheight, srcUnit, imageAttributes, callback, callbackData)) \
    m(EnumerateMetafileDestPoint, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST PointF & destPoint, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destPoint, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileDestPointI, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST Point & destPoint, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destPoint, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileDestRect, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST RectF & destRect, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destRect, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileDestRectI, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST Rect & destRect, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destRect, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileDestPoints, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST PointF *destPoints, INT count, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destPoints, count, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileDestPointsI, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST Point *destPoints, INT count, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destPoints, count, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileSrcRectDestPoint, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST PointF & destPoint, GDIPCONST RectF & srcRect, Unit srcUnit, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destPoint, srcRect, srcUnit, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileSrcRectDestPointI, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST Point & destPoint, GDIPCONST Rect & srcRect, Unit srcUnit, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destPoint, srcRect, srcUnit, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileSrcRectDestRect, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST RectF & destRect, GDIPCONST RectF & srcRect, Unit srcUnit, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destRect, srcRect, srcUnit, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileSrcRectDestRectI, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST Rect & destRect, GDIPCONST Rect & srcRect, Unit srcUnit, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destRect, srcRect, srcUnit, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileSrcRectDestPoints, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST PointF *destPoints, INT count, GDIPCONST RectF & srcRect, Unit srcUnit, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destPoints, count, srcRect, srcUnit, callback, callbackData, imageAttributes)) \
    m(EnumerateMetafileSrcRectDestPointsI, (GpGraphics *graphics, GDIPCONST GpMetafile *metafile, GDIPCONST Point *destPoints, INT count, GDIPCONST Rect & srcRect, Unit srcUnit, EnumerateMetafileProc callback, VOID *callbackData, GDIPCONST GpImageAttributes *imageAttributes), (graphics, metafile, destPoints, count, srcRect, srcUnit, callback, callbackData, imageAttributes)) \
    m(PlayMetafileRecord, (GDIPCONST GpMetafile *metafile, EmfPlusRecordType recordType, UINT flags, UINT dataSize, GDIPCONST BYTE *data), (metafile, recordType, flags, dataSize, data)) \
    m(SetClipGraphics, (GpGraphics *graphics, GpGraphics *srcgraphics, CombineMode combineMode), (graphics, srcgraphics, combineMode)) \
    m(SetClipRect, (GpGraphics *graphics, REAL x, REAL y, REAL width, REAL height, CombineMode combineMode), (graphics, x, y, width, height, combineMode)) \
    m(SetClipRectI, (GpGraphics *graphics, INT x, INT y, INT width, INT height, CombineMode combineMode), (graphics, x, y, width, height, combineMode)) \
    m(SetClipPath, (GpGraphics *graphics, GpPath *path, CombineMode combineMode), (graphics, path, combineMode)) \
    m(SetClipRegion, (GpGraphics *graphics, GpRegion *region, CombineMode combineMode), (graphics, region, combineMode)) \
    m(SetClipHrgn, (GpGraphics *graphics, HRGN hRgn, CombineMode combineMode), (graphics, hRgn, combineMode)) \
    m(ResetClip, (GpGraphics *graphics), (graphics)) \
    m(TranslateClip, (GpGraphics *graphics, REAL dx, REAL dy), (graphics, dx, dy)) \
    m(TranslateClipI, (GpGraphics *graphics, INT dx, INT dy), (graphics, dx, dy)) \
    m(GetClip, (GpGraphics *graphics, GpRegion *region), (graphics, region)) \
    m(GetClipBounds, (GpGraphics *graphics, GpRectF *rect), (graphics, rect)) \
    m(GetClipBoundsI, (GpGraphics *graphics, GpRect *rect), (graphics, rect)) \
    m(IsClipEmpty, (GpGraphics *graphics, BOOL *result), (graphics, result)) \
    m(GetVisibleClipBounds, (GpGraphics *graphics, GpRectF *rect), (graphics, rect)) \
    m(GetVisibleClipBoundsI, (GpGraphics *graphics, GpRect *rect), (graphics, rect)) \
    m(IsVisibleClipEmpty, (GpGraphics *graphics, BOOL *result), (graphics, result)) \
    m(IsVisiblePoint, (GpGraphics *graphics, REAL x, REAL y, BOOL *result), (graphics, x, y, result)) \
    m(IsVisiblePointI, (GpGraphics *graphics, INT x, INT y, BOOL *result), (graphics, x, y, result)) \
    m(IsVisibleRect, (GpGraphics *graphics, REAL x, REAL y, REAL width, REAL height, BOOL *result), (graphics, x, y, width, height, result)) \
    m(IsVisibleRectI, (GpGraphics *graphics, INT x, INT y, INT width, INT height, BOOL *result), (graphics, x, y, width, height, result)) \
    m(SaveGraphics, (GpGraphics *graphics, GraphicsState *state), (graphics, state)) \
    m(RestoreGraphics, (GpGraphics *graphics, GraphicsState state), (graphics, state)) \
    m(BeginContainer, (GpGraphics *graphics, GDIPCONST GpRectF* dstrect, GDIPCONST GpRectF *srcrect, GpUnit unit, GraphicsContainer *state), (graphics, dstrect, srcrect, unit, state)) \
    m(BeginContainerI, (GpGraphics *graphics, GDIPCONST GpRect* dstrect, GDIPCONST GpRect *srcrect, GpUnit unit, GraphicsContainer *state), (graphics, dstrect, srcrect, unit, state)) \
    m(BeginContainer2, (GpGraphics *graphics, GraphicsContainer* state), (graphics, state)) \
    m(EndContainer, (GpGraphics *graphics, GraphicsContainer state), (graphics, state)) \
    m(GetMetafileHeaderFromEmf, (HENHMETAFILE hEmf, MetafileHeader *header), (hEmf, header)) \
    m(GetMetafileHeaderFromFile, (GDIPCONST WCHAR* filename, MetafileHeader *header), (filename, header)) \
    m(GetMetafileHeaderFromStream, (IStream *stream, MetafileHeader *header), (stream, header)) \
    m(GetMetafileHeaderFromMetafile, (GpMetafile *metafile, MetafileHeader *header), (metafile, header)) \
    m(GetHemfFromMetafile, (GpMetafile *metafile, HENHMETAFILE *hEmf), (metafile, hEmf)) \
    m(CreateStreamOnFile, (GDIPCONST WCHAR *filename, UINT access, IStream **stream), (filename, access, stream)) \
    m(CreateMetafileFromWmf, (HMETAFILE hWmf, BOOL deleteWmf, GDIPCONST WmfPlaceableFileHeader *wmfPlaceableFileHeader, GpMetafile **metafile), (hWmf, deleteWmf, wmfPlaceableFileHeader, metafile)) \
    m(CreateMetafileFromEmf, (HENHMETAFILE hEmf, BOOL deleteEmf, GpMetafile **metafile), (hEmf, deleteEmf, metafile)) \
    m(CreateMetafileFromFile, (GDIPCONST WCHAR* file, GpMetafile **metafile), (file, metafile)) \
    m(CreateMetafileFromWmfFile, (GDIPCONST WCHAR* file, GDIPCONST WmfPlaceableFileHeader *wmfPlaceableFileHeader, GpMetafile **metafile), (file, wmfPlaceableFileHeader, metafile)) \
    m(CreateMetafileFromStream, (IStream *stream, GpMetafile **metafile), (stream, metafile)) \
    m(RecordMetafile, (HDC referenceHdc, EmfType type, GDIPCONST GpRectF *frameRect, MetafileFrameUnit frameUnit, GDIPCONST WCHAR *description, GpMetafile ** metafile), (referenceHdc, type, frameRect, frameUnit, description, metafile)) \
    m(RecordMetafileI, (HDC referenceHdc, EmfType type, GDIPCONST GpRect *frameRect, MetafileFrameUnit frameUnit, GDIPCONST WCHAR *description, GpMetafile ** metafile), (referenceHdc, type, frameRect, frameUnit, description, metafile)) \
    m(RecordMetafileFileName, (GDIPCONST WCHAR* fileName, HDC referenceHdc, EmfType type, GDIPCONST GpRectF *frameRect, MetafileFrameUnit frameUnit, GDIPCONST WCHAR *description, GpMetafile ** metafile), (fileName, referenceHdc, type, frameRect, frameUnit, description, metafile)) \
    m(RecordMetafileFileNameI, (GDIPCONST WCHAR* fileName, HDC referenceHdc, EmfType type, GDIPCONST GpRect *frameRect, MetafileFrameUnit frameUnit, GDIPCONST WCHAR *description, GpMetafile ** metafile), (fileName, referenceHdc, type, frameRect, frameUnit, description, metafile)) \
    m(RecordMetafileStream, (IStream *stream, HDC referenceHdc, EmfType type, GDIPCONST GpRectF *frameRect, MetafileFrameUnit frameUnit, GDIPCONST WCHAR *description, GpMetafile ** metafile), (stream, referenceHdc, type, frameRect, frameUnit, description, metafile)) \
    m(RecordMetafileStreamI, (IStream *stream, HDC referenceHdc, EmfType type, GDIPCONST GpRect *frameRect, MetafileFrameUnit frameUnit, GDIPCONST WCHAR *description, GpMetafile ** metafile), (stream, referenceHdc, type, frameRect, frameUnit, description, metafile)) \
    m(SetMetafileDownLevelRasterizationLimit, (GpMetafile *metafile, UINT metafileRasterizationLimitDpi), (metafile, metafileRasterizationLimitDpi)) \
    m(GetMetafileDownLevelRasterizationLimit, (GDIPCONST GpMetafile *metafile, UINT *metafileRasterizationLimitDpi), (metafile, metafileRasterizationLimitDpi)) \
    m(GetImageDecodersSize, (UINT *numDecoders, UINT *size), (numDecoders, size)) \
    m(GetImageDecoders, (UINT numDecoders, UINT size, ImageCodecInfo *decoders), (numDecoders, size, decoders)) \
    m(GetImageEncodersSize, (UINT *numEncoders, UINT *size), (numEncoders, size)) \
    m(GetImageEncoders, (UINT numEncoders, UINT size, ImageCodecInfo *encoders), (numEncoders, size, encoders)) \
    m(Comment, (GpGraphics* graphics, UINT sizeData, GDIPCONST BYTE *data), (graphics, sizeData, data)) \
    m(CreateFontFamilyFromName, (GDIPCONST WCHAR *name, GpFontCollection *fontCollection, GpFontFamily **FontFamily), (name, fontCollection, FontFamily)) \
    m(DeleteFontFamily, (GpFontFamily *FontFamily), (FontFamily)) \
    m(CloneFontFamily, (GpFontFamily *FontFamily, GpFontFamily **clonedFontFamily), (FontFamily, clonedFontFamily)) \
    m(GetGenericFontFamilySansSerif, (GpFontFamily **nativeFamily), (nativeFamily)) \
    m(GetGenericFontFamilySerif, (GpFontFamily **nativeFamily), (nativeFamily)) \
    m(GetGenericFontFamilyMonospace, (GpFontFamily **nativeFamily), (nativeFamily)) \
    m(GetFamilyName, (GDIPCONST GpFontFamily *family, WCHAR name[LF_FACESIZE], LANGID language), (family, name, language)) \
    m(IsStyleAvailable, (GDIPCONST GpFontFamily *family, INT style, BOOL *IsStyleAvailable), (family, style, IsStyleAvailable)) \
    m(GetEmHeight, (GDIPCONST GpFontFamily *family, INT style, UINT16 *EmHeight), (family, style, EmHeight)) \
    m(GetCellAscent, (GDIPCONST GpFontFamily *family, INT style, UINT16 *CellAscent), (family, style, CellAscent)) \
    m(GetCellDescent, (GDIPCONST GpFontFamily *family, INT style, UINT16 *CellDescent), (family, style, CellDescent)) \
    m(GetLineSpacing, (GDIPCONST GpFontFamily *family, INT style, UINT16 *LineSpacing), (family, style, LineSpacing)) \
    m(CreateFontFromDC, (HDC hdc, GpFont **font), (hdc, font)) \
    m(CreateFontFromLogfontA, (HDC hdc, GDIPCONST LOGFONTA *logfont, GpFont **font), (hdc, logfont, font)) \
    m(CreateFontFromLogfontW, (HDC hdc, GDIPCONST LOGFONTW *logfont, GpFont **font), (hdc, logfont, font)) \
    m(CreateFont, (GDIPCONST GpFontFamily *fontFamily, REAL emSize, INT style, Unit unit, GpFont **font), (fontFamily, emSize, style, unit, font)) \
    m(CloneFont, (GpFont* font, GpFont** cloneFont), (font, cloneFont)) \
    m(DeleteFont, (GpFont* font), (font)) \
    m(GetFamily, (GpFont *font, GpFontFamily **family), (font, family)) \
    m(GetFontStyle, (GpFont *font, INT *style), (font, style)) \
    m(GetFontSize, (GpFont *font, REAL *size), (font, size)) \
    m(GetFontUnit, (GpFont *font, Unit *unit), (font, unit)) \
    m(GetFontHeight, (GDIPCONST GpFont *font, GDIPCONST GpGraphics *graphics, REAL *height), (font, graphics, height)) \
    m(GetFontHeightGivenDPI, (GDIPCONST GpFont *font, REAL dpi, REAL *height), (font, dpi, height)) \
    m(GetLogFontA, (GpFont *font, GpGraphics *graphics, LOGFONTA *logfontA), (font, graphics, logfontA)) \
    m(GetLogFontW, (GpFont *font, GpGraphics *graphics, LOGFONTW *logfontW), (font, graphics, logfontW)) \
    m(NewInstalledFontCollection, (GpFontCollection** fontCollection), (fontCollection)) \
    m(NewPrivateFontCollection, (GpFontCollection** fontCollection), (fontCollection)) \
    m(DeletePrivateFontCollection, (GpFontCollection** fontCollection), (fontCollection)) \
    m(GetFontCollectionFamilyCount, (GpFontCollection* fontCollection, INT *numFound), (fontCollection, numFound)) \
    m(GetFontCollectionFamilyList, (GpFontCollection* fontCollection, INT numSought, GpFontFamily* gpfamilies[], INT* numFound), (fontCollection, numSought, gpfamilies, numFound)) \
    m(PrivateAddFontFile, (GpFontCollection* fontCollection, GDIPCONST WCHAR* filename), (fontCollection, filename)) \
    m(PrivateAddMemoryFont, (GpFontCollection* fontCollection, GDIPCONST void* memory, INT length), (fontCollection, memory, length)) \
    m(DrawString, (GpGraphics *graphics, GDIPCONST WCHAR *string, INT length, GDIPCONST GpFont *font, GDIPCONST RectF *layoutRect, GDIPCONST GpStringFormat *stringFormat, GDIPCONST GpBrush *brush), (graphics, string, length, font, layoutRect, stringFormat, brush)) \
    m(MeasureString, (GpGraphics *graphics, GDIPCONST WCHAR *string, INT length, GDIPCONST GpFont *font, GDIPCONST RectF *layoutRect, GDIPCONST GpStringFormat *stringFormat, RectF *boundingBox, INT *codepointsFitted, INT *linesFilled), (graphics, string, length, font, layoutRect, stringFormat, boundingBox, codepointsFitted, linesFilled)) \
    m(MeasureCharacterRanges, (GpGraphics *graphics, GDIPCONST WCHAR *string, INT length, GDIPCONST GpFont *font, GDIPCONST RectF &layoutRect, GDIPCONST GpStringFormat *stringFormat, INT regionCount, GpRegion **regions), (graphics, string, length, font, layoutRect, stringFormat, regionCount, regions)) \
    m(DrawDriverString, (GpGraphics *graphics, GDIPCONST UINT16 *text, INT length, GDIPCONST GpFont *font, GDIPCONST GpBrush *brush, GDIPCONST PointF *positions, INT flags, GDIPCONST GpMatrix *matrix), (graphics, text, length, font, brush, positions, flags, matrix)) \
    m(MeasureDriverString, (GpGraphics *graphics, GDIPCONST UINT16 *text, INT length, GDIPCONST GpFont *font, GDIPCONST PointF *positions, INT flags, GDIPCONST GpMatrix *matrix, RectF *boundingBox), (graphics, text, length, font, positions, flags, matrix, boundingBox)) \
    m(CreateStringFormat, (INT formatAttributes, LANGID language, GpStringFormat **format), (formatAttributes, language, format)) \
    m(StringFormatGetGenericDefault, (GpStringFormat **format), (format)) \
    m(StringFormatGetGenericTypographic, (GpStringFormat **format), (format)) \
    m(DeleteStringFormat, (GpStringFormat *format), (format)) \
    m(CloneStringFormat, (GDIPCONST GpStringFormat *format, GpStringFormat **newFormat), (format, newFormat)) \
    m(SetStringFormatFlags, (GpStringFormat *format, INT flags), (format, flags)) \
    m(GetStringFormatFlags, (GDIPCONST GpStringFormat *format, INT *flags), (format, flags)) \
    m(SetStringFormatAlign, (GpStringFormat *format, StringAlignment align), (format, align)) \
    m(GetStringFormatAlign, (GDIPCONST GpStringFormat *format, StringAlignment *align), (format, align)) \
    m(SetStringFormatLineAlign, (GpStringFormat *format, StringAlignment align), (format, align)) \
    m(GetStringFormatLineAlign, (GDIPCONST GpStringFormat *format, StringAlignment *align), (format, align)) \
    m(SetStringFormatTrimming, (GpStringFormat *format, StringTrimming trimming), (format, trimming)) \
    m(GetStringFormatTrimming, (GDIPCONST GpStringFormat *format, StringTrimming *trimming), (format, trimming)) \
    m(SetStringFormatHotkeyPrefix, (GpStringFormat *format, INT hotkeyPrefix), (format, hotkeyPrefix)) \
    m(GetStringFormatHotkeyPrefix, (GDIPCONST GpStringFormat *format, INT *hotkeyPrefix), (format, hotkeyPrefix)) \
    m(SetStringFormatTabStops, (GpStringFormat *format, REAL firstTabOffset, INT count, GDIPCONST REAL *tabStops), (format, firstTabOffset, count, tabStops)) \
    m(GetStringFormatTabStops, (GDIPCONST GpStringFormat *format, INT count, REAL *firstTabOffset, REAL *tabStops), (format, count, firstTabOffset, tabStops)) \
    m(GetStringFormatTabStopCount, (GDIPCONST GpStringFormat *format, INT *count), (format, count)) \
    m(SetStringFormatDigitSubstitution, (GpStringFormat *format, LANGID language, StringDigitSubstitute substitute), (format, language, substitute)) \
    m(GetStringFormatDigitSubstitution, (GDIPCONST GpStringFormat *format, LANGID *language, StringDigitSubstitute *substitute), (format, language, substitute)) \
    m(GetStringFormatMeasurableCharacterRangeCount, (GDIPCONST GpStringFormat *format, INT *count), (format, count)) \
    m(SetStringFormatMeasurableCharacterRanges, (GpStringFormat *format, INT rangeCount, GDIPCONST CharacterRange *ranges), (format, rangeCount, ranges)) \
    m(CreateCachedBitmap, (GpBitmap *bitmap, GpGraphics *graphics, GpCachedBitmap **cachedBitmap), (bitmap, graphics, cachedBitmap)) \
    m(DeleteCachedBitmap, (GpCachedBitmap *cachedBitmap), (cachedBitmap)) \
    m(DrawCachedBitmap, (GpGraphics *graphics, GpCachedBitmap *cachedBitmap, INT x, INT y), (graphics, cachedBitmap, x, y)) \
    m(SetImageAttributesCachedBackground, (GpImageAttributes *imageattr, BOOL enableFlag), (imageattr, enableFlag)) \
    m(TestControl, (GpTestControlEnum control, void *param), (control, param)) \

    // non-standard/problematic functions, to review later if needed
#if 0
    // these functions don't seem to exist in the DLL even though they are
    // declared in the header
    m(SetImageAttributesICMMode, (GpImageAttributes *imageAttr, BOOL on), (imageAttr, on)) \
    m(FontCollectionEnumerable, (GpFontCollection* fontCollection, GpGraphics* graphics, INT *numFound), (fontCollection, graphics, numFound)) \
    m(FontCollectionEnumerate, (GpFontCollection* fontCollection, INT numSought, GpFontFamily* gpfamilies[], INT* numFound, GpGraphics* graphics), (fontCollection, numSought, gpfamilies, numFound, graphics)) \

GpStatus
GdipGetMetafileHeaderFromWmf(
    HMETAFILE           hWmf,
    GDIPCONST WmfPlaceableFileHeader *     wmfPlaceableFileHeader,
    MetafileHeader *    header
    );
HPALETTE WINGDIPAPI
GdipCreateHalftonePalette();

UINT WINGDIPAPI
GdipEmfToWmfBits(
    HENHMETAFILE hemf,
    UINT         cbData16,
    LPBYTE       pData16,
    INT          iMapMode,
    INT          eFlags
);
#endif // 0

// this macro expands into an invocation of the given macro m for all GDI+
// functions: m is called with the name of the function without "Gdip" prefix
#define wxFOR_ALL_GDIP_FUNCNAMES(m)                                           \
    m(Alloc, (size_t size), (size))                                           \
    m(Free, (void *ptr), (ptr))                                               \
    wxFOR_ALL_GDIPLUS_STATUS_FUNCS(m)

// unfortunately we need a separate macro for these functions as they have
// "Gdiplus" prefix instead of "Gdip" for (almost) all the others (and also
// WINAPI calling convention instead of WINGDIPAPI although they happen to be
// both stdcall in fact)
#define wxFOR_ALL_GDIPLUS_FUNCNAMES(m)                                        \
    m(Startup, (ULONG_PTR *token,                                             \
                const GdiplusStartupInput *input,                             \
                GdiplusStartupOutput *output),                                \
                (token, input, output))                                       \
    m(Shutdown, (ULONG_PTR token), (token))                                   \
    m(NotificationHook, (ULONG_PTR *token), (token))                          \
    m(NotificationUnhook, (ULONG_PTR token), (token))                         \

#define wxFOR_ALL_FUNCNAMES(m)                                                \
    wxFOR_ALL_GDIP_FUNCNAMES(m)                                               \
    wxFOR_ALL_GDIPLUS_FUNCNAMES(m)

// ----------------------------------------------------------------------------
// declare typedefs for types of all GDI+ functions
// ----------------------------------------------------------------------------

extern "C"
{

typedef void* (WINGDIPAPI *wxGDIPLUS_FUNC_T(Alloc))(size_t size);
typedef void (WINGDIPAPI *wxGDIPLUS_FUNC_T(Free))(void* ptr);
typedef Status
(WINAPI *wxGDIPLUS_FUNC_T(Startup))(ULONG_PTR *token,
                                    const GdiplusStartupInput *input,
                                    GdiplusStartupOutput *output);
typedef void (WINAPI *wxGDIPLUS_FUNC_T(Shutdown))(ULONG_PTR token);
typedef GpStatus (WINAPI *wxGDIPLUS_FUNC_T(NotificationHook))(ULONG_PTR *token);
typedef void (WINAPI *wxGDIPLUS_FUNC_T(NotificationUnhook))(ULONG_PTR token);

#define wxDECL_GDIPLUS_FUNC_TYPE(name, params, args) \
    typedef GpStatus (WINGDIPAPI *wxGDIPLUS_FUNC_T(name)) params ;

wxFOR_ALL_GDIPLUS_STATUS_FUNCS(wxDECL_GDIPLUS_FUNC_TYPE)

#undef wxDECL_GDIPLUS_FUNC_TYPE

// Special hack for w32api headers that reference this variable which is
// normally defined in w32api-specific gdiplus.lib but as we don't link with it
// and load gdiplus.dll dynamically, it's not defined in our case resulting in
// linking errors -- so just provide it ourselves, it doesn't matter where it
// is and if Cygwin headers are modified to not use it in the future, it's not
// a big deal neither, we'll just have an unused pointer.
#if defined(__CYGWIN__) || defined(__MINGW32__)
void *_GdipStringFormatCachedGenericTypographic = NULL;
#endif // __CYGWIN__ || __MINGW32__

} // extern "C"

// ============================================================================
// wxGdiPlus helper class
// ============================================================================

class wxGdiPlus
{
public:
    // load GDI+ DLL when we're called for the first time, return true on
    // success or false on failure
    static bool Initialize()
    {
        if ( m_initialized == -1 )
            m_initialized = DoInit();

        return m_initialized == 1;
    }

    // check if we're initialized without loading the GDI+ DLL
    static bool IsInitialized()
    {
        return m_initialized == 1;
    }

    // shutdown: should be called on termination to unload the GDI+ DLL, safe
    // to call even if we hadn't loaded it
    static void Terminate()
    {
        if ( m_hdll )
        {
            wxDynamicLibrary::Unload(m_hdll);
            m_hdll = 0;
        }

        m_initialized = -1;
    }


    // define function pointers as members
    #define wxDECL_GDIPLUS_FUNC_MEMBER(name, params, args) \
        static wxGDIPLUS_FUNC_T(name) name;

    wxFOR_ALL_FUNCNAMES(wxDECL_GDIPLUS_FUNC_MEMBER)

    #undef wxDECL_GDIPLUS_FUNC_MEMBER

private:
    // do load the GDI+ DLL and bind all the functions
    static bool DoInit();


    // initially -1 meaning unknown, set to false or true by Initialize()
    static int m_initialized;

    // handle of the GDI+ DLL if we loaded it successfully
    static wxDllType m_hdll;
};

#define wxINIT_GDIPLUS_FUNC(name, params, args) \
    wxGDIPLUS_FUNC_T(name) wxGdiPlus::name = NULL;

wxFOR_ALL_FUNCNAMES(wxINIT_GDIPLUS_FUNC)

#undef wxINIT_GDIPLUS_FUNC

int wxGdiPlus::m_initialized = -1;
wxDllType wxGdiPlus::m_hdll = 0;

/* static */
bool wxGdiPlus::DoInit()
{
    // we're prepared to handler errors so suppress log messages about them
    wxLogNull noLog;

    wxDynamicLibrary dllGdip(wxT("gdiplus.dll"), wxDL_VERBATIM);
    if ( !dllGdip.IsLoaded() )
        return false;

    // use RawGetSymbol() for efficiency, we have ~600 functions to load...
    #define wxDO_LOAD_FUNC(name, namedll)                                     \
        name = (wxGDIPLUS_FUNC_T(name))dllGdip.RawGetSymbol(namedll);         \
        if ( !name )                                                          \
            return false;

    #define wxLOAD_GDIPLUS_FUNC(name, params, args)                           \
        wxDO_LOAD_FUNC(name, wxT("Gdiplus") wxSTRINGIZE_T(name))

    wxFOR_ALL_GDIPLUS_FUNCNAMES(wxLOAD_GDIPLUS_FUNC)

    #undef wxLOAD_GDIPLUS_FUNC

    #define wxLOAD_GDIP_FUNC(name, params, args)                              \
        wxDO_LOAD_FUNC(name, wxT("Gdip") wxSTRINGIZE_T(name))

    wxFOR_ALL_GDIP_FUNCNAMES(wxLOAD_GDIP_FUNC)

    #undef wxLOAD_GDIP_FUNC

    // ok, prevent the DLL from being unloaded right now, we'll do it later
    m_hdll = dllGdip.Detach();

    return true;
}

// ============================================================================
// module to unload GDI+ DLL on program termination
// ============================================================================

class wxGdiPlusModule : public wxModule
{
public:
    virtual bool OnInit() { return true; }
    virtual void OnExit() { wxGdiPlus::Terminate(); }

    DECLARE_DYNAMIC_CLASS(wxGdiPlusModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxGdiPlusModule, wxModule)

// ============================================================================
// implementation of the functions themselves
// ============================================================================

extern "C"
{

void* WINGDIPAPI
GdipAlloc(size_t size)
{
    return wxGdiPlus::Initialize() ? wxGdiPlus::Alloc(size) : NULL;
}

void WINGDIPAPI
GdipFree(void* ptr)
{
    if ( wxGdiPlus::Initialize() )
        wxGdiPlus::Free(ptr);
}

Status WINAPI
GdiplusStartup(ULONG_PTR *token,
               const GdiplusStartupInput *input,
               GdiplusStartupOutput *output)
{
    return wxGdiPlus::Initialize() ? wxGdiPlus::Startup(token, input, output)
                                   : GdiplusNotInitialized;
}

void WINAPI
GdiplusShutdown(ULONG_PTR token)
{
    if ( wxGdiPlus::IsInitialized() )
        wxGdiPlus::Shutdown(token);
}

#define wxIMPL_GDIPLUS_FUNC(name, params, args)                               \
    GpStatus WINGDIPAPI                                                       \
    Gdip##name params                                                         \
    {                                                                         \
        return wxGdiPlus::Initialize() ? wxGdiPlus::name args                 \
                                       : GdiplusNotInitialized;               \
    }

wxFOR_ALL_GDIPLUS_STATUS_FUNCS(wxIMPL_GDIPLUS_FUNC)

#undef wxIMPL_GDIPLUS_FUNC

} // extern "C"

#endif // wxUSE_GRAPHICS_CONTEXT

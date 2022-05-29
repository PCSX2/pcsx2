/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "GSVertexSW.h"
#include "GS/Renderers/Common/GSFunctionMap.h"
#include "GS/GSAlignedClass.h"
#include "GS/GSPerfMon.h"
#include "GS/GSThread_CXX11.h"
#include "GS/GSRingHeap.h"

class alignas(32) GSRasterizerData : public GSAlignedClass<32>
{
	static int s_counter;

public:
	GSVector4i scissor;
	GSVector4i bbox;
	GS_PRIM_CLASS primclass;
	u8* buff;
	GSVertexSW* vertex;
	int vertex_count;
	u32* index;
	int index_count;
	u64 frame;
	u64 start;
	int pixels;
	int counter;
	u8 scanmsk_value;

	GSRasterizerData()
		: scissor(GSVector4i::zero())
		, bbox(GSVector4i::zero())
		, primclass(GS_INVALID_CLASS)
		, buff(nullptr)
		, vertex(NULL)
		, vertex_count(0)
		, index(NULL)
		, index_count(0)
		, frame(0)
		, start(0)
		, pixels(0)
		, scanmsk_value(0)
	{
		counter = s_counter++;
	}

	virtual ~GSRasterizerData()
	{
		if (buff != NULL)
			GSRingHeap::free(buff);
	}
};

class IDrawScanline : public GSAlignedClass<32>
{
public:
	typedef void (*SetupPrimPtr)(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan);
	typedef void (*DrawScanlinePtr)(int pixels, int left, int top, const GSVertexSW& scan);
	typedef void (IDrawScanline::*DrawRectPtr)(const GSVector4i& r, const GSVertexSW& v); // TODO: jit

protected:
	SetupPrimPtr m_sp;
	DrawScanlinePtr m_ds;
	DrawScanlinePtr m_de;
	DrawRectPtr m_dr;

public:
	IDrawScanline()
		: m_sp(NULL)
		, m_ds(NULL)
		, m_de(NULL)
		, m_dr(NULL)
	{
	}
	virtual ~IDrawScanline() {}

	virtual void BeginDraw(const GSRasterizerData* data) = 0;
	virtual void EndDraw(u64 frame, u64 ticks, int actual, int total, int prims) = 0;

#ifdef ENABLE_JIT_RASTERIZER

	__forceinline void SetupPrim(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan) { m_sp(vertex, index, dscan); }
	__forceinline void DrawScanline(int pixels, int left, int top, const GSVertexSW& scan) { m_ds(pixels, left, top, scan); }
	__forceinline void DrawEdge(int pixels, int left, int top, const GSVertexSW& scan) { m_de(pixels, left, top, scan); }
	__forceinline void DrawRect(const GSVector4i& r, const GSVertexSW& v) { (this->*m_dr)(r, v); }

#else

	virtual void SetupPrim(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan) = 0;
	virtual void DrawScanline(int pixels, int left, int top, const GSVertexSW& scan) = 0;
	virtual void DrawEdge(int pixels, int left, int top, const GSVertexSW& scan) = 0;
	virtual void DrawRect(const GSVector4i& r, const GSVertexSW& v) = 0;

#endif

	virtual void PrintStats() = 0;

	__forceinline bool HasEdge() const { return m_de != NULL; }
	__forceinline bool IsSolidRect() const { return m_dr != NULL; }
};

class IRasterizer : public GSAlignedClass<32>
{
public:
	virtual ~IRasterizer() {}

	virtual void Queue(const GSRingHeap::SharedPtr<GSRasterizerData>& data) = 0;
	virtual void Sync() = 0;
	virtual bool IsSynced() const = 0;
	virtual int GetPixels(bool reset = true) = 0;
	virtual void PrintStats() = 0;
};

class alignas(32) GSRasterizer : public IRasterizer
{
protected:
	IDrawScanline* m_ds;
	int m_id;
	int m_threads;
	int m_thread_height;
	u8* m_scanline;
	u8 m_scanmsk_value;
	GSVector4i m_scissor;
	GSVector4 m_fscissor_x;
	GSVector4 m_fscissor_y;
	struct { GSVertexSW* buff; int count; } m_edge;
	struct { int sum, actual, total; } m_pixels;
	int m_primcount;

	typedef void (GSRasterizer::*DrawPrimPtr)(const GSVertexSW* v, int count);

	template <bool scissor_test>
	void DrawPoint(const GSVertexSW* vertex, int vertex_count, const u32* index, int index_count);
	void DrawLine(const GSVertexSW* vertex, const u32* index);
	void DrawTriangle(const GSVertexSW* vertex, const u32* index);
	void DrawSprite(const GSVertexSW* vertex, const u32* index);

#if _M_SSE >= 0x501
	__forceinline void DrawTriangleSection(int top, int bottom, GSVertexSW2& RESTRICT edge, const GSVertexSW2& RESTRICT dedge, const GSVertexSW2& RESTRICT dscan, const GSVector4& RESTRICT p0);
#else
	__forceinline void DrawTriangleSection(int top, int bottom, GSVertexSW& RESTRICT edge, const GSVertexSW& RESTRICT dedge, const GSVertexSW& RESTRICT dscan, const GSVector4& RESTRICT p0);
#endif

	void DrawEdge(const GSVertexSW& v0, const GSVertexSW& v1, const GSVertexSW& dv, int orientation, int side);

	__forceinline void AddScanline(GSVertexSW* e, int pixels, int left, int top, const GSVertexSW& scan);
	__forceinline void Flush(const GSVertexSW* vertex, const u32* index, const GSVertexSW& dscan, bool edge = false);

	__forceinline void DrawScanline(int pixels, int left, int top, const GSVertexSW& scan);
	__forceinline void DrawEdge(int pixels, int left, int top, const GSVertexSW& scan);

public:
	GSRasterizer(IDrawScanline* ds, int id, int threads);
	virtual ~GSRasterizer();

	__forceinline bool IsOneOfMyScanlines(int top) const;
	__forceinline bool IsOneOfMyScanlines(int top, int bottom) const;
	__forceinline int FindMyNextScanline(int top) const;

	void Draw(GSRasterizerData* data);

	// IRasterizer

	void Queue(const GSRingHeap::SharedPtr<GSRasterizerData>& data);
	void Sync() {}
	bool IsSynced() const { return true; }
	int GetPixels(bool reset);
	void PrintStats() { m_ds->PrintStats(); }
};

class GSRasterizerList : public IRasterizer
{
protected:
	using GSWorker = GSJobQueue<GSRingHeap::SharedPtr<GSRasterizerData>, 65536>;

	// Worker threads depend on the rasterizers, so don't change the order.
	std::vector<std::unique_ptr<GSRasterizer>> m_r;
	std::vector<std::unique_ptr<GSWorker>> m_workers;
	u8* m_scanline;
	int m_thread_height;

	GSRasterizerList(int threads);

	static void OnWorkerStartup(int i);
	static void OnWorkerShutdown(int i);

public:
	virtual ~GSRasterizerList();

	template <class DS>
	static std::unique_ptr<IRasterizer> Create(int threads)
	{
		threads = std::max<int>(threads, 0);

		if (threads == 0)
		{
			return std::make_unique<GSRasterizer>(new DS(), 0, 1);
		}

		std::unique_ptr<GSRasterizerList> rl(new GSRasterizerList(threads));

		for (int i = 0; i < threads; i++)
		{
			rl->m_r.push_back(std::unique_ptr<GSRasterizer>(new GSRasterizer(new DS(), i, threads)));
			auto& r = *rl->m_r[i];
			rl->m_workers.push_back(std::unique_ptr<GSWorker>(new GSWorker(
				[i]() { GSRasterizerList::OnWorkerStartup(i); },
				[&r](GSRingHeap::SharedPtr<GSRasterizerData>& item) { r.Draw(item.get()); },
				[i]() { GSRasterizerList::OnWorkerShutdown(i); })));
		}

		return rl;
	}

	// IRasterizer

	void Queue(const GSRingHeap::SharedPtr<GSRasterizerData>& data);
	void Sync();
	bool IsSynced() const;
	int GetPixels(bool reset);
	void PrintStats() {}
};

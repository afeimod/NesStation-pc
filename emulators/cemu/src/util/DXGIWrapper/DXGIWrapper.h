#pragma once

// ★NesStation 修正（对齐上游 Cemu main 分支同款修复）：
//   wxWidgets 的 wx/msw/private.h 定义了函数式宏 #define GetHwnd() ((HWND)GetHWND())。
//   含 wxgui.h 的源文件（如 CafeSystem.cpp）再经 Renderer.h → 本文件包含
//   <dxgi1_4.h>（→ dxgi1_2.h）时，IDXGISwapChain1::GetHwnd( 被宏展开为
//   ((HWND)GetHWND())( → Windows SDK 10.0.26100 的 dxgi1_2.h(1319,43)
//   error C2059: syntax error: '('（本次 EMU cemu · windows-x64 的死因，
//   与 SDK 版本无关——任何 SDK 的 dxgi1_2.h 都有该声明）。
#undef GetHwnd
#include <dxgi1_4.h>
//#include <atlbase.h>

class DXGIWrapper
{
public:
	DXGIWrapper()
		: DXGIWrapper(nullptr)
	{}
	
	DXGIWrapper(uint8* deviceLUID)
	{
		m_moduleHandle = LoadLibraryA("dxgi.dll");
		if (!m_moduleHandle)
			throw std::runtime_error("can't load dxgi module");

		const auto pCreateDXGIFactory1 = (decltype(&CreateDXGIFactory1))GetProcAddress(m_moduleHandle, "CreateDXGIFactory1");
		if (!pCreateDXGIFactory1)
		{
			FreeLibrary(m_moduleHandle);
			throw std::runtime_error("can't find CreateDXGIFactory1 in dxgi module");
		}

		IDXGIFactory1* dxgiFactory = nullptr;
		pCreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

		IDXGIAdapter1* tmpDxgiAdapter = nullptr;
		UINT adapterIndex = 0;
		while (dxgiFactory->EnumAdapters1(adapterIndex, &tmpDxgiAdapter) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC1 desc;
			tmpDxgiAdapter->GetDesc1(&desc);

			if (deviceLUID == nullptr || memcmp(&desc.AdapterLuid, deviceLUID, sizeof(LUID)) == 0)
			{
				tmpDxgiAdapter->QueryInterface(IID_PPV_ARGS(&m_dxgiAdapter));
				tmpDxgiAdapter->Release();
				break;
			}

			tmpDxgiAdapter->Release();
			++adapterIndex;
		}

		dxgiFactory->Release();

		if (!m_dxgiAdapter)
		{
			Cleanup();
			throw std::runtime_error("can't create dxgi adapter");
		}
	}

	~DXGIWrapper()
	{
		Cleanup();
	}

	bool QueryVideoMemoryInfo(DXGI_QUERY_VIDEO_MEMORY_INFO& info) const
	{
		return m_dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info) == S_OK;
	}

private:
	HMODULE m_moduleHandle = nullptr;
	IDXGIAdapter3* m_dxgiAdapter = nullptr;

	void Cleanup()
	{
		if (m_dxgiAdapter)
		{
			m_dxgiAdapter->Release();
			m_dxgiAdapter = nullptr;
		}

		if (m_moduleHandle)
		{
			FreeLibrary(m_moduleHandle);
			m_moduleHandle = nullptr;
		}
	}
};
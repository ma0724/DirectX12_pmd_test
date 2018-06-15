#ifndef DEF_D3D12MANAGER_H
#define DEF_D3D12MANAGER_H

#include <Windows.h>
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "natumashi.h"
#include "Light.h"
#include <vector>
#include <memory>


#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3d12.lib")


class VMDLoader;
class VMDData;
class ShadowMapDebug;
class Pmd;
class Plane;
class Light;
class ShadowMapping;

class D3D12Manager {
public:
	static D3D12Manager* Instiate(HWND& hwnd, const int& windowWith, const int& windowHeight);
	~D3D12Manager();
	HRESULT Render();
private:
	static constexpr int RTV_NUM = 2;
	HWND	window_handle_;
	int		window_width_;
	int		window_height_;

	UINT64	frames_;
	UINT	rtv_index_;

	Microsoft::WRL::ComPtr<IDXGIFactory4>				_factory;
	Microsoft::WRL::ComPtr<IDXGIAdapter3>				_adapter;
	Microsoft::WRL::ComPtr<ID3D12Device>				_device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>			_commandQueue;
	HANDLE												_fenceEvent;
	Microsoft::WRL::ComPtr<ID3D12Fence>					_fence;
	Microsoft::WRL::ComPtr<IDXGISwapChain3>				_swapChain;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	_commnadList;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		_commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12Resource>				_renderTarget[RTV_NUM];
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		_dh_RTV;
	D3D12_CPU_DESCRIPTOR_HANDLE							_RTV_handle[RTV_NUM];
	Microsoft::WRL::ComPtr<ID3D12Resource>				_depthBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		_dh_DSV;
	D3D12_CPU_DESCRIPTOR_HANDLE							_DSV_handle;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>			_pipelineState;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>			_pipelineState2;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>			_rootSignature;
	D3D12_RECT											_scissorRect;
	D3D12_VIEWPORT										_viewPort;

	Plane*												_plane;
	Pmd*												_pmd;
	ShadowMapDebug*										_smDebug;
	VMDLoader*											_vmdLoader;
	VMDData*											_vmdData;
	Light*												_light;
	ShadowMapping*										_shadow;
	// RenderTexture用
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		_dh_renderTexSRV;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		_dh_renderTexRTV;
	Microsoft::WRL::ComPtr<ID3D12Resource>				_renderTexBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE							_renderTexRTV_handle;

	Microsoft::WRL::ComPtr<ID3D12Resource>				_boneMatBuffer;		// ボーン用

	D3D12Manager(HWND hwnd, int window_width, int window_height);
	D3D12Manager& operator=(const D3D12Manager& cpy);
	HRESULT CreateFactory();
	HRESULT CreateDevice();
	HRESULT CreateVisualCone();
	HRESULT CreateShadowVisualCone();
	HRESULT CreateCommandQueue();
	HRESULT CreateSwapChain();
	HRESULT CreateRenderTargetView();
	HRESULT CreateDepthStencilBuffer();
	HRESULT CreateCommandList();
	HRESULT CreateRootSignature();
	HRESULT CreateRenderTexture();
	HRESULT CreatePipelineStateObject();
	HRESULT CreatePipelineStateObject2();
	HRESULT WaitForPreviousFrame();
	HRESULT SetResourceBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES BeforeState, D3D12_RESOURCE_STATES AfterState);
	HRESULT PopulateCommandList(int boneIndex);

	HRESULT ExecuteCommandList();

	void Func(int indexNum);
	void DeformBones(unsigned int flameNo);

};

#endif

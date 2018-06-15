#include <fstream>
#include "D3D12Manager.h"
#include <tchar.h>
#include "d3dx12.h"
#include "WICTextureLoader12.h"
#include "VMDLoader.h"
#include "VMDData.h"
#include <algorithm>
#include "ShadowMapDebug.h"
#include "Pmd.h"
#include "Plane.h"
#include "pmdMesh.h"
#include "ShadowMapping.h"

using namespace DirectX;
using namespace Microsoft::WRL;


D3D12Manager* D3D12Manager::Instiate(HWND& hwnd, const int& windowWith, const int& windowHeight)
{
	static D3D12Manager* manager = new D3D12Manager(hwnd, windowWith, windowHeight);
	return manager;
}

D3D12Manager::D3D12Manager(HWND hwnd, int window_width, int window_height) :
	// ウィンドウハンドルの連結、画面のサイズを初期化
	window_handle_(hwnd), window_width_(window_width),
	window_height_(window_height), frames_{}, rtv_index_{}
{
	HRESULT hr;
	// ファクトリの生成
	CreateFactory();
	// デバイスの生成Finput
	CreateDevice();
	// コマンドキューの生成
	CreateCommandQueue();
	// スワップチェインの生成
	CreateSwapChain();
	// レンダーターゲットビューの生成
	CreateRenderTargetView();
	// コマンドリストの生成
	CreateCommandList();
	// レンダーテクスチャ作成
	CreateRenderTexture();
	// ルートシグネチャの生成
	CreateRootSignature();
	// PSOの生成
	CreatePipelineStateObject();
	CreatePipelineStateObject2();

	// シャドウマッピング周り
	_shadow = new ShadowMapping();
	_shadow->CreateShadowBuffers(_device.Get());
	_shadow->CreatePso(_device.Get(), _rootSignature.Get());

	// 深度ステンシルビューの生成
	CreateDepthStencilBuffer();
	
	// モーションデータ周り
	// PMD周り
	_vmdLoader = new VMDLoader();
	_pmd = new Pmd();
	_vmdData = _vmdLoader->LoadMotion("ModelData/motiontest.vmd");
	_pmd->Initialize(_device.Get(), _commnadList.Get(), _commandQueue.Get(), _commandAllocator.Get(), _pipelineState.Get(), frames_, _fenceEvent, _fence.Get(), _shadow->GetBuffer());

	// ViewPort,_scissorRectの設定
	CreateVisualCone();

	// HUD表示周り
	_smDebug = new ShadowMapDebug();
	_smDebug->Initialize(_device.Get());
	// 板ポリ周り
	_plane = new Plane();
	_plane->Initialize(_device.Get(), _rootSignature.Get(), _commnadList.Get(), _commandQueue.Get(), _commandAllocator.Get(), _pipelineState.Get(), frames_, _fenceEvent, _fence.Get(), _shadow->GetBuffer());
	// ライト周り
	_light = new Light();
	_light->CreateBuffer(_device.Get());
}

D3D12Manager::~D3D12Manager() {}

HRESULT D3D12Manager::CreateVisualCone()
{
	_viewPort.TopLeftX = 0.f;
	_viewPort.TopLeftY = 0.f;
	_viewPort.Width = (FLOAT)window_width_;
	_viewPort.Height = (FLOAT)window_height_;
	_viewPort.MinDepth = 0.f;
	_viewPort.MaxDepth = 1.f;

	_scissorRect.top = 0;
	_scissorRect.left = 0;
	_scissorRect.right = window_width_;
	_scissorRect.bottom = window_height_;

	return S_OK;
}

HRESULT D3D12Manager::CreateShadowVisualCone()
{
	_viewPort.TopLeftX = 0.f;
	_viewPort.TopLeftY = 0.f;
	// 光はワイドににゃらんのよ！
	_viewPort.Width = FLOAT((window_width_ + 0xff)&~0xff);
	_viewPort.Height = FLOAT((window_width_ + 0xff)&~0xff);
	_viewPort.MinDepth = 0.f;
	_viewPort.MaxDepth = 1.f;

	_scissorRect.top = 0;
	_scissorRect.left = 0;
	_scissorRect.right = FLOAT((window_width_ + 0xff)&~0xff);
	_scissorRect.bottom = FLOAT((window_width_ + 0xff)&~0xff);

	return S_OK;
}

//ファクトリの作成
HRESULT D3D12Manager::CreateFactory() {
	HRESULT hr{};
	UINT flag{};

	//デバッグモードの場合はデバッグレイヤーを有効にする
#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debug;
		hr = D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf()));
		if (FAILED(hr)) {
			return hr;
		}
		debug->EnableDebugLayer();
	}

	flag |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	//ファクトリの作成
	hr = CreateDXGIFactory2(flag, IID_PPV_ARGS(_factory.GetAddressOf()));

	return hr;
}


HRESULT D3D12Manager::CreateDevice() {
	HRESULT hr{};

	//最初に見つかったアダプターを使用する
	hr = _factory->EnumAdapters(0, (IDXGIAdapter**)_adapter.GetAddressOf());

	if (SUCCEEDED(hr)) {
		//見つかったアダプタを使用してデバイスを作成する
		hr = D3D12CreateDevice(_adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&_device));
	}
	if (FAILED(hr)) {
		return hr;
	}

	return hr;
}

HRESULT D3D12Manager::CreateCommandQueue() {
	HRESULT hr{};
	D3D12_COMMAND_QUEUE_DESC command_queue_desc{};

	// コマンドキューを生成.
	command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	command_queue_desc.Priority = 0;
	command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	command_queue_desc.NodeMask = 0;
	hr = _device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(_commandQueue.GetAddressOf()));
	if (FAILED(hr)) {
		return hr;
	}

	//コマンドキュー用のフェンスの生成
	_fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	if (_fenceEvent == NULL) {
		return E_FAIL;
	}
	hr = _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.GetAddressOf()));

	return hr;
}

HRESULT D3D12Manager::CreateSwapChain() {
	HRESULT hr{};
	DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
	ComPtr<IDXGISwapChain> swap_chain{};

	swap_chain_desc.BufferDesc.Width = window_width_;
	swap_chain_desc.BufferDesc.Height = window_height_;
	swap_chain_desc.OutputWindow = window_handle_;
	swap_chain_desc.Windowed = TRUE;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount = RTV_NUM;//裏と表で２枚作りたいので2を指定する
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swap_chain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;


	hr = _factory->CreateSwapChain(_commandQueue.Get(), &swap_chain_desc, swap_chain.GetAddressOf());
	if (FAILED(hr)) {
		return hr;
	}

	hr = swap_chain->QueryInterface(_swapChain.GetAddressOf());
	if (FAILED(hr)) {
		return hr;
	}

	//カレントのバックバッファのインデックスを取得する
	rtv_index_ = _swapChain->GetCurrentBackBufferIndex();

	return S_OK;
}

HRESULT D3D12Manager::CreateRenderTargetView() {
	HRESULT hr{};
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};

	//RTV用デスクリプタヒープの作成
	heap_desc.NumDescriptors = RTV_NUM;
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heap_desc.NodeMask = 0;
	hr = _device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(_dh_RTV.GetAddressOf()));
	if (FAILED(hr)) {
		return hr;
	}

	//レンダーターゲットを作成する処理
	UINT size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (UINT i = 0; i < RTV_NUM; ++i) {
		//スワップチェインからバッファを受け取る
		hr = _swapChain->GetBuffer(i, IID_PPV_ARGS(&_renderTarget[i]));
		if (FAILED(hr)) {
			return hr;
		}

		//RenderTargetViewを作成してヒープデスクリプタに登録
		_RTV_handle[i] = _dh_RTV->GetCPUDescriptorHandleForHeapStart();
		_RTV_handle[i].ptr += size * i;
		_device->CreateRenderTargetView(_renderTarget[i].Get(), nullptr, _RTV_handle[i]);
	}

	return hr;
}

HRESULT D3D12Manager::CreateDepthStencilBuffer() {
	HRESULT hr;

	//深度バッファ用のデスクリプタヒープの作成
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc{};
	descriptor_heap_desc.NumDescriptors = 1;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descriptor_heap_desc.NodeMask = 0;
	hr = _device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&_dh_DSV));
	if (FAILED(hr)) {
		return hr;
	}


	//深度バッファの作成
	D3D12_HEAP_PROPERTIES heap_properties{};
	D3D12_RESOURCE_DESC resource_desc{};
	D3D12_CLEAR_VALUE clear_value{};

	heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask = 0;
	heap_properties.VisibleNodeMask = 0;

	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resource_desc.Width = window_width_;
	resource_desc.Height = window_height_;
	resource_desc.DepthOrArraySize = 1;
	resource_desc.MipLevels = 0;
	resource_desc.Format = DXGI_FORMAT_R32_TYPELESS;
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource_desc.SampleDesc.Count = 1;
	resource_desc.SampleDesc.Quality = 0;
	resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	resource_desc.Alignment = 0;

	clear_value.Format = DXGI_FORMAT_D32_FLOAT;
	clear_value.DepthStencil.Depth = 1.0f;
	clear_value.DepthStencil.Stencil = 0;

	hr = _device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, IID_PPV_ARGS(&_depthBuffer));
	if (FAILED(hr)) {
		return hr;
	}


	//深度バッファのビューの作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};

	dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
	dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv_desc.Texture2D.MipSlice = 0;
	dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

	_DSV_handle = _dh_DSV->GetCPUDescriptorHandleForHeapStart();

	_device->CreateDepthStencilView(_depthBuffer.Get(), &dsv_desc, _DSV_handle);

	return S_OK;
}

HRESULT D3D12Manager::CreateCommandList() {
	HRESULT hr;

	//コマンドアロケータの作成
	hr = _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator));
	if (FAILED(hr)) {
		return hr;
	}

	//コマンドアロケータとバインドしてコマンドリストを作成する
	hr = _device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.Get(), nullptr, IID_PPV_ARGS(&_commnadList));

	return hr;
}


HRESULT D3D12Manager::CreateRootSignature() {
	HRESULT hr{};

	D3D12_DESCRIPTOR_RANGE		range[2]{};				// DiscriptorTableが持っている 複数持っている もちろんレジスタの種類が変わればrangeも変わってくる
	D3D12_ROOT_PARAMETER		root_parameters[7]{};	// ルートシグネチャを作成する前に設定しなけばならないパラメーター
	D3D12_ROOT_SIGNATURE_DESC	root_signature_desc{};  // ルートシグネチャの設定
	D3D12_STATIC_SAMPLER_DESC	sampler_desc[2]{};
	ComPtr<ID3DBlob> blob{};

	root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[0].Descriptor.ShaderRegister = 0;
	root_parameters[0].Descriptor.RegisterSpace = 0;

	root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[1].Descriptor.ShaderRegister = 1;
	root_parameters[1].Descriptor.RegisterSpace = 0;

	root_parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[2].Descriptor.ShaderRegister = 2;
	root_parameters[2].Descriptor.RegisterSpace = 0;

	range[1].NumDescriptors = 1;
	range[1].BaseShaderRegister = 3;// レジスタ番号
	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	range[1].OffsetInDescriptorsFromTableStart = 0;

	root_parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[3].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[3].DescriptorTable.pDescriptorRanges = &range[1];


	//テクスチャのデスクリプタヒープを渡すように設定
	//テーブルに入れるからrangeいるよん
	range[0].NumDescriptors = 2;
	range[0].BaseShaderRegister = 0;// レジスタ番号
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	root_parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	root_parameters[4].DescriptorTable.NumDescriptorRanges = 1;
	root_parameters[4].DescriptorTable.pDescriptorRanges = &range[0];

	//ライト用の定数バッファ
	root_parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[5].Descriptor.ShaderRegister = 4;
	root_parameters[5].Descriptor.RegisterSpace = 0;

	//ボーン用バッファ
	root_parameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	root_parameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	root_parameters[6].Descriptor.ShaderRegister = 5;
	root_parameters[6].Descriptor.RegisterSpace = 0;

	//静的サンプラの設定
	sampler_desc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler_desc[0].MipLODBias = 0.0f;
	sampler_desc[0].MaxAnisotropy = 16;
	sampler_desc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler_desc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler_desc[0].MinLOD = 0.0f;
	sampler_desc[0].MaxLOD = D3D12_FLOAT32_MAX;
	sampler_desc[0].ShaderRegister = 0;
	sampler_desc[0].RegisterSpace = 0;
	sampler_desc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	sampler_desc[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler_desc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler_desc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler_desc[1].MipLODBias = 0.0f;
	sampler_desc[1].MaxAnisotropy = 16;
	sampler_desc[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler_desc[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler_desc[1].MinLOD = 0.0f;
	sampler_desc[1].MaxLOD = D3D12_FLOAT32_MAX;
	sampler_desc[1].ShaderRegister = 1;
	sampler_desc[1].RegisterSpace = 0;
	sampler_desc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


	root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	root_signature_desc.NumParameters = _countof(root_parameters);
	root_signature_desc.pParameters = root_parameters;
	root_signature_desc.NumStaticSamplers = _countof(sampler_desc);
	root_signature_desc.pStaticSamplers = sampler_desc;

	// RootSignaturを作成するのに必要なバッファを確保しTableの情報をシリアライズ
	hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
	if (FAILED(hr)) {
		return hr;
	}
	// RootSignaturの作成
	hr = _device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&_rootSignature));

	return hr;
}


HRESULT D3D12Manager::CreatePipelineStateObject() {
	HRESULT hr;
	ComPtr<ID3DBlob> vertex_shader{};
	ComPtr<ID3DBlob> pixel_shader{};

#if defined(_DEBUG)
	UINT compile_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compile_flag = 0;
#endif

	hr = D3DCompileFromFile(_T("normalShader.hlsl"), nullptr, nullptr, "VSMain", "vs_5_0", compile_flag, 0, vertex_shader.GetAddressOf(), nullptr);
	if (FAILED(hr)) {
		return hr;
	}

	hr = D3DCompileFromFile(_T("normalShader.hlsl"), nullptr, nullptr, "PSMain", "ps_5_0", compile_flag, 0, pixel_shader.GetAddressOf(), nullptr);
	if (FAILED(hr)) {
		return hr;
	}


	// 頂点レイアウト.
	D3D12_INPUT_ELEMENT_DESC InputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCODE",    0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONENO", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },			// ボーン情報
		{ "WEIGHT", 0, DXGI_FORMAT_R8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }			// 影響度
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};

	//シェーダーの設定
	pipeline_state_desc.VS.pShaderBytecode = vertex_shader->GetBufferPointer();
	pipeline_state_desc.VS.BytecodeLength = vertex_shader->GetBufferSize();
	pipeline_state_desc.PS.pShaderBytecode = pixel_shader->GetBufferPointer();
	pipeline_state_desc.PS.BytecodeLength = pixel_shader->GetBufferSize();


	//インプットレイアウトの設定
	pipeline_state_desc.InputLayout.pInputElementDescs = InputElementDesc;
	pipeline_state_desc.InputLayout.NumElements = _countof(InputElementDesc);


	//サンプル系の設定
	pipeline_state_desc.SampleDesc.Count = 1;
	pipeline_state_desc.SampleDesc.Quality = 0;
	pipeline_state_desc.SampleMask = UINT_MAX;

	//レンダーターゲットの設定
	pipeline_state_desc.NumRenderTargets = 1;
	pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;


	//三角形に設定
	pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//ルートシグネチャ
	pipeline_state_desc.pRootSignature = _rootSignature.Get();


	//ラスタライザステートの設定
	pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//D3D12_CULL_MODE_BACK;
	pipeline_state_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pipeline_state_desc.RasterizerState.FrontCounterClockwise = FALSE;
	pipeline_state_desc.RasterizerState.DepthBias = 0;
	pipeline_state_desc.RasterizerState.DepthBiasClamp = 0;
	pipeline_state_desc.RasterizerState.SlopeScaledDepthBias = 0;
	pipeline_state_desc.RasterizerState.DepthClipEnable = TRUE;
	pipeline_state_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	pipeline_state_desc.RasterizerState.AntialiasedLineEnable = FALSE;
	pipeline_state_desc.RasterizerState.MultisampleEnable = FALSE;


	//ブレンドステートの設定
	for (int i = 0; i < _countof(pipeline_state_desc.BlendState.RenderTarget); ++i) {
		pipeline_state_desc.BlendState.RenderTarget[i].BlendEnable = FALSE;
		pipeline_state_desc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		pipeline_state_desc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
		pipeline_state_desc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		pipeline_state_desc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		pipeline_state_desc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		pipeline_state_desc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		pipeline_state_desc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pipeline_state_desc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
		pipeline_state_desc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
	}
	pipeline_state_desc.BlendState.AlphaToCoverageEnable = FALSE;
	pipeline_state_desc.BlendState.IndependentBlendEnable = FALSE;


	//デプスステンシルステートの設定
	pipeline_state_desc.DepthStencilState.DepthEnable = TRUE;								//深度テストあり
	pipeline_state_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pipeline_state_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pipeline_state_desc.DepthStencilState.StencilEnable = FALSE;							//ステンシルテストなし
	pipeline_state_desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	pipeline_state_desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	pipeline_state_desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	pipeline_state_desc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	pipeline_state_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	hr = _device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&_pipelineState));
	if (FAILED(hr)) {
		return hr;
	}
	return hr;
}

HRESULT D3D12Manager::CreatePipelineStateObject2() {
	HRESULT hr;
	ComPtr<ID3DBlob> vertex_shader{};
	ComPtr<ID3DBlob> pixel_shader{};

#if defined(_DEBUG)
	UINT compile_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compile_flag = 0;
#endif

	hr = D3DCompileFromFile(_T("normalShader.hlsl"), nullptr, nullptr, "VSMain2", "vs_5_0", compile_flag, 0, vertex_shader.GetAddressOf(), nullptr);
	if (FAILED(hr)) {
		return hr;
	}

	hr = D3DCompileFromFile(_T("normalShader.hlsl"), nullptr, nullptr, "PSMain", "ps_5_0", compile_flag, 0, pixel_shader.GetAddressOf(), nullptr);
	if (FAILED(hr)) {
		return hr;
	}


	// 頂点レイアウト.
	D3D12_INPUT_ELEMENT_DESC InputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCODE",    0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT , D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONENO", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },			// ボーン情報
		{ "WEIGHT", 0, DXGI_FORMAT_R8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }			// 影響度
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc{};

	//シェーダーの設定
	pipeline_state_desc.VS.pShaderBytecode = vertex_shader->GetBufferPointer();
	pipeline_state_desc.VS.BytecodeLength = vertex_shader->GetBufferSize();
	pipeline_state_desc.PS.pShaderBytecode = pixel_shader->GetBufferPointer();
	pipeline_state_desc.PS.BytecodeLength = pixel_shader->GetBufferSize();


	//インプットレイアウトの設定
	pipeline_state_desc.InputLayout.pInputElementDescs = InputElementDesc;
	pipeline_state_desc.InputLayout.NumElements = _countof(InputElementDesc);


	//サンプル系の設定
	pipeline_state_desc.SampleDesc.Count = 1;
	pipeline_state_desc.SampleDesc.Quality = 0;
	pipeline_state_desc.SampleMask = UINT_MAX;

	//レンダーターゲットの設定
	pipeline_state_desc.NumRenderTargets = 1;
	pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;


	//三角形に設定
	pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//ルートシグネチャ
	pipeline_state_desc.pRootSignature = _rootSignature.Get();


	//ラスタライザステートの設定
	pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//D3D12_CULL_MODE_BACK;
	pipeline_state_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pipeline_state_desc.RasterizerState.FrontCounterClockwise = FALSE;
	pipeline_state_desc.RasterizerState.DepthBias = 0;
	pipeline_state_desc.RasterizerState.DepthBiasClamp = 0;
	pipeline_state_desc.RasterizerState.SlopeScaledDepthBias = 0;
	pipeline_state_desc.RasterizerState.DepthClipEnable = TRUE;
	pipeline_state_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	pipeline_state_desc.RasterizerState.AntialiasedLineEnable = FALSE;
	pipeline_state_desc.RasterizerState.MultisampleEnable = FALSE;


	//ブレンドステートの設定
	for (int i = 0; i < _countof(pipeline_state_desc.BlendState.RenderTarget); ++i) {
		pipeline_state_desc.BlendState.RenderTarget[i].BlendEnable = FALSE;
		pipeline_state_desc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		pipeline_state_desc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
		pipeline_state_desc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		pipeline_state_desc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		pipeline_state_desc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		pipeline_state_desc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		pipeline_state_desc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pipeline_state_desc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
		pipeline_state_desc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
	}
	pipeline_state_desc.BlendState.AlphaToCoverageEnable = FALSE;
	pipeline_state_desc.BlendState.IndependentBlendEnable = FALSE;


	//デプスステンシルステートの設定
	pipeline_state_desc.DepthStencilState.DepthEnable = TRUE;								//深度テストあり
	pipeline_state_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pipeline_state_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pipeline_state_desc.DepthStencilState.StencilEnable = FALSE;							//ステンシルテストなし
	pipeline_state_desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	pipeline_state_desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	pipeline_state_desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	pipeline_state_desc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	pipeline_state_desc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	pipeline_state_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	hr = _device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&_pipelineState2));
	if (FAILED(hr)) {
		return hr;
	}
	return hr;
}

HRESULT D3D12Manager::WaitForPreviousFrame() {
	HRESULT hr;

	const UINT64 fence = frames_;
	hr = _commandQueue->Signal(_fence.Get(), fence);
	if (FAILED(hr)) {
		return -1;
	}
	++frames_;

	if (_fence->GetCompletedValue() < fence) {
		hr = _fence->SetEventOnCompletion(fence, _fenceEvent);
		if (FAILED(hr)) {
			return -1;
		}

		WaitForSingleObject(_fenceEvent, INFINITE);
	}
	return S_OK;
}

//リソースの設定変更用
HRESULT D3D12Manager::SetResourceBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES BeforeState, D3D12_RESOURCE_STATES AfterState) {
	D3D12_RESOURCE_BARRIER resource_barrier{};

	resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resource_barrier.Transition.pResource = resource;
	resource_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	resource_barrier.Transition.StateBefore = BeforeState;
	resource_barrier.Transition.StateAfter = AfterState;

	_commnadList->ResourceBarrier(1, &resource_barrier);
	return S_OK;
}


HRESULT D3D12Manager::PopulateCommandList(int boneIndex) {
	HRESULT hr;

	FLOAT clear_color[4] = { 1.0f, 0.5f, 0.5f, 1.0f };
	_plane->Update();

	// テストとしてテクスチャへのレンダリングを行う
	//リソースの状態をプレゼント用からレンダーターゲット用に変更
	// いやもうすでにレンダーターゲットとして用意してますんで ^ ^
	SetResourceBarrier(_renderTexBuffer.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

	CreateVisualCone();

	//深度バッファとレンダーターゲットのクリア
	_commnadList->ClearDepthStencilView(_dh_DSV->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	_commnadList->ClearRenderTargetView(_renderTexRTV_handle, clear_color, 0, nullptr);

	//ルートシグネチャとPSOの設定
	// グラフィックスパイプラインのRootSignatureを設定
	_commnadList->SetGraphicsRootSignature(_rootSignature.Get());
	_commnadList->SetPipelineState(_plane->GetPso());

	//ビューポートとシザー矩形の設定
	_commnadList->RSSetViewports(1, &_viewPort);
	_commnadList->RSSetScissorRects(1, &_scissorRect);

	//レンダーターゲットの設定
	_commnadList->OMSetRenderTargets(1, &_renderTexRTV_handle, TRUE, &_dh_DSV->GetCPUDescriptorHandleForHeapStart());

	//ライトの設定
	_commnadList->SetGraphicsRootConstantBufferView(5, _light->GetBuffer()->GetGPUVirtualAddress());
	// ボーン行列
	_commnadList->SetGraphicsRootConstantBufferView(6, _boneMatBuffer->GetGPUVirtualAddress());

	// 板さんの描画
	_plane->Draw(_commnadList.Get());

	// ミクさんの描画
	_commnadList->SetPipelineState(_pipelineState.Get());
	_pmd->Draw(_commnadList.Get());

	// 確認用ミニマップの描画
	/*_commnadList->SetGraphicsRootSignature(_smDebug->GetSignature());
	_commnadList->SetPipelineState(_smDebug->GetPso());
	_smDebug->Draw(_commnadList.Get(), _shadow->GetShadowTexture());*/


	//だんだんリソースバリアがわかっていく
	SetResourceBarrier(_renderTexBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	hr = _commnadList->Close();
	ExecuteCommandList();

	// シャドウ用コマンド ============================================================
	// シャドーマップ用のリソースを深度書き込みに設定
	// 深度値を書き込みます
	// 深度値さえあればいいのでDepthStecilviewをOMにセットしてあげます(Clearも忘れずに)

	SetResourceBarrier(_shadow->GetBuffer(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	CreateShadowVisualCone();

	//シャドーマップ用の深度バッファをクリア
	_commnadList->ClearDepthStencilView(_shadow->GetShadowBuffer()->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


	//ルートシグネチャとPSOの設定
	_commnadList->SetGraphicsRootSignature(_rootSignature.Get());
	_commnadList->SetPipelineState(_shadow->GetPso());


	//ビューポートとシザー矩形の設定
	_commnadList->RSSetViewports(1, _light->GetViewPort());
	_commnadList->RSSetScissorRects(1, _light->GetScissorRect());


	//深度バッファの設定
	//シャドウマップとして使うテクスチャは「深度バッファ」そのものをSRVとするので
	//描画ターゲットは不要です
	_commnadList->OMSetRenderTargets(0, nullptr, FALSE, &_shadow->GetShadowBuffer()->GetCPUDescriptorHandleForHeapStart());

	//ライトの設定
	_commnadList->SetGraphicsRootConstantBufferView(5, _light->GetBuffer()->GetGPUVirtualAddress());
	// ボーン行列
	_commnadList->SetGraphicsRootConstantBufferView(6, _boneMatBuffer->GetGPUVirtualAddress());


	//板ポリの描画
	_plane->Draw(_commnadList.Get());
	_pmd->Draw(_commnadList.Get());

	//シャドーマップ用のリソースを読み込みに変更する
	SetResourceBarrier(_shadow->GetBuffer(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);

	hr = _commnadList->Close();
	ExecuteCommandList();


	// MainDisplay用コマンド=============================================================

	//リソースの状態をプレゼント用からレンダーターゲット用に変更
	SetResourceBarrier(_renderTarget[rtv_index_].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	CreateVisualCone();

	//深度バッファとレンダーターゲットのクリア
	_commnadList->ClearDepthStencilView(_dh_DSV->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	_commnadList->ClearRenderTargetView(_RTV_handle[rtv_index_], clear_color, 0, nullptr);

	//ルートシグネチャとPSOの設定
	// グラフィックスパイプラインのRootSignatureを設定
	_commnadList->SetGraphicsRootSignature(_rootSignature.Get());
	_commnadList->SetPipelineState(_plane->GetPso());

	//ビューポートとシザー矩形の設定
	_commnadList->RSSetViewports(1, &_viewPort);
	_commnadList->RSSetScissorRects(1, &_scissorRect);

	//レンダーターゲットの設定
	_commnadList->OMSetRenderTargets(1, &_RTV_handle[rtv_index_], TRUE, &_dh_DSV->GetCPUDescriptorHandleForHeapStart());

	//ライトの設定
	_commnadList->SetGraphicsRootConstantBufferView(5, _light->GetBuffer()->GetGPUVirtualAddress());
	// ボーン行列
	_commnadList->SetGraphicsRootConstantBufferView(6, _boneMatBuffer->GetGPUVirtualAddress());

	// 板さんの描画
	_plane->Draw(_commnadList.Get());

	// ミクさんの描画
	_commnadList->SetPipelineState(_pipelineState.Get());
	_pmd->Draw(_commnadList.Get());

	// 確認用ミニマップの描画
	_commnadList->SetGraphicsRootSignature(_smDebug->GetSignature());
	_commnadList->SetPipelineState(_smDebug->GetPso());
	_smDebug->Draw(_commnadList.Get(), _dh_renderTexSRV.Get());

	SetResourceBarrier(_renderTarget[rtv_index_].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	// 読み込んだレンダーテクスチャをRTVに戻す
	SetResourceBarrier(_renderTexBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_GENERIC_READ);

	hr = _commnadList->Close();
	ExecuteCommandList();

	return hr;
}

HRESULT D3D12Manager::CreateRenderTexture()
{
	HRESULT hr;
	// リソース作成
	D3D12_HEAP_PROPERTIES heap_properties{};
	D3D12_RESOURCE_DESC resource_desc{};

	heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask = 0;
	heap_properties.VisibleNodeMask = 0;

	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resource_desc.Alignment = 0;
	resource_desc.Width = 640U;
	resource_desc.Height = 480U;
	resource_desc.DepthOrArraySize = 1;
	resource_desc.MipLevels = 1;
	resource_desc.Format = DXGI_FORMAT_B8G8R8A8_TYPELESS;
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource_desc.SampleDesc.Count = 1;
	resource_desc.SampleDesc.Quality = 0;
	resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clear_value{};
	FLOAT clear_color[4] = { 1.0f, 0.5f, 0.5f, 1.0f };
	clear_value.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	clear_value.Color[0] = clear_color[0];
	clear_value.Color[1] = clear_color[1];
	clear_value.Color[2] = clear_color[2];
	clear_value.Color[3] = clear_color[3];
	clear_value.DepthStencil.Depth = 1.0f;
	clear_value.DepthStencil.Stencil = 0;

	hr = _device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, &clear_value, IID_PPV_ARGS(&_renderTexBuffer));
	if (FAILED(hr)) {
		return hr;
	}

	// Heap作成
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc{};
	descriptor_heap_desc.NumDescriptors = 1;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptor_heap_desc.NodeMask = 0;
	hr = _device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&_dh_renderTexSRV));
	if (FAILED(hr)) {
		return hr;
	}

	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = _device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&_dh_renderTexRTV));
	if (FAILED(hr)) {
		return hr;
	}

	D3D12_RENDER_TARGET_VIEW_DESC render_desc{};
	render_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	render_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;  //レンダーターゲットリソースへのアクセス方法を指定します。
	render_desc.Texture2D.MipSlice = 0;							//使用できるみっぷマップレベルのインデックス
	render_desc.Texture2D.PlaneSlice = 0;						//テクスチャで使用する平面のインデックス
	render_desc.Buffer.FirstElement = 0;						// アクセスするバッファの設定
	render_desc.Buffer.NumElements = 1;

	_renderTexRTV_handle = _dh_renderTexRTV.Get()->GetCPUDescriptorHandleForHeapStart();
	_device->CreateRenderTargetView(_renderTexBuffer.Get(), &render_desc, _renderTexRTV_handle);

	D3D12_SHADER_RESOURCE_VIEW_DESC resourct_view_desc{};
	resourct_view_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	resourct_view_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	resourct_view_desc.Texture2D.MipLevels = 1;
	resourct_view_desc.Texture2D.MostDetailedMip = 0;
	resourct_view_desc.Texture2D.PlaneSlice = 0;
	resourct_view_desc.Texture2D.ResourceMinLODClamp = 0.0F;
	resourct_view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	_device->CreateShaderResourceView(_renderTexBuffer.Get(), &resourct_view_desc, _dh_renderTexSRV->GetCPUDescriptorHandleForHeapStart());

	return hr;
}

HRESULT D3D12Manager::Render() {
	HRESULT hr;

	// =======================
	// コマンドリストで頂点や定数などの設定をして
	// コマンドアロケータでコマンドリストをバッファとして蓄えて
	// コマンドキューの積むことで遅延実行される
	// =======================
	static int i = 0;
	if (i++ > 60*2){ i = 0; }
	DeformBones(i/2);

	//std::fill(_pmd->GetPmdMesh()->BoneMatrix().begin(), _pmd->GetPmdMesh()->BoneMatrix().end(), XMMatrixIdentity());
	//_pmd->CcdikSolve(_pmd->GetPmdMesh()->IkMap(), "右足ＩＫ先", XMFLOAT3(-0.3f,-0.6f, -1.0f));
	//_pmd->CcdikSolve(_pmd->GetPmdMesh()->IkMap(), "左足ＩＫ先", XMFLOAT3(0.3f, -0.6f, -1.0f));

	//Func(0);

	D3D12_HEAP_PROPERTIES heapProperties{};
	D3D12_RESOURCE_DESC   resourceDesc{};

	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeof(float)*16*256;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;// アンチェリ関連のはず

	hr = _device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_boneMatBuffer));
	if (FAILED(hr))
	{
		return hr;
	}
	XMMATRIX *matBuffer{};

	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;



	hr = _boneMatBuffer->Map(0, nullptr, (void**)&matBuffer);
	if (FAILED(hr)) {
		return hr;
	}

	memcpy(&matBuffer[0], &_pmd->GetPmdMesh()->BoneMatrix()[0], 256 * sizeof(XMMATRIX));
	_boneMatBuffer->Unmap(0, nullptr);
	matBuffer = nullptr;

	PopulateCommandList(i);

	hr = _swapChain->Present(1, 0);
	if (FAILED(hr)) {
		return hr;
	}


	//カレントのバックバッファのインデックスを取得する
	rtv_index_ = _swapChain->GetCurrentBackBufferIndex();

	return S_OK;
}


HRESULT D3D12Manager::ExecuteCommandList() {
	HRESULT hr;

	ID3D12CommandList *const command_lists = _commnadList.Get();
	_commandQueue->ExecuteCommandLists(1, &command_lists);

	//実行したコマンドの終了待ち
	WaitForPreviousFrame();


	hr = _commandAllocator->Reset();
	if (FAILED(hr)) {
		return hr;
	}

	hr = _commnadList->Reset(_commandAllocator.Get(), nullptr);
	if (FAILED(hr)) {
		return hr;
	}

	return hr;
}


// ボーン変形
void D3D12Manager::Func(int indexNum)
{

	XMMATRIX bone = _pmd->GetPmdMesh()->BoneMatrix()[indexNum];

	std::vector<BoneNode>& nodes = _pmd->GetPmdMesh()->BoneNode();
	// 子に伝達
	for (auto& idx : nodes[indexNum].childIdx)
	{
		XMMATRIX mat = XMMatrixIdentity();
		mat = _pmd->GetPmdMesh()->BoneMatrix()[idx];
		mat = mat*bone;
		_pmd->GetPmdMesh()->BoneMatrix()[idx] = mat;
		Func(idx);
	}
}

void D3D12Manager::DeformBones(unsigned int flameNo)
{

	std::fill(_pmd->GetPmdMesh()->BoneMatrix().begin(), _pmd->GetPmdMesh()->BoneMatrix().end(), XMMatrixIdentity());

	//std::sort(data->GetMotionKeyflameData().begin(), data->GetMotionKeyflameData().end(), [](MotionData& a, MotionData&b) {return a.flameNo < b.flameNo; });


	float t = 0.0f;
	for (auto& vmddatas : _vmdData->GetMotionKeyflameData())
	{
		auto revit = std::find_if(vmddatas.second.rbegin(),
			vmddatas.second.rend(),
			[flameNo](const MotionData& md)
		{
			return md.flameNo <= flameNo;
			//return flameNo == md.flameNo;
		});

		//nextをとる、補完値Tを計算
		auto it = revit.base();
		if (it != vmddatas.second.end())
		{
			t = ((float)flameNo - (float)revit->flameNo) /
				((float)it->flameNo - (float)revit->flameNo);
		}
		int idx = _pmd->GetPmdMesh()->BoneMap()[vmddatas.first];
		for (auto& at : vmddatas.second)
		{
			if (it == vmddatas.second.end())
			{
				continue;
			}
			if (revit == vmddatas.second.rend())
			{
				continue;
			}
			//int parts = mesh->GetBoneMap()[framebone.first];
			XMVECTOR offset = XMLoadFloat3(&_pmd->GetPmdMesh()->Bone()[idx].headPos);
			// ワールドの原点に戻すための行列
			XMMATRIX rotMat = XMMatrixTranslationFromVector(-offset);
			//rotMat *= XMMatrixRotationQuaternion(revit->quaternion);//XMMatrixRotationZ(XM_PIDIV2);
			rotMat *= XMMatrixRotationQuaternion(XMQuaternionSlerp(revit->quaternion, it->quaternion, t));//XMMatrixRotationZ(XM_PIDIV2);
			// ローカル座標に戻す
			rotMat *= XMMatrixTranslationFromVector(offset);
			// 結果を入れる
			_pmd->GetPmdMesh()->BoneMatrix()[idx] = rotMat;
		}
	}

	Func(0);
}
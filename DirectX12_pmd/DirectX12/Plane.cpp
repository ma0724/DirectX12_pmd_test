#include "Plane.h"
#include "D3D12Manager.h"
#include <fstream>
#include <tchar.h>
#include "WICTextureLoader12.h"
#include "d3dx12.h"

using namespace DirectX;
using namespace Microsoft::WRL;

Plane::Plane() :
	_vertexBuffer{},
	_constantBuffer{},
	_texture{},
	_dh_texture{}
{

}

HRESULT Plane::Initialize(ID3D12Device *device, ID3D12RootSignature* inSignature, ID3D12GraphicsCommandList *command_list, ID3D12CommandQueue* inCmdQueue, ID3D12CommandAllocator* inCmdAlloc, ID3D12PipelineState* inPiplineState, UINT64& inFlames, HANDLE& inFenceEvent, ID3D12Fence* inFnece, ID3D12Resource *shadowResource) {
	HRESULT hr{};
	D3D12_HEAP_PROPERTIES heap_properties{};
	D3D12_RESOURCE_DESC   resource_desc{};

	heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask = 0;
	heap_properties.VisibleNodeMask = 0;

	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resource_desc.Width = 256;
	resource_desc.Height = 1;
	resource_desc.DepthOrArraySize = 1;
	resource_desc.MipLevels = 1;
	resource_desc.Format = DXGI_FORMAT_UNKNOWN;
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resource_desc.SampleDesc.Count = 1;
	resource_desc.SampleDesc.Quality = 0;

	//頂点バッファの作成
	hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_vertexBuffer));
	if (FAILED(hr)) {
		return hr;
	}

	D3D12_HEAP_PROPERTIES heapProperties{};
	D3D12_RESOURCE_DESC   resourceDesc{};

	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = 256;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;// アンチェリ関連のはず

	// ①定数バッファの作成(合成行列用)
	{
		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBuffer));
		if (FAILED(hr))
		{
			return hr;
		}
	}
	// ②定数バッファの作成(World用)
	{
		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBufferWorld));
		if (FAILED(hr))
		{
			return hr;
		}
	}
	// ③定数バッファの作成(View * Projection用)
	{
		hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_constantBufferViewProjection));
		if (FAILED(hr))
		{
			return hr;
		}
	}

	heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask = 0;
	heap_properties.VisibleNodeMask = 0;

	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resource_desc.Width = 256;
	resource_desc.Height = 1;
	resource_desc.DepthOrArraySize = 1;
	resource_desc.MipLevels = 1;
	resource_desc.Format = DXGI_FORMAT_UNKNOWN;
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resource_desc.SampleDesc.Count = 1;
	resource_desc.SampleDesc.Quality = 0;

	// バインドのみ
	hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_nullBuffer));
	if (FAILED(hr)) {
		return hr;
	}
	D3D12_DESCRIPTOR_HEAP_DESC dh_mt_desc;
	dh_mt_desc.NumDescriptors = 1;	// デスクリプタの数
	dh_mt_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//今回はシェーダリソースで使うよ
	dh_mt_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//可視化
	dh_mt_desc.NodeMask = 0;

	hr = device->CreateDescriptorHeap(&dh_mt_desc, IID_PPV_ARGS(&_dh_nullBuff));

	// コンスタントバッファビューを作成
	D3D12_CPU_DESCRIPTOR_HANDLE handleMatCBV{};
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
	cbv_desc.BufferLocation = _nullBuffer.Get()->GetGPUVirtualAddress();
	unsigned int dhSize = device->GetDescriptorHandleIncrementSize(dh_mt_desc.Type);//デスクリプタのサイズ
	handleMatCBV = _dh_nullBuff.Get()->GetCPUDescriptorHandleForHeapStart();
	device->CreateConstantBufferView(&cbv_desc, handleMatCBV);


	hr = device->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_nullBuffer2));
	if (FAILED(hr)) {
		return hr;
	}

	hr = device->CreateDescriptorHeap(&dh_mt_desc, IID_PPV_ARGS(&_dh_nullBuff2));

	cbv_desc.BufferLocation = _nullBuffer2.Get()->GetGPUVirtualAddress();
	dhSize = device->GetDescriptorHandleIncrementSize(dh_mt_desc.Type);//デスクリプタのサイズ
	handleMatCBV = _dh_nullBuff2.Get()->GetCPUDescriptorHandleForHeapStart();
	device->CreateConstantBufferView(&cbv_desc, handleMatCBV);

	//頂点データの書き込み
	Vertex3D *buffer{};
	hr = _vertexBuffer->Map(0, nullptr, (void**)&buffer);
	if (FAILED(hr)) {
		return hr;
	}
	buffer[0].Position = { -80.0f,  0.0f, 80.0f };
	buffer[1].Position = { 80.0f,  0.0f, 80.0f };
	buffer[2].Position = { -80.0f, 0.0f, -40.0f };
	buffer[3].Position = { 80.0f, 0.0f, -40.0f };
	buffer[0].Normal = { 0.0f, 1.0f, 0.0f };
	buffer[1].Normal = { 0.0f, 1.0f, 0.0f };
	buffer[2].Normal = { 0.0f, 1.0f, 0.0f };
	buffer[3].Normal = { 0.0f, 1.0f, 0.0f };
	buffer[0].UV = { 0.0f, 0.0f };
	buffer[1].UV = { 1.0f, 0.0f };
	buffer[2].UV = { 0.0f, 1.0f };
	buffer[3].UV = { 1.0f, 1.0f };

	buffer[0].Tangent = { 1.0f, 0.0f, 0.0f };
	buffer[0].Binormal = { 0.0f, 1.0f, 0.0f };
	buffer[1].Tangent = { 1.0f, 0.0f, 0.0f };
	buffer[1].Binormal = { 0.0f, 1.0f, 0.0f };
	buffer[2].Tangent = { 1.0f, 0.0f, 0.0f };
	buffer[2].Binormal = { 0.0f, 1.0f, 0.0f };
	buffer[3].Tangent = { 1.0f, 0.0f, 0.0f };
	buffer[3].Binormal = { 0.0f, 1.0f, 0.0f };

	_vertexBuffer->Unmap(0, nullptr);
	buffer = nullptr;



	//画像の読み込み
	//WICでリソースは作ってくれます
	hr = DirectX::LoadWICTextureFromFile(device, _T("NormalMap/indominus_normal.jpg"), _texture.ReleaseAndGetAddressOf(), _decodedData, _subresource);

	//画像データの書き込み

	ComPtr<ID3D12Resource> textureUploadHeap;
	D3D12_RESOURCE_DESC uploadDesc = {
		D3D12_RESOURCE_DIMENSION_BUFFER,
		0,
		GetRequiredIntermediateSize(_texture.Get(), 0, 1),
		1,
		1,
		1,
		DXGI_FORMAT_UNKNOWN,
		{ 1, 0 },
		D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		D3D12_RESOURCE_FLAG_NONE
	};

	D3D12_HEAP_PROPERTIES props = {
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		D3D12_MEMORY_POOL_UNKNOWN,
		1,
		1
	};

	hr = device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(textureUploadHeap.GetAddressOf()));
	if (FAILED(hr))
	{
		return hr;
	}


	UpdateSubresources(command_list, _texture.Get(), textureUploadHeap.Get(), static_cast<UINT>(0), static_cast<UINT>(0), static_cast<UINT>(1), &_subresource);
	if (FAILED(hr))
	{
		return hr;
	}
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));


	//テクスチャ用のデスクリプタヒープの作成
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc{};
	descriptor_heap_desc.NumDescriptors = 2;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptor_heap_desc.NodeMask = 0;
	hr = device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&_dh_texture));
	if (FAILED(hr)) {
		return hr;
	}


	//シェーダリソースビューの作成
	D3D12_CPU_DESCRIPTOR_HANDLE handle_srv{};
	D3D12_SHADER_RESOURCE_VIEW_DESC resourct_view_desc{};

	resourct_view_desc.Format = _texture.Get()->GetDesc().Format;
	resourct_view_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	resourct_view_desc.Texture2D.MipLevels = 1;
	resourct_view_desc.Texture2D.MostDetailedMip = 0;
	resourct_view_desc.Texture2D.PlaneSlice = 0;
	resourct_view_desc.Texture2D.ResourceMinLODClamp = 0.0F;
	resourct_view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle_srv = _dh_texture->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(_texture.Get(), &resourct_view_desc, handle_srv);
	resourct_view_desc.Format = DXGI_FORMAT_R32_FLOAT;
	handle_srv.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	device->CreateShaderResourceView(shadowResource, &resourct_view_desc, handle_srv);

	command_list->Close();
	ID3D12CommandList* command_lists[] = { command_list };
	// closeしてから呼ぼうね
	inCmdQueue->ExecuteCommandLists(_countof(command_lists), command_lists); // 実行するコマンドリストの配列をコマンドキューへ送信します
																			 // コマンド転送自体はすぐ終わるけどGPUの処理が終わらないかもしれないから待とうね
	WaitForPreviousFrame(inFlames, inCmdQueue, inFenceEvent, inFnece);

	inCmdAlloc->Reset();
	command_list->Reset(inCmdAlloc, inPiplineState);

	CreatePSO(device, inSignature);

	return S_OK;
}

HRESULT Plane::Update() {
	HRESULT hr;
	static constexpr float PI = 3.14159265358979323846264338f;
	static int cnt{};
	//++cnt;

	XMMATRIX view = XMMatrixLookAtLH({ 0.0f, 20.0f, -41.0f }, { 0.0f, 5.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
	XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(50.0f), 640.0f / 480.0f, 1.0f, 300.0f);

	//オブジェクトの回転の設定
	XMMATRIX world = XMMatrixRotationY(XMConvertToRadians(static_cast<float>(cnt % 1800)) / 5.0f);

	// 合成行列用コンスタントバッファを書き込む
	// 行列に変換(Store)
	//XMFLOAT4X4 Mat;
	//XMStoreFloat4x4(&Mat, XMMatrixTranspose(world * view * projection));
	//XMFLOAT4X4 *buffer{};
	//hr = _constantBuffer->Map(0, nullptr, (void**)&buffer);
	//if (FAILED(hr)) {

	//	return hr;
	//}
	//_constantBuffer->Unmap(0, nullptr);
	//*buffer = Mat;

	// worldだけ投げる

	XMMATRIX *bufferWorld{};
	hr = _constantBufferWorld->Map(0, nullptr, (void**)&bufferWorld);
	if (FAILED(hr)) {
		return hr;
	}
	*bufferWorld = world;
	_constantBufferWorld->Unmap(0, nullptr);
	bufferWorld = nullptr;

	// viewとprojectioの合成行列を投げる
	XMMATRIX *bufferViewProjection{};
	hr = _constantBufferViewProjection->Map(0, nullptr, (void**)&bufferViewProjection);
	if (FAILED(hr)) {
		return hr;
	}
	*bufferViewProjection = view * projection;
	_constantBufferViewProjection->Unmap(0, nullptr);
	bufferViewProjection = nullptr;

	return hr;
}

HRESULT Plane::Draw(ID3D12GraphicsCommandList *command_list) {
	D3D12_VERTEX_BUFFER_VIEW	 vertex_view{};
	vertex_view.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
	vertex_view.StrideInBytes = sizeof(Vertex3D);
	vertex_view.SizeInBytes = sizeof(Vertex3D) * 4;

	//定数バッファをシェーダのレジスタにセット
	command_list->SetGraphicsRootConstantBufferView(0, _constantBuffer->GetGPUVirtualAddress());
	command_list->SetGraphicsRootConstantBufferView(1, _constantBufferWorld->GetGPUVirtualAddress());
	command_list->SetGraphicsRootConstantBufferView(2, _constantBufferViewProjection->GetGPUVirtualAddress());

	////テクスチャをシェーダのレジスタにセット
	ID3D12DescriptorHeap* tex_heaps[] = { _dh_texture.Get() };
	command_list->SetDescriptorHeaps(_countof(tex_heaps), tex_heaps);
	command_list->SetGraphicsRootDescriptorTable(4, _dh_texture->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* mat_heaps[] = { _dh_nullBuff2.Get() };
	command_list->SetDescriptorHeaps(_countof(mat_heaps), mat_heaps);
	command_list->SetGraphicsRootDescriptorTable(3, _dh_nullBuff2.Get()->GetGPUDescriptorHandleForHeapStart());// コマンドリストに設定


																											   //インデックスを使用しないトライアングルストリップで描画
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	command_list->IASetVertexBuffers(0, 1, &vertex_view);

	//描画
	command_list->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}

HRESULT Plane::CreatePSO(ID3D12Device *device, ID3D12RootSignature* inSignature)
{
	HRESULT hr;
	ComPtr<ID3DBlob> vertex_shader{};
	ComPtr<ID3DBlob> pixel_shader{};

#if defined(_DEBUG)
	UINT compile_flag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compile_flag = 0;
#endif

	hr = D3DCompileFromFile(_T("normalShader.hlsl"), nullptr, nullptr, "VSFloorShader", "vs_5_0", compile_flag, 0, vertex_shader.GetAddressOf(), nullptr);
	if (FAILED(hr)) {
		return hr;
	}

	hr = D3DCompileFromFile(_T("normalShader.hlsl"), nullptr, nullptr, "PSNormalMain", "ps_5_0", compile_flag, 0, pixel_shader.GetAddressOf(), nullptr);
	if (FAILED(hr)) {
		return hr;
	}


	// 頂点レイアウト.
	D3D12_INPUT_ELEMENT_DESC InputElementDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCODE", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },			// ボーン情報
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }			// 影響度
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
	pipeline_state_desc.pRootSignature = inSignature;


	//ラスタライザステートの設定
	pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
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

	hr = device->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&_pipelineState));

	return S_OK;
}

HRESULT Plane::WaitForPreviousFrame(UINT64& inFlames, ID3D12CommandQueue* ommand_queue_, HANDLE& fence_event_, ID3D12Fence* inFnece) {
	HRESULT hr;

	const UINT64 fence = inFlames;
	hr = ommand_queue_->Signal(inFnece, fence);
	if (FAILED(hr)) {
		return -1;
	}
	++inFlames;

	if (inFnece->GetCompletedValue() < fence) {
		hr = inFnece->SetEventOnCompletion(fence, fence_event_);
		if (FAILED(hr)) {
			return -1;
		}

		WaitForSingleObject(fence_event_, INFINITE);
	}
	return S_OK;
}

ID3D12PipelineState* Plane::GetPso()
{
	return _pipelineState.Get();
}
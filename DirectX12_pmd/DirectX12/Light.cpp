#include "Light.h"

using namespace DirectX;
using namespace Microsoft::WRL;

Light::Light()
{
}


Light::~Light()
{
}

HRESULT Light::CreateBuffer(ID3D12Device *inDevice)
{
	HRESULT hr{};
	D3D12_HEAP_PROPERTIES heap_properties{};
	D3D12_RESOURCE_DESC   resource_desc{};

	heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask = 0;
	heap_properties.VisibleNodeMask = 0;

	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resource_desc.Width = (640 + 0xff)&~0xff;
	resource_desc.Height = 1;
	resource_desc.DepthOrArraySize = 1;
	resource_desc.MipLevels = 1;
	resource_desc.Format = DXGI_FORMAT_UNKNOWN;
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resource_desc.SampleDesc.Count = 1;
	resource_desc.SampleDesc.Quality = 0;


	hr = inDevice->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_lightBuffer));
	if (FAILED(hr)) {
		return hr;
	}


	_viewPortSm.TopLeftX = 0.f;
	_viewPortSm.TopLeftY = 0.f;
	_viewPortSm.Width = (640 + 0xff)&~0xff;
	_viewPortSm.Height = (640 + 0xff)&~0xff;
	_viewPortSm.MinDepth = 0.f;
	_viewPortSm.MaxDepth = 1.f;

	_scissorRectSm.top = 0;
	_scissorRectSm.left = 0;
	_scissorRectSm.right = (640 + 0xff)&~0xff;
	_scissorRectSm.bottom = (640 + 0xff)&~0xff;

	// バッファに書き込み
	struct Light {
		XMMATRIX light_vp;
		XMFLOAT4 color;
		XMFLOAT3 dir;
	};

	Light *p{};
	hr = _lightBuffer->Map(0, nullptr, (void**)&p);
	if (FAILED(hr)) {
		return hr;
	}

	_lightPos = { 30.0f, 30.5f, -35.0f };
	_lightDst = { 0.0f, 1.0f, 0.0f };
	_lightDir = {
		_lightPos.x - _lightDst.x,
		_lightPos.y - _lightDst.y,
		_lightPos.z - _lightDst.z
	};
	_lightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMMATRIX view = XMMatrixLookAtLH(
	{ _lightPos.x, _lightPos.y, _lightPos.z },
	{ _lightDst.x, _lightDst.y, _lightDst.z },
	{ 0.0f, 1.0f, 0.0f }
	);

	XMMATRIX projection = XMMatrixOrthographicLH(40.0f, 40.0f, 0.1f, 150.0f);

	XMFLOAT4X4 mat;
	XMStoreFloat4x4(&mat, XMMatrixTranspose(view * projection));


	p->light_vp = view * projection;
	p->dir = _lightDir;
	p->color = _lightColor;

	_lightBuffer->Unmap(0, nullptr);
	p = nullptr;

	return S_OK;
}
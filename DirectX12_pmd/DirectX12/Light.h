#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>

class Light
{
private:
	D3D12_VIEWPORT										_viewPortSm;
	D3D12_RECT											_scissorRectSm;
	DirectX::XMFLOAT3									_lightPos;			//ライトの位置
	DirectX::XMFLOAT3									_lightDst;			//ライトの注視点
	DirectX::XMFLOAT3									_lightDir;			//ディレクショナルライトの向き
	DirectX::XMFLOAT4									_lightColor;		//ディレクショナルライトの色
	Microsoft::WRL::ComPtr<ID3D12Resource>				_lightBuffer;
public:
	Light();
	~Light();
	HRESULT CreateBuffer(ID3D12Device *inDevice);
	ID3D12Resource* GetBuffer()
	{
		return _lightBuffer.Get();
	}
	D3D12_VIEWPORT* GetViewPort()
	{
		return &_viewPortSm;
	}
	D3D12_RECT* GetScissorRect()
	{
		return &_scissorRectSm;
	}
	void UpdateBuffer();
};


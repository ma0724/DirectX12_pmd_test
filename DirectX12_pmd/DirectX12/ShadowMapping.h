#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>

class ShadowMapping
{
private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		_dh_shadowDSV;	//シャドウマップ用深度バッファ用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		_dh_shadowRTV;	//シャドウマップ用深度テクスチャ用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12Resource>				_shadowBuffer;		//シャドウマップ用深度バッファ
	Microsoft::WRL::ComPtr<ID3D12PipelineState>			_shadowMapPso;		//シャドウマップ用のパイプライン
public:
	ShadowMapping();
	~ShadowMapping();

	HRESULT CreateShadowBuffers(ID3D12Device* inDevice);
	HRESULT CreatePso(ID3D12Device* inDevice, ID3D12RootSignature* inSignature);
	ID3D12DescriptorHeap* GetShadowBuffer()
	{
		return _dh_shadowDSV.Get();
	}
	ID3D12DescriptorHeap* GetShadowTexture()
	{
		return _dh_shadowRTV.Get();
	}
	ID3D12Resource* GetBuffer()
	{
		return _shadowBuffer.Get();
	}
	ID3D12PipelineState* GetPso()
	{
		return _shadowMapPso.Get();
	}

};


#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>

class ShadowMapping
{
private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		_dh_shadowDSV;	//�V���h�E�}�b�v�p�[�x�o�b�t�@�p�f�X�N���v�^�q�[�v
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		_dh_shadowRTV;	//�V���h�E�}�b�v�p�[�x�e�N�X�`���p�f�X�N���v�^�q�[�v
	Microsoft::WRL::ComPtr<ID3D12Resource>				_shadowBuffer;		//�V���h�E�}�b�v�p�[�x�o�b�t�@
	Microsoft::WRL::ComPtr<ID3D12PipelineState>			_shadowMapPso;		//�V���h�E�}�b�v�p�̃p�C�v���C��
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


#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class ShadowView
{
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> _viewShadowMatrixBuffer;
public:
	ShadowView(ID3D12Resource* inResource);
	~ShadowView();

	ID3D12Resource* GetCameraMatBuffer()
	{
		return _viewShadowMatrixBuffer.Get();
	}
};


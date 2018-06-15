#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class Camera
{
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> _viewCameraMatrixBuffer;
public:
	Camera(ID3D12Resource* inResource);
	~Camera();

	ID3D12Resource* GetCameraMatBuffer()
	{
		return _viewCameraMatrixBuffer.Get();
	}
};


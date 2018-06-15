#ifndef PLANE_HEADER_
#define PLANE_HEADER_

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>


class Plane {
public:
	Plane();
	~Plane() {}
	HRESULT Initialize(ID3D12Device *device, ID3D12RootSignature* inSignature, ID3D12GraphicsCommandList *command_list, ID3D12CommandQueue* inCmdQueue, ID3D12CommandAllocator* inCmdAlloc, ID3D12PipelineState* inPiplineState, UINT64& inFlames, HANDLE& inFenceEvent, ID3D12Fence* inFnece, ID3D12Resource *shadowResource);
	HRESULT Update();
	HRESULT Draw(ID3D12GraphicsCommandList *command_list);

	HRESULT CreatePSO(ID3D12Device *device, ID3D12RootSignature* inSignature);
	HRESULT WaitForPreviousFrame(UINT64& inFlames, ID3D12CommandQueue* ommand_queue_, HANDLE& fence_event_, ID3D12Fence* inFnece);
	ID3D12PipelineState* GetPso();

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBufferWorld;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBufferViewProjection;

	Microsoft::WRL::ComPtr<ID3D12Resource>			_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource>			_texture;
	Microsoft::WRL::ComPtr<ID3D12Resource>			_boneMatBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	_dh_texture;

	Microsoft::WRL::ComPtr<ID3D12Resource>			_nullBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource>			_nullBuffer2;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	_dh_nullBuff;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	_dh_nullBuff2;

	std::unique_ptr<uint8_t[]>		_decodedData;
	D3D12_SUBRESOURCE_DATA			_subresource;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>		_pipelineState;
};

#endif

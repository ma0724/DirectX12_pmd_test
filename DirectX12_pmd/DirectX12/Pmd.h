#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include "natumashi.h"
#include <map>

class pmdMesh;

class Pmd
{
private:

	pmdMesh* _pmdMesh;

	std::vector<pmdVertex> _vertcies;			// PMDの頂点達
	std::vector<unsigned short> _indcies;		// PMDのインデックス
	std::vector<pmdMaterial> _materials;		// PMDのマテリアル(ディフューズ)
	std::vector<usePMDMaterial> _useMaterials;	// 実際につかうボーン

	unsigned int _vertexCount;
	unsigned int _indexCount;
	unsigned int _materialCount;
	unsigned int _boneCount;
	unsigned int _ikBoneCount;
	int _idx;

public:
	typedef unsigned char byte;
	typedef unsigned short uint16;

public:
	Pmd();
	~Pmd();
	HRESULT Initialize(ID3D12Device *inDevice, ID3D12GraphicsCommandList* inCmdList, ID3D12CommandQueue* inCmdQueue, ID3D12CommandAllocator* inCmdAlloc, ID3D12PipelineState* inPiplineState, UINT64& inFlames, HANDLE& inFenceEvent, ID3D12Fence* inFnece, ID3D12Resource *shadowResource);
	HRESULT Draw(ID3D12GraphicsCommandList *inCommandList);

	pmdMesh* GetPmdMesh()
	{
		return _pmdMesh;
	}
	void CcdikSolve(std::map <std::string, IKList>& ikmap,const char* ikname, DirectX::XMFLOAT3& location);
	float Length(DirectX::XMFLOAT3 inValue);
	DirectX::XMFLOAT3 Normalize(DirectX::XMFLOAT3 invalL);

private:
	HRESULT WaitForPreviousFrame(UINT64& inFlames, ID3D12CommandQueue* ommand_queue_, HANDLE& fence_event_, ID3D12Fence* inFnece);

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> _vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> _indexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBufferWorld;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBufferViewProjection;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBufferMaterial;
	Microsoft::WRL::ComPtr<ID3D12Resource> _texture;
	Microsoft::WRL::ComPtr<ID3D12Resource> _nullTexture;
	std::unique_ptr<uint8_t[]> _decodedData;
	D3D12_SUBRESOURCE_DATA _subresource;
	std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> _dh_texture;
	std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> _dh_materials;

};
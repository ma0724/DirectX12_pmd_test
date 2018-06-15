#pragma once
#include <wrl/client.h>
#include <vector>
#include <d3d12.h>
#include <map>
#include <memory>
#include "natumashi.h"

class pmdMesh
{
private:

	Microsoft::WRL::ComPtr<ID3D12Resource> _vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> _indexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBufferWorld;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBufferViewProjection;
	Microsoft::WRL::ComPtr<ID3D12Resource> _constantBufferMaterial;

	std::unique_ptr<uint8_t[]> _decodedData;
	D3D12_SUBRESOURCE_DATA _subresource;
	std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> _dh_texture;
	std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> _dh_materials;

	std::vector<BoneInformation>   _boneInformation;	// É{Å[ÉìèÓïÒ
	std::vector<Bones>			   _bone;
	std::map<std::string, int>	   _boneMap;
	std::vector<DirectX::XMMATRIX> _boneMatrix;
	std::vector<BoneNode>		   _boneNodes;

	std::vector<IKList>			   _ikList;
	std::map <std::string, IKList> _ikMap;

public:
	pmdMesh();
	~pmdMesh();

	void SetVertexBuffer(ID3D12Resource* inData)
	{
		_vertexBuffer = inData;
	}
	void SetIndexBuffer(ID3D12Resource* inData)
	{
		_indexBuffer = inData;
	}
	//  =====================================================
	ID3D12Resource* GetVertexBuffer()
	{
		return _vertexBuffer.Get();
	}
	ID3D12Resource* GetIndexBuffer()
	{
		return _indexBuffer.Get();
	}

	std::vector<BoneInformation>& BoneInformation()
	{
		return _boneInformation;
	}

	std::map<std::string, int>& BoneMap()
	{
		return this->_boneMap;
	}

	std::vector<Bones>& Bone()
	{
		return _bone;
	}

	std::vector<BoneNode>& BoneNode()
	{
		return _boneNodes;
	}

	std::vector<DirectX::XMMATRIX>& BoneMatrix()
	{
		return _boneMatrix;
	}
	// =======================================================
	std::vector<IKList>& IkList()
	{
		return _ikList;
	}

	std::map <std::string, IKList>& IkMap()
	{
		return _ikMap;
	}
};


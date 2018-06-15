#pragma once
#include <DirectXMath.h>
#include <vector>
class VMDData;

class VMDLoader
{
private:
	struct VMDHeader {
		char vmdheader[30];
		char vmdModelName[20];
	};
	struct VMDMmotion
	{
		char boneName[15];
		unsigned int frameNo;	// フレーム番号
		DirectX::XMFLOAT3 location;	// 位置
		DirectX::XMFLOAT4 rotation;	// [Quaternion] 回転
		unsigned char interporation[64]; // [4][4][4] 補完(ベジェデータ)
	};

	VMDHeader _header;
	std::vector<VMDMmotion> _motion;
	unsigned int _motionCount;
	VMDData* _data;

public:
	VMDLoader();
	~VMDLoader();

	VMDData* LoadMotion(char* str);
};


#pragma once
#include <vector>
#include <map>
#include "WICTextureLoader12.h"
#include <DirectXMath.h>
#include "natumashi.h"
#include <string>

class VMDData
{
private:
	std::vector<MotionData> _data;
	std::map< std::string, std::vector<MotionData>> _keyflames;
public:
	std::vector<MotionData>& GetMotionData();
	std::map< std::string, std::vector<MotionData>>& GetMotionKeyflameData();
	VMDData();
	~VMDData();
};


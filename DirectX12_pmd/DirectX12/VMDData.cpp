#include "VMDData.h"



VMDData::VMDData()
{
}


VMDData::~VMDData()
{
}


std::vector<MotionData>& VMDData::GetMotionData()
{
	return _data;
}

std::map< std::string, std::vector<MotionData>>& VMDData::GetMotionKeyflameData()
{
	return _keyflames;
}
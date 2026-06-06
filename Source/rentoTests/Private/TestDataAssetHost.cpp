// TestDataAssetHost.cpp
// 测试桩实现文件（UHT 生成代码宿主 + 静态计数器定义）
// story-003 专用，物理路径：Source/rentoTests/Private/TestDataAssetHost.cpp
#include "TestDataAssetHost.h"

// 静态计数器定义（在 TestDataAssetHost.h 中 extern 声明）
int32 GDAHostInitCount   = 0;
int32 GDAHostDeinitCount = 0;

// F2 seam：CancelHandle 路径断言镜像（extern 声明在 TestDataAssetHost.h）
// 须在相关测试函数头部重置：GDAHostLastCancelObserved = false;
bool  GDAHostLastCancelObserved = false;

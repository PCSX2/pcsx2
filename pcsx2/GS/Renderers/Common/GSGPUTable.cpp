#include <span>
#include <tuple>

#include "GSDevice.h"

// Table obtained from https://dgpu-docs.intel.com/devices/hardware-table.html (2026-2-2)
static constexpr std::pair<u16, GPUArchitecture> gs_gpu_table_intel[] = {
	{ 0xb080, GPUArchitecture::IntelXe3 }, // Intel Arc B390 GPU
	{ 0xb082, GPUArchitecture::IntelXe3 }, // Intel Arc B390 GPU
	{ 0xb084, GPUArchitecture::IntelXe3 }, // Intel Arc Pro B390 GPU
	{ 0xb086, GPUArchitecture::IntelXe3 }, // Intel Arc Pro B390 GPU
	{ 0xb081, GPUArchitecture::IntelXe3 }, // Intel Arc B370 GPU
	{ 0xb083, GPUArchitecture::IntelXe3 }, // Intel Arc B370 GPU
	{ 0xb085, GPUArchitecture::IntelXe3 }, // Intel Arc Pro B370 GPU
	{ 0xb087, GPUArchitecture::IntelXe3 }, // Intel Arc Pro B370 GPU
	{ 0xb090, GPUArchitecture::IntelXe3 }, // Intel Graphics
	{ 0xb0a0, GPUArchitecture::IntelXe3 }, // Intel Graphics
	{ 0xe212, GPUArchitecture::IntelXe2 }, // Intel Arc Pro B50 Graphics
	{ 0xe211, GPUArchitecture::IntelXe2 }, // Intel Arc Pro B60 Graphics
	{ 0xe20b, GPUArchitecture::IntelXe2 }, // Intel Arc B580 Graphics
	{ 0xe20c, GPUArchitecture::IntelXe2 }, // Intel Arc B570 Graphics
	{ 0x64a0, GPUArchitecture::IntelXe2 }, // Intel Arc Graphics
	{ 0x6420, GPUArchitecture::IntelXe2 }, // Intel Graphics
	{ 0x7d51, GPUArchitecture::IntelXeLPG }, // Intel Graphics
	{ 0x7d67, GPUArchitecture::IntelXeLPG }, // Intel Graphics
	{ 0x7d41, GPUArchitecture::IntelXeLPG }, // Intel Graphics
	{ 0x7dd5, GPUArchitecture::IntelXeLPG }, // Intel Graphics
	{ 0x7d45, GPUArchitecture::IntelXeLPG }, // Intel Graphics
	{ 0x7d40, GPUArchitecture::IntelXeLPG }, // Intel Graphics
	{ 0x7d55, GPUArchitecture::IntelXeLPG }, // Intel Arc Graphics
	{ 0x0bd5, GPUArchitecture::IntelXeHPC }, // Intel Data Center GPU Max 1550
	{ 0x0bda, GPUArchitecture::IntelXeHPC }, // Intel Data Center GPU Max 1100
	{ 0x56c0, GPUArchitecture::IntelXeHPG }, // Intel Data Center GPU Flex 170
	{ 0x56c1, GPUArchitecture::IntelXeHPG }, // Intel Data Center GPU Flex 140
	{ 0x5690, GPUArchitecture::IntelXeHPG }, // Intel Arc A770M Graphics
	{ 0x5691, GPUArchitecture::IntelXeHPG }, // Intel Arc A730M Graphics
	{ 0x5696, GPUArchitecture::IntelXeHPG }, // Intel Arc A570M Graphics
	{ 0x5692, GPUArchitecture::IntelXeHPG }, // Intel Arc A550M Graphics
	{ 0x5697, GPUArchitecture::IntelXeHPG }, // Intel Arc A530M Graphics
	{ 0x5693, GPUArchitecture::IntelXeHPG }, // Intel Arc A370M Graphics
	{ 0x5694, GPUArchitecture::IntelXeHPG }, // Intel Arc A350M Graphics
	{ 0x56a0, GPUArchitecture::IntelXeHPG }, // Intel Arc A770 Graphics
	{ 0x56a1, GPUArchitecture::IntelXeHPG }, // Intel Arc A750 Graphics
	{ 0x56a2, GPUArchitecture::IntelXeHPG }, // Intel Arc A580 Graphics
	{ 0x56a5, GPUArchitecture::IntelXeHPG }, // Intel Arc A380 Graphics
	{ 0x56a6, GPUArchitecture::IntelXeHPG }, // Intel Arc A310 Graphics
	{ 0x56b3, GPUArchitecture::IntelXeHPG }, // Intel Arc Pro A60 Graphics
	{ 0x56b2, GPUArchitecture::IntelXeHPG }, // Intel Arc Pro A60M Graphics
	{ 0x56b1, GPUArchitecture::IntelXeHPG }, // Intel Arc Pro A40/A50 Graphics
	{ 0x56b0, GPUArchitecture::IntelXeHPG }, // Intel Arc Pro A30M Graphics
	{ 0x56ba, GPUArchitecture::IntelXeHPG }, // Intel Arc A380E Graphics
	{ 0x56bc, GPUArchitecture::IntelXeHPG }, // Intel Arc A370E Graphics
	{ 0x56bd, GPUArchitecture::IntelXeHPG }, // Intel Arc A350E Graphics
	{ 0x56bb, GPUArchitecture::IntelXeHPG }, // Intel Arc A310E Graphics
	{ 0xa780, GPUArchitecture::IntelXe }, // Intel UHD Graphics 770
	{ 0xa781, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0xa788, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0xa789, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0xa78a, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0xa782, GPUArchitecture::IntelXe }, // Intel UHD Graphics 730
	{ 0xa78b, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0xa783, GPUArchitecture::IntelXe }, // Intel UHD Graphics 710
	{ 0xa7a0, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0xa7a1, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0xa7a8, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0xa7aa, GPUArchitecture::IntelXe }, // Intel Graphics
	{ 0xa7ab, GPUArchitecture::IntelXe }, // Intel Graphics
	{ 0xa7ac, GPUArchitecture::IntelXe }, // Intel Graphics
	{ 0xa7ad, GPUArchitecture::IntelXe }, // Intel Graphics
	{ 0xa7a9, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0xa721, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4905, GPUArchitecture::IntelXe }, // Intel Iris Xe MAX Graphics
	{ 0x4907, GPUArchitecture::IntelXe }, // Intel Server GPU SG-18M
	{ 0x4908, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x4909, GPUArchitecture::IntelXe }, // Intel Iris Xe MAX 100 Graphics
	{ 0x4680, GPUArchitecture::IntelXe }, // Intel UHD Graphics 770
	{ 0x4690, GPUArchitecture::IntelXe }, // Intel UHD Graphics 770
	{ 0x4688, GPUArchitecture::IntelXe }, // Intel UHD Graphics 770
	{ 0x468a, GPUArchitecture::IntelXe }, // Intel UHD Graphics 770
	{ 0x468b, GPUArchitecture::IntelXe }, // Intel UHD Graphics 770
	{ 0x4682, GPUArchitecture::IntelXe }, // Intel UHD Graphics 730
	{ 0x4692, GPUArchitecture::IntelXe }, // Intel UHD Graphics 730
	{ 0x4693, GPUArchitecture::IntelXe }, // Intel UHD Graphics 710
	{ 0x46d3, GPUArchitecture::IntelXe }, // Intel Graphics
	{ 0x46d4, GPUArchitecture::IntelXe }, // Intel Graphics
	{ 0x46d0, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x46d1, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x46d2, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4626, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4628, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x462a, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x46a2, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x46b3, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x46c2, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x46a3, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x46b2, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x46c3, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x46a0, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x46b0, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x46c0, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x46a6, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x46aa, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x46a8, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x46a1, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x46b1, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x46c1, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x4c8a, GPUArchitecture::IntelXe }, // Intel UHD Graphics 750
	{ 0x4c8b, GPUArchitecture::IntelXe }, // Intel UHD Graphics 730
	{ 0x4c90, GPUArchitecture::IntelXe }, // Intel UHD Graphics P750
	{ 0x4c9a, GPUArchitecture::IntelXe }, // Intel UHD Graphics P750
	{ 0x4e71, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4e61, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4e57, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4e55, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4e51, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4557, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4555, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4571, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4551, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x4541, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x9a59, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x9a78, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x9a60, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x9a70, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x9a68, GPUArchitecture::IntelXe }, // Intel UHD Graphics
	{ 0x9a40, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x9a49, GPUArchitecture::IntelXe }, // Intel Iris Xe Graphics
	{ 0x8a70, GPUArchitecture::IntelGen11 }, // Intel HD Graphics
	{ 0x8a71, GPUArchitecture::IntelGen11 }, // Intel HD Graphics
	{ 0x8a56, GPUArchitecture::IntelGen11 }, // Intel UHD Graphics
	{ 0x8a58, GPUArchitecture::IntelGen11 }, // Intel UHD Graphics
	{ 0x8a5b, GPUArchitecture::IntelGen11 }, // Intel HD Graphics
	{ 0x8a5d, GPUArchitecture::IntelGen11 }, // Intel HD Graphics
	{ 0x8a54, GPUArchitecture::IntelGen11 }, // Intel Iris Plus Graphics
	{ 0x8a5a, GPUArchitecture::IntelGen11 }, // Intel Iris Plus Graphics
	{ 0x8a5c, GPUArchitecture::IntelGen11 }, // Intel Iris Plus Graphics
	{ 0x8a57, GPUArchitecture::IntelGen11 }, // Intel HD Graphics
	{ 0x8a59, GPUArchitecture::IntelGen11 }, // Intel HD Graphics
	{ 0x8a50, GPUArchitecture::IntelGen11 }, // Intel HD Graphics
	{ 0x8a51, GPUArchitecture::IntelGen11 }, // Intel Iris Plus Graphics
	{ 0x8a52, GPUArchitecture::IntelGen11 }, // Intel Iris Plus Graphics
	{ 0x8a53, GPUArchitecture::IntelGen11 }, // Intel Iris Plus Graphics
	{ 0x3ea5, GPUArchitecture::IntelGen9 }, // Intel Iris Plus Graphics 655
	{ 0x3ea8, GPUArchitecture::IntelGen9 }, // Intel Iris Plus Graphics 655
	{ 0x3ea6, GPUArchitecture::IntelGen9 }, // Intel Iris Plus Graphics 645
	{ 0x3ea7, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x3ea2, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x3e90, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 610
	{ 0x3e93, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 610
	{ 0x3e99, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 610
	{ 0x3e9c, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 610
	{ 0x3ea1, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 610
	{ 0x9ba5, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 610
	{ 0x9ba8, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 610
	{ 0x3ea4, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9b21, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9ba0, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9ba2, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9ba4, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9baa, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9bab, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9bac, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x87ca, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x3ea3, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9b41, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9bc0, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9bc2, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9bc4, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9bca, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9bcb, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x9bcc, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics
	{ 0x3e91, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 630
	{ 0x3e92, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 630
	{ 0x3e98, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 630
	{ 0x3e9b, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 630
	{ 0x9bc5, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 630
	{ 0x9bc8, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 630
	{ 0x3e96, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics P630
	{ 0x3e9a, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics P630
	{ 0x3e94, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics P630
	{ 0x9bc6, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics P630
	{ 0x9be6, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics P630
	{ 0x9bf6, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics P630
	{ 0x3ea9, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 620
	{ 0x3ea0, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 620
	{ 0x593b, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x5923, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 635
	{ 0x5926, GPUArchitecture::IntelGen9 }, // Intel Iris Plus Graphics 640
	{ 0x5927, GPUArchitecture::IntelGen9 }, // Intel Iris Plus Graphics 650
	{ 0x5917, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 620
	{ 0x5912, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 630
	{ 0x591b, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 630
	{ 0x5916, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 620
	{ 0x5921, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 620
	{ 0x591a, GPUArchitecture::IntelGen9 }, // Intel HD Graphics P630
	{ 0x591d, GPUArchitecture::IntelGen9 }, // Intel HD Graphics P630
	{ 0x591e, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 615
	{ 0x591c, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 615
	{ 0x87c0, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 617
	{ 0x5913, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x5915, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x5902, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 610
	{ 0x5906, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 610
	{ 0x590b, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 610
	{ 0x590a, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x5908, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x590e, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x3185, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 600
	{ 0x3184, GPUArchitecture::IntelGen9 }, // Intel UHD Graphics 605
	{ 0x1a85, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x5a85, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 500
	{ 0x0a84, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x1a84, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x5a84, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 505
	{ 0x192a, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x1932, GPUArchitecture::IntelGen9 }, // Intel Iris Pro Graphics 580
	{ 0x193b, GPUArchitecture::IntelGen9 }, // Intel Iris Pro Graphics 580
	{ 0x193a, GPUArchitecture::IntelGen9 }, // Intel Iris Pro Graphics P580
	{ 0x193d, GPUArchitecture::IntelGen9 }, // Intel Iris Pro Graphics P580
	{ 0x1923, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 535
	{ 0x1926, GPUArchitecture::IntelGen9 }, // Intel Iris Graphics 540
	{ 0x1927, GPUArchitecture::IntelGen9 }, // Intel Iris Graphics 550
	{ 0x192b, GPUArchitecture::IntelGen9 }, // Intel Iris Graphics 555
	{ 0x192d, GPUArchitecture::IntelGen9 }, // Intel Iris Graphics P555
	{ 0x1912, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 530
	{ 0x191b, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 530
	{ 0x1913, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x1915, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x1917, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x191a, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x1916, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 520
	{ 0x1921, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 520
	{ 0x191d, GPUArchitecture::IntelGen9 }, // Intel HD Graphics P530
	{ 0x191e, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 515
	{ 0x1902, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 510
	{ 0x1906, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 510
	{ 0x190b, GPUArchitecture::IntelGen9 }, // Intel HD Graphics 510
	{ 0x190a, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x190e, GPUArchitecture::IntelGen9 }, // Intel HD Graphics
	{ 0x163d, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x163a, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x1632, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x163e, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x163b, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x1636, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x1622, GPUArchitecture::IntelGen8 }, // Intel Iris Pro Graphics 6200
	{ 0x1626, GPUArchitecture::IntelGen8 }, // Intel HD Graphics 6000
	{ 0x162a, GPUArchitecture::IntelGen8 }, // Intel Iris Pro Graphics P6300
	{ 0x162b, GPUArchitecture::IntelGen8 }, // Intel Iris Graphics 6100
	{ 0x162d, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x162e, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x1612, GPUArchitecture::IntelGen8 }, // Intel HD Graphics 5600
	{ 0x1616, GPUArchitecture::IntelGen8 }, // Intel HD Graphics 5500
	{ 0x161a, GPUArchitecture::IntelGen8 }, // Intel HD Graphics P5700
	{ 0x161b, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x161d, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x161e, GPUArchitecture::IntelGen8 }, // Intel HD Graphics 5300
	{ 0x1602, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x1606, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x160a, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x160b, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x160d, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x160e, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x22b0, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x22b2, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x22b3, GPUArchitecture::IntelGen8 }, // Intel HD Graphics
	{ 0x22b1, GPUArchitecture::IntelGen8 }, // Intel HD Graphics XXX
	{ 0x0f30, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0f31, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0f32, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0f33, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0157, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0155, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0422, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0426, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x042a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x042b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x042e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c22, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c26, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c2a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c2b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c2e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a22, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a2a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a2b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d2a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d2b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d2e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a26, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 5000
	{ 0x0a2e, GPUArchitecture::IntelGen7 }, // Intel Iris Graphics 5100
	{ 0x0d22, GPUArchitecture::IntelGen7 }, // Intel Iris Pro Graphics 5200
	{ 0x0d26, GPUArchitecture::IntelGen7 }, // Intel Iris Pro Graphics P5200
	{ 0x0412, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 4600
	{ 0x0416, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 4600
	{ 0x0d12, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 4600
	{ 0x041a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics P4600/P4700
	{ 0x041b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c12, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c16, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c1a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c1b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c1e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a12, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a1a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a1b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d16, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d1a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d1b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d1e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x041e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 4400
	{ 0x0a16, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 4400
	{ 0x0a1e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 4200
	{ 0x0402, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0406, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x040a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x040b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x040e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c02, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c06, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c0a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c0b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0c0e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a02, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a06, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a0a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a0b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0a0e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d02, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d06, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d0a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d0b, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0d0e, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0162, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 4000
	{ 0x0166, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 4000
	{ 0x016a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics P4000
	{ 0x0152, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 2500
	{ 0x0156, GPUArchitecture::IntelGen7 }, // Intel HD Graphics 2500
	{ 0x015a, GPUArchitecture::IntelGen7 }, // Intel HD Graphics
	{ 0x0112, GPUArchitecture::IntelGen6 }, // Intel HD Graphics 3000
	{ 0x0122, GPUArchitecture::IntelGen6 }, // Intel HD Graphics 3000
	{ 0x0116, GPUArchitecture::IntelGen6 }, // Intel HD Graphics 3000
	{ 0x0126, GPUArchitecture::IntelGen6 }, // Intel HD Graphics 3000
	{ 0x0102, GPUArchitecture::IntelGen6 }, // Intel HD Graphics 2000
	{ 0x0106, GPUArchitecture::IntelGen6 }, // Intel HD Graphics 2000
	{ 0x010a, GPUArchitecture::IntelGen6 }, // Intel HD Graphics 2000
	{ 0x0042, GPUArchitecture::IntelGen5 }, // Intel HD Graphics
	{ 0x0046, GPUArchitecture::IntelGen5 }, // Intel HD Graphics
	{ 0x2a42, GPUArchitecture::IntelGen4 }, // Mobile Intel GM45 Express Chipset
	{ 0x2e02, GPUArchitecture::IntelGen4 }, // Intel Integrated Graphics Device
	{ 0x2e12, GPUArchitecture::IntelGen4 }, // Intel Q45/Q43
	{ 0x2e22, GPUArchitecture::IntelGen4 }, // Intel G45/G43
	{ 0x2e32, GPUArchitecture::IntelGen4 }, // Intel G41
	{ 0x2e42, GPUArchitecture::IntelGen4 }, // Intel B43
	{ 0x2e92, GPUArchitecture::IntelGen4 }, // Intel B43
	{ 0x29a2, GPUArchitecture::IntelGen4 }, // Intel 965G
	{ 0x2982, GPUArchitecture::IntelGen4 }, // Intel 965G
	{ 0x2992, GPUArchitecture::IntelGen4 }, // Intel 965Q
	{ 0x2972, GPUArchitecture::IntelGen4 }, // Intel 946GZ
	{ 0x2a02, GPUArchitecture::IntelGen4 }, // Intel 965GM
	{ 0x2a12, GPUArchitecture::IntelGen4 }, // Intel 965GME/GLE
	{ 0xa001, GPUArchitecture::IntelGen3 }, // Intel Atom D4xx/D5xx
	{ 0xa011, GPUArchitecture::IntelGen3 }, // Intel Atom N4xx/N5xx
	{ 0x29d2, GPUArchitecture::IntelGen3 }, // Intel Q33
	{ 0x29c2, GPUArchitecture::IntelGen3 }, // Intel G33
	{ 0x29b2, GPUArchitecture::IntelGen3 }, // Intel Q35
	{ 0x27ae, GPUArchitecture::IntelGen3 }, // Intel 945GME
	{ 0x27a2, GPUArchitecture::IntelGen3 }, // Intel 945GM
	{ 0x2772, GPUArchitecture::IntelGen3 }, // Intel 945G
	{ 0x2592, GPUArchitecture::IntelGen3 }, // Intel 915GM
	{ 0x258a, GPUArchitecture::IntelGen3 }, // Intel E7221G (i915)
	{ 0x2582, GPUArchitecture::IntelGen3 }, // Intel E7221G (i915)
	{ 0x2582, GPUArchitecture::IntelGen2 }, // Intel 915G
	{ 0x2572, GPUArchitecture::IntelGen2 }, // Intel 865G
	{ 0x3582, GPUArchitecture::IntelGen2 }, // Intel 852GM/855GM
	{ 0x358e, GPUArchitecture::IntelGen2 }, // Intel 852GM/855GM
	{ 0x2562, GPUArchitecture::IntelGen2 }, // Intel 845G
	{ 0x3577, GPUArchitecture::IntelGen2 }, // Intel 830M
};

GPUArchitecture LookupGPUArchitecture(uint16_t vendor_id, uint16_t pci_id)
{
	std::span<const std::pair<uint16_t, GPUArchitecture>> table = {};
	if (vendor_id == 0x8086)
		table = gs_gpu_table_intel;
	// We can add other tables if we need to check AMD/NV architectures later
	for (const auto& entry : table)
		if (entry.first == pci_id)
			return entry.second;
	return GPUArchitecture::Unknown;
}
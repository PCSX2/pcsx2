#include <stdint.h>

#include <cpuinfo.h>
#include <x86/api.h>


/* Intel vendor string: "GenuineIntel" */
#define Genu UINT32_C(0x756E6547)
#define ineI UINT32_C(0x49656E69)
#define ntel UINT32_C(0x6C65746E)

/* AMD vendor strings: "AuthenticAMD", "AMDisbetter!", "AMD ISBETTER" */
#define Auth UINT32_C(0x68747541)
#define enti UINT32_C(0x69746E65)
#define cAMD UINT32_C(0x444D4163)
#define AMDi UINT32_C(0x69444D41)
#define sbet UINT32_C(0x74656273)
#define ter  UINT32_C(0x21726574)
#define AMD  UINT32_C(0x20444D41)
#define ISBE UINT32_C(0x45425349)
#define TTER UINT32_C(0x52455454)

/* VIA (Centaur) vendor strings: "CentaurHauls", "VIA VIA VIA " */
#define Cent UINT32_C(0x746E6543)
#define aurH UINT32_C(0x48727561)
#define auls UINT32_C(0x736C7561)
#define VIA  UINT32_C(0x20414956)

/* Hygon vendor string: "HygonGenuine" */
#define Hygo UINT32_C(0x6F677948)
#define nGen UINT32_C(0x6E65476E)
#define uine UINT32_C(0x656E6975)

/* Transmeta vendor strings: "GenuineTMx86", "TransmetaCPU" */
#define ineT UINT32_C(0x54656E69)
#define Mx86 UINT32_C(0x3638784D)
#define Tran UINT32_C(0x6E617254)
#define smet UINT32_C(0x74656D73)
#define aCPU UINT32_C(0x55504361)

/* Cyrix vendor string: "CyrixInstead" */
#define Cyri UINT32_C(0x69727943)
#define xIns UINT32_C(0x736E4978)
#define tead UINT32_C(0x64616574)

/* Rise vendor string: "RiseRiseRise" */
#define Rise UINT32_C(0x65736952)

/* NSC vendor string: "Geode by NSC" */
#define Geod UINT32_C(0x646F6547)
#define e_by UINT32_C(0x79622065)
#define NSC  UINT32_C(0x43534E20)

/* SiS vendor string: "SiS SiS SiS " */
#define SiS  UINT32_C(0x20536953)

/* NexGen vendor string: "NexGenDriven" */
#define NexG UINT32_C(0x4778654E)
#define enDr UINT32_C(0x72446E65)
#define iven UINT32_C(0x6E657669)

/* UMC vendor string: "UMC UMC UMC " */
#define UMC  UINT32_C(0x20434D55)

/* RDC vendor string: "Genuine  RDC" */
#define ine  UINT32_C(0x20656E69)
#define RDC  UINT32_C(0x43445220)

/* D&MP vendor string: "Vortex86 SoC" */
#define Vort UINT32_C(0x74726F56)
#define ex86 UINT32_C(0x36387865)
#define SoC  UINT32_C(0x436F5320)


enum cpuinfo_vendor cpuinfo_x86_decode_vendor(uint32_t ebx, uint32_t ecx, uint32_t edx) {
	switch (ebx) {
		case Genu:
			switch (edx) {
				case ineI:
					if (ecx == ntel) {
						/* "GenuineIntel" */
						return cpuinfo_vendor_intel;
					}
					break;
#if CPUINFO_ARCH_X86
				case ineT:
					if (ecx == Mx86) {
						/* "GenuineTMx86" */
						return cpuinfo_vendor_transmeta;
					}
					break;
				case ine:
					if (ecx == RDC) {
						/* "Genuine  RDC" */
						return cpuinfo_vendor_rdc;
					}
					break;
#endif
			}
			break;
		case Auth:
			if (edx == enti && ecx == cAMD) {
				/* "AuthenticAMD" */
				return cpuinfo_vendor_amd;
			}
			break;
		case Cent:
			if (edx == aurH && ecx == auls) {
				/* "CentaurHauls" */
				return cpuinfo_vendor_via;
			}
			break;
		case Hygo:
			if (edx == nGen && ecx == uine) {
				/* "HygonGenuine" */
				return cpuinfo_vendor_hygon;
			}
			break;
#if CPUINFO_ARCH_X86
		case AMDi:
			if (edx == sbet && ecx == ter) {
				/* "AMDisbetter!" */
				return cpuinfo_vendor_amd;
			}
			break;
		case AMD:
			if (edx == ISBE && ecx == TTER) {
				/* "AMD ISBETTER" */
				return cpuinfo_vendor_amd;
			}
			break;
		case VIA:
			if (edx == VIA && ecx == VIA) {
				/* "VIA VIA VIA " */
				return cpuinfo_vendor_via;
			}
			break;
		case Tran:
			if (edx == smet && ecx == aCPU) {
				/* "TransmetaCPU" */
				return cpuinfo_vendor_transmeta;
			}
			break;
		case Cyri:
			if (edx == xIns && ecx == tead) {
				/* "CyrixInstead" */
				return cpuinfo_vendor_cyrix;
			}
			break;
		case Rise:
			if (edx == Rise && ecx == Rise) {
				/* "RiseRiseRise" */
				return cpuinfo_vendor_rise;
			}
			break;
		case Geod:
			if (edx == e_by && ecx == NSC) {
				/* "Geode by NSC" */
				return cpuinfo_vendor_nsc;
			}
			break;
		case SiS:
			if (edx == SiS && ecx == SiS) {
				/* "SiS SiS SiS " */
				return cpuinfo_vendor_sis;
			}
			break;
		case NexG:
			if (edx == enDr && ecx == iven) {
				/* "NexGenDriven" */
				return cpuinfo_vendor_nexgen;
			}
			break;
		case UMC:
			if (edx == UMC && ecx == UMC) {
				/* "UMC UMC UMC " */
				return cpuinfo_vendor_umc;
			}
			break;
		case Vort:
			if (edx == ex86 && ecx == SoC) {
				/* "Vortex86 SoC" */
				return cpuinfo_vendor_dmp;
			}
			break;
#endif
	}
	return cpuinfo_vendor_unknown;
}

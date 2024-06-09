#include <stdint.h>

#include <cpuinfo.h>
#include <x86/api.h>

enum cpuinfo_uarch cpuinfo_x86_decode_uarch(
	enum cpuinfo_vendor vendor,
	const struct cpuinfo_x86_model_info* model_info) {
	switch (vendor) {
		case cpuinfo_vendor_intel:
			switch (model_info->family) {
#if CPUINFO_ARCH_X86
				case 0x05:
					switch (model_info->model) {
						case 0x01: // Pentium (60, 66)
						case 0x02: // Pentium (75, 90,
							   // 100, 120, 133,
							   // 150, 166, 200)
						case 0x03: // Pentium OverDrive
							   // for Intel486-based
							   // systems
						case 0x04: // Pentium MMX
							return cpuinfo_uarch_p5;
						case 0x09:
							return cpuinfo_uarch_quark;
					}
					break;
#endif /* CPUINFO_ARCH_X86 */
				case 0x06:
					switch (model_info->model) {
						/* Mainstream cores */
#if CPUINFO_ARCH_X86
						case 0x01: // Pentium Pro
						case 0x03: // Pentium II
							   // (Klamath) and
							   // Pentium II
							   // Overdrive
						case 0x05: // Pentium II
							   // (Deschutes,
							   // Tonga), Pentium II
							   // Celeron
							   // (Covington),
							   // Pentium II Xeon
							   // (Drake)
						case 0x06: // Pentium II
							   // (Dixon), Pentium
							   // II Celeron
							   // (Mendocino)
						case 0x07: // Pentium III
							   // (Katmai), Pentium
							   // III Xeon (Tanner)
						case 0x08: // Pentium III
							   // (Coppermine),
							   // Pentium II Celeron
							   // (Coppermine-128),
							   // Pentium III Xeon
							   // (Cascades)
						case 0x0A: // Pentium III Xeon
							   // (Cascades-2MB)
						case 0x0B: // Pentium III
							   // (Tualatin),
							   // Pentium III
							   // Celeron
							   // (Tualatin-256)
							return cpuinfo_uarch_p6;
						case 0x09: // Pentium M
							   // (Banias), Pentium
							   // M Celeron
							   // (Banias-0,
							   // Banias-512)
						case 0x0D: // Pentium M
							   // (Dothan), Pentium
							   // M Celeron
							   // (Dothan-512,
							   // Dothan-1024)
						case 0x15: // Intel 80579
							   // (Tolapai)
							return cpuinfo_uarch_dothan;
						case 0x0E: // Core Solo/Duo
							   // (Yonah), Pentium
							   // Dual-Core T2xxx
							   // (Yonah), Celeron M
							   // (Yonah-512,
							   // Yonah-1024),
							   // Dual-Core Xeon
							   // (Sossaman)
							return cpuinfo_uarch_yonah;
#endif /* CPUINFO_ARCH_X86 */
						case 0x0F: // Core 2 Duo
							   // (Conroe,
							   // Conroe-2M, Merom),
							   // Core 2 Quad
							   // (Tigerton), Xeon
							   // (Woodcrest,
							   // Clovertown,
							   // Kentsfield)
						case 0x16: // Celeron (Conroe-L,
							   // Merom-L), Core 2
							   // Duo (Merom)
							return cpuinfo_uarch_conroe;
						case 0x17: // Core 2 Duo
							   // (Penryn-3M), Core
							   // 2 Quad
							   // (Yorkfield), Core
							   // 2 Extreme
							   // (Yorkfield), Xeon
							   // (Harpertown),
							   // Pentium Dual-Core
							   // (Penryn)
						case 0x1D: // Xeon (Dunnington)
							return cpuinfo_uarch_penryn;
						case 0x1A: // Core iX
							   // (Bloomfield), Xeon
							   // (Gainestown)
						case 0x1E: // Core iX
							   // (Lynnfield,
							   // Clarksfield)
						case 0x1F: // Core iX
							   // (Havendale)
						case 0x2E: // Xeon (Beckton)
						case 0x25: // Core iX
							   // (Clarkdale)
						case 0x2C: // Core iX
							   // (Gulftown), Xeon
							   // (Gulftown)
						case 0x2F: // Xeon (Eagleton)
							return cpuinfo_uarch_nehalem;
						case 0x2A: // Core iX (Sandy
							   // Bridge)
						case 0x2D: // Core iX (Sandy
							   // Bridge-E), Xeon
							   // (Sandy Bridge
							   // EP/EX)
							return cpuinfo_uarch_sandy_bridge;
						case 0x3A: // Core iX (Ivy
							   // Bridge)
						case 0x3E: // Ivy Bridge-E
							return cpuinfo_uarch_ivy_bridge;
						case 0x3C:
						case 0x3F: // Haswell-E
						case 0x45: // Haswell ULT
						case 0x46: // Haswell with eDRAM
							return cpuinfo_uarch_haswell;
						case 0x3D: // Broadwell-U
						case 0x47: // Broadwell-H
						case 0x4F: // Broadwell-E
						case 0x56: // Broadwell-DE
							return cpuinfo_uarch_broadwell;
						case 0x4E: // Sky Lake Client
							   // Y/U
						case 0x55: // Sky/Cascade/Cooper
							   // Lake Server
						case 0x5E: // Sky Lake Client
							   // DT/H/S
						case 0x8E: // Kaby/Whiskey/Amber/Comet
							   // Lake Y/U
						case 0x9E: // Kaby/Coffee Lake
							   // DT/H/S
						case 0xA5: // Comet Lake H/S
						case 0xA6: // Comet Lake U/Y
							return cpuinfo_uarch_sky_lake;
						case 0x66: // Cannon Lake (Core
							   // i3-8121U)
							return cpuinfo_uarch_palm_cove;
						case 0x6A: // Ice Lake-DE
						case 0x6C: // Ice Lake-SP
						case 0x7D: // Ice Lake-Y
						case 0x7E: // Ice Lake-U
							return cpuinfo_uarch_sunny_cove;

						/* Low-power cores */
						case 0x1C: // Diamondville,
							   // Silverthorne,
							   // Pineview
						case 0x26: // Tunnel Creek
							return cpuinfo_uarch_bonnell;
						case 0x27: // Medfield
						case 0x35: // Cloverview
						case 0x36: // Cedarview,
							   // Centerton
							return cpuinfo_uarch_saltwell;
						case 0x37: // Bay Trail
						case 0x4A: // Merrifield
						case 0x4D: // Avoton, Rangeley
						case 0x5A: // Moorefield
						case 0x5D: // SoFIA
							return cpuinfo_uarch_silvermont;
						case 0x4C: // Braswell, Cherry
							   // Trail
						case 0x75: // Spreadtrum
							   // SC9853I-IA
							return cpuinfo_uarch_airmont;
						case 0x5C: // Apollo Lake
						case 0x5F: // Denverton
							return cpuinfo_uarch_goldmont;
						case 0x7A: // Gemini Lake
							return cpuinfo_uarch_goldmont_plus;

						/* Knights-series cores */
						case 0x57:
							return cpuinfo_uarch_knights_landing;
						case 0x85:
							return cpuinfo_uarch_knights_mill;
					}
					break;
				case 0x0F:
					switch (model_info->model) {
						case 0x00: // Pentium 4 Xeon
							   // (Foster)
						case 0x01: // Pentium 4 Celeron
							   // (Willamette-128),
							   // Pentium 4 Xeon
							   // (Foster, Foster
							   // MP)
						case 0x02: // Pentium 4
							   // (Northwood),
							   // Pentium 4 EE
							   // (Gallatin),
							   // Pentium 4 Celeron
							   // (Northwood-128,
							   // Northwood-256),
							   // Pentium 4 Xeon
							   // (Gallatin DP,
							   // Prestonia)
							return cpuinfo_uarch_willamette;
							break;
						case 0x03: // Pentium 4
							   // (Prescott),
							   // Pentium 4 Xeon
							   // (Nocona)
						case 0x04: // Pentium 4
							   // (Prescott-2M),
							   // Pentium 4 EE
							   // (Prescott-2M),
							   // Pentium D
							   // (Smithfield),
							   // Celeron D
							   // (Prescott-256),
							   // Pentium 4 Xeon
							   // (Cranford,
							   // Irwindale,
							   // Paxville)
						case 0x06: // Pentium 4 (Cedar
							   // Mill), Pentium D
							   // EE (Presler),
							   // Celeron D (Cedar
							   // Mill), Pentium 4
							   // Xeon (Dempsey,
							   // Tulsa)
							return cpuinfo_uarch_prescott;
					}
					break;
			}
			break;
		case cpuinfo_vendor_amd:
			switch (model_info->family) {
#if CPUINFO_ARCH_X86
				case 0x5:
					switch (model_info->model) {
						case 0x00:
						case 0x01:
						case 0x02:
							return cpuinfo_uarch_k5;
						case 0x06:
						case 0x07:
						case 0x08:
						case 0x0D:
							return cpuinfo_uarch_k6;
						case 0x0A:
							return cpuinfo_uarch_geode;
					}
					break;
				case 0x6:
					return cpuinfo_uarch_k7;
#endif /* CPUINFO_ARCH_X86 */
				case 0xF: // Opteron, Athlon 64, Sempron
				case 0x11: // Turion
					return cpuinfo_uarch_k8;
				case 0x10: // Opteron, Phenom, Athlon, Sempron
				case 0x12: // Llano APU
					return cpuinfo_uarch_k10;
				case 0x14:
					return cpuinfo_uarch_bobcat;
				case 0x15:
					switch (model_info->model) {
						case 0x00: // Engineering
							   // samples
						case 0x01: // Zambezi,
							   // Interlagos
							return cpuinfo_uarch_bulldozer;
						case 0x02: // Vishera
						case 0x10: // Trinity
						case 0x13: // Richland
							return cpuinfo_uarch_piledriver;
						case 0x38: // Godavari
						case 0x30: // Kaveri
							return cpuinfo_uarch_steamroller;
						case 0x60: // Carrizo
						case 0x65: // Bristol Ridge
						case 0x70: // Stoney Ridge
							return cpuinfo_uarch_excavator;
						default:
							switch (model_info->extended_model) {
								case 0x0:
									return cpuinfo_uarch_bulldozer;
								case 0x1: // No
									  // L3
									  // cache
								case 0x2: // With
									  // L3
									  // cache
									return cpuinfo_uarch_piledriver;
								case 0x3: // With
									  // L3
									  // cache
								case 0x4: // No
									  // L3
									  // cache
									return cpuinfo_uarch_steamroller;
							}
							break;
					}
					break;
				case 0x16:
					if (model_info->extended_model >= 0x03) {
						return cpuinfo_uarch_puma;
					} else {
						return cpuinfo_uarch_jaguar;
					}
				case 0x17:
					switch (model_info->extended_model) {
						case 0x0: // model 01h -> 14 nm
							  // Naples/Whitehaven/Summit
							  // Ridge/Snowy Owl,
							  // model 08h -> 12 nm
							  // Colfax/Pinnacle
							  // Ridge
						case 0x1: // model 11h -> 14 nm
							  // Raven Ridge/Great
							  // Horned Owl, model
							  // 18h -> 14 nm Banded
							  // Kestrel / 12 nm
							  // Picasso
							return cpuinfo_uarch_zen;
						case 0x3: // model 31h ->
							  // Rome/Castle Peak
						case 0x4: // model 47h -> Xbox
							  // Series X
						case 0x6: // model 60h ->
							  // Renoir/Grey Hawk,
							  // model 68h ->
							  // Lucienne
						case 0x7: // model 71h ->
							  // Matisse
						case 0x9: // model 90h -> Van
							  // Gogh, model 98h ->
							  // Mero
							return cpuinfo_uarch_zen2;
					}
					break;
				case 0x19:
					switch (model_info->extended_model) {
						case 0x0: // model 00h ->
							  // Genesis, model 01h
							  // -> Milan, model 08h
							  // -> Chagall
						case 0x2: // model 21h ->
							  // Vermeer
						case 0x3: // model 30h ->
							  // Badami, Trento
						case 0x4: // model 40h ->
							  // Rembrandt
						case 0x5: // model 50h ->
							  // Cezanne
							return cpuinfo_uarch_zen3;
						case 0x1: // model 10h..1Fh ->
							  // Stones
						case 0x6: // model 60h..6Fh ->
							  // Raphael
						case 0x7: // model 70h..77h ->
							  // Phoenix/Hawkpoint1,
							  // model 78h..7Fh ->
							  // Phoenix
							  // 2/Hawkpoint2
						case 0xA: // model A0h..AFh ->
							  // Stones-Dense
							return cpuinfo_uarch_zen4;
					}
					break;
			}
			break;
		case cpuinfo_vendor_hygon:
			switch (model_info->family) {
				case 0x00:
					return cpuinfo_uarch_dhyana;
			}
			break;
		default:
			break;
	}
	return cpuinfo_uarch_unknown;
}

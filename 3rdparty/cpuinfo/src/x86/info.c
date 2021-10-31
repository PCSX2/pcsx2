#include <stdint.h>

#include <cpuinfo.h>
#include <x86/api.h>


struct cpuinfo_x86_model_info cpuinfo_x86_decode_model_info(uint32_t eax) {
	struct cpuinfo_x86_model_info model_info;
	model_info.stepping        =  eax        & 0xF;
	model_info.base_model      = (eax >> 4)  & 0xF;
	model_info.base_family     = (eax >> 8)  & 0xF;
	model_info.processor_type  = (eax >> 12) & 0x3;
	model_info.extended_model  = (eax >> 16) & 0xF;
	model_info.extended_family = (eax >> 20) & 0xFF;

	model_info.family = model_info.base_family + model_info.extended_family;
	model_info.model  = model_info.base_model + (model_info.extended_model << 4);
	return model_info;
}

#include <math.h>

// sqrtf only defined in C++
extern "C" {

float (*fpusqrtf)(float fval) = 0;
float (*fpufabsf)(float fval) = 0;
float (*fpusinf)(float fval) = 0;
float (*fpucosf)(float fval) = 0;
float (*fpuexpf)(float fval) = 0;
float (*fpuatanf)(float fval) = 0;
float (*fpuatan2f)(float fvalx, float fvaly) = 0;

void InitFPUOps()
{
	fpusqrtf = sqrtf;
	fpufabsf = fabsf;
	fpusinf = sinf;
	fpucosf = cosf;
	fpuexpf = expf;
	fpuatanf = atanf;
	fpuatan2f = atan2f;
}

}
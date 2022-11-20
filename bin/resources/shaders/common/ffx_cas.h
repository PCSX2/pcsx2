//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//
//                                 [CAS] FIDELITY FX - CONSTRAST ADAPTIVE SHARPENING 1.20190610
//
//==============================================================================================================================
// LICENSE
// =======
// Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All rights reserved.
// -------
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// -------
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
// Software.
// -------
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//------------------------------------------------------------------------------------------------------------------------------
// ABOUT
// =====
// CAS is a spatial only filter.
// CAS takes RGB color input.
// CAS enchances sharpness and local high-frequency contrast, and with or without added upsampling.
// CAS outputs RGB color.
//------------------------------------------------------------------------------------------------------------------------------
// SUGGESTIONS FOR INTEGRATION
// ===========================
// Best for performance, run CAS in sharpen-only mode, choose a video mode to have scan-out or the display scale.
//  - Sharpen-only mode is faster, and provides a better quality sharpening.
// The scaling support in CAS was designed for when the application wants to do Dynamic Resolution Scaling (DRS).
//  - With DRS, the render resolution can change per frame.
//  - Use CAS to sharpen and upsample to the fixed output resolution, then composite the full resolution UI over CAS output.
//  - This can all happen in one compute dispatch.
// It is likely better to reduce the amount of film grain which happens before CAS (as CAS will amplify grain).
//  - An alternative would be to add grain after CAS.
// It is best to run CAS after tonemapping.
//  - CAS needs to have input value 1.0 at the peak of the display output.
// It is ok to run CAS after compositing UI (it won't harm the UI).
//------------------------------------------------------------------------------------------------------------------------------
// EXECUTION
// =========
// CAS runs as a compute shader.
// CAS is designed to be run either in a 32-bit, CasFilter(), or packed 16-bit, CasFilterH(), form.
// The 32-bit form works on 8x8 tiles via one {64,1,1} workgroup.
// The 16-bit form works on a pair of 8x8 tiles in a 16x8 configuration via one {64,1,1} workgroup.
// CAS is designed to work best in semi-persistent form if running not async with graphics.
// For 32-bit this means looping across a collection of 4 8x8 tiles in a 2x2 tile foot-print.
// For 16-bit this means looping 2 times, once for the top 16x8 region and once for the bottom 16x8 region.
//------------------------------------------------------------------------------------------------------------------------------
// INTEGRATION SUMMARY FOR CPU
// ===========================
// // Make sure <stdint.h> has already been included.
// // Setup pre-portability-header defines.
// #define A_CPU 1
// // Include the portability header (requires version 1.20190530 or later which is backwards compatible).
// #include "ffx_a.h"
// // Include the CAS header.
// #include "ffx_cas.h"
// ...
// // Call the setup function to build out the constants for the shader, pass these to the shader.
// // The 'varAU4(const0);' expands into 'uint32_t const0[4];' on the CPU.
// varAU4(const0);
// varAU4(const1);
// CasSetup(const0,const1,
//  0.0f,             // Sharpness tuning knob (0.0 to 1.0).
//  1920.0f,1080.0f,  // Example input size.
//  2560.0f,1440.0f); // Example output size.
// ...
// // Later dispatch the shader based on the amount of semi-persistent loop unrolling.
// // Here is an example for running with the 16x16 (4-way unroll for 32-bit or 2-way unroll for 16-bit)
// vkCmdDispatch(cmdBuf,(widthInPixels+15)>>4,(heightInPixels+15)>>4,1);
//------------------------------------------------------------------------------------------------------------------------------
// INTEGRATION SUMMARY FOR GPU
// ===========================
// // Setup layout. Example below for VK_FORMAT_R16G16B16A16_SFLOAT.
// layout(set=0,binding=0,rgba16f)uniform image2D imgSrc;
// layout(set=0,binding=1,rgba16f)uniform image2D imgDst; 
// ...
// // Setup pre-portability-header defines (sets up GLSL/HLSL path, packed math support, etc)
// #define A_GPU 1
// #define A_GLSL 1
// #define A_HALF 1
// ...
// // Include the portability header (or copy it in without an include).
// #include "ffx_a.h"
// ...
// // Define the fetch function(s).
// // CasLoad() takes a 32-bit unsigned integer 2D coordinate and loads color.
// AF3 CasLoad(ASU2 p){return imageLoad(imgSrc,p).rgb;}
// // CasLoadH() is the 16-bit version taking 16-bit unsigned integer 2D coordinate and loading 16-bit float color.
// // The ASU2() typecast back to 32-bit is a NO-OP, the compiler pattern matches and uses A16 opcode support instead.
// // The AH3() typecast to 16-bit float is a NO-OP, the compiler pattern matches and uses D16 opcode support instead.
// AH3 CasLoadH(ASW2 p){return AH3(imageLoad(imgSrc,ASU2(p)).rgb);}
// ...
// // Define the input modifiers as nop's initially.
// // See "INPUT FORMAT SPECIFIC CASES" below for specifics on what to place in these functions.
// void CasInput(inout AF1 r,inout AF1 g,inout AF1 b){}
// void CasInputH(inout AH2 r,inout AH2 g,inout AH2 b){}
// ...
// // Include this CAS header file (or copy it in without an include).
// #include "ffx_cas.h"
// ...
// // Example in shader integration for loop-unrolled 16x16 case for 32-bit.
// layout(local_size_x=64)in;
// void main(){
//   // Fetch constants from CasSetup().
//   AU4 const0=cb.const0;
//   AU4 const1=cb.const1;
//   // Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
//   AU2 gxy=ARmp8x8(gl_LocalInvocationID.x)+AU2(gl_WorkGroupID.x<<4u,gl_WorkGroupID.y<<4u);
//   // Filter.
//   AF4 c;
//   CasFilter(c.r,c.g,c.b,gxy,const0,const1,false);imageStore(imgDst,ASU2(gxy),c);
//   gxy.x+=8u;
//   CasFilter(c.r,c.g,c.b,gxy,const0,const1,false);imageStore(imgDst,ASU2(gxy),c);
//   gxy.y+=8u;
//   CasFilter(c.r,c.g,c.b,gxy,const0,const1,false);imageStore(imgDst,ASU2(gxy),c);
//   gxy.x-=8u;
//   CasFilter(c.r,c.g,c.b,gxy,const0,const1,false);imageStore(imgDst,ASU2(gxy),c);}
// ...
// // Example for semi-persistent 16x16 but this time for packed math.
// // Use this before including 'cas.h' if not using the non-packed filter function.
// #define CAS_PACKED_ONLY 1
// ...
// layout(local_size_x=64)in;
// void main(){
//  // Fetch constants from CasSetup().
//  AU4 const0=cb.const0;
//  AU4 const1=cb.const1;
//  // Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
//  AU2 gxy=ARmp8x8(gl_LocalInvocationID.x)+AU2(gl_WorkGroupID.x<<4u,gl_WorkGroupID.y<<4u);
//  // Filter.
//  AH4 c0,c1;AH2 cR,cG,cB;
//  CasFilterH(cR,cG,cB,gxy,const0,const1,false);
//  // Extra work integrated after CAS would go here.
//  ...
//  // Suggest only running CasDepack() right before stores, to maintain packed math for any work after CasFilterH().
//  CasDepack(c0,c1,cR,cG,cB);
//  imageStore(imgDst,ASU2(gxy),AF4(c0));
//  imageStore(imgDst,ASU2(gxy)+ASU2(8,0),AF4(c1));
//  gxy.y+=8u;
//  CasFilterH(cR,cG,cB,gxy,const0,const1,false);
//  ...
//  CasDepack(c0,c1,cR,cG,cB);
//  imageStore(imgDst,ASU2(gxy),AF4(c0));
//  imageStore(imgDst,ASU2(gxy)+ASU2(8,0),AF4(c1));}
//------------------------------------------------------------------------------------------------------------------------------
// CAS FILTERING LOGIC
// ===================
// CAS uses the minimal nearest 3x3 source texel window for filtering.
// The filter coefficients are radially symmetric (phase adaptive, computed per pixel based on output pixel center).
// The filter kernel adapts to local contrast (adjusting the negative lobe strength of the filter kernel).
//------------------------------------------------------------------------------------------------------------------------------
// CAS INPUT REQUIREMENTS
// ======================
// This is designed to be a linear filter.
// Running CAS on perceptual inputs will yield over-sharpening.
// Input must range between {0 to 1} for each color channel.
// CAS output will be {0 to 1} ranged as well.
// CAS does 5 loads, so any conversion applied during CasLoad() or CasInput() has a 5 load * 3 channel = 15x cost amplifier.
//  - So input conversions need to be factored into the prior pass's output.
//  - But if necessary use CasInput() instead of CasLoad(), as CasInput() works with packed color.
//  - For CAS with scaling the amplifier is 12 load * 3 channel = 36x cost amplifier.
// Any conversion applied to output has a 3x cost amplifier (3 color channels).
//  - Output conversions are substantially less expensive.
// Added VALU ops due to conversions will have visible cost as this shader is already quite VALU heavy.
// This filter does not function well on sRGB or gamma 2.2 non-linear data.
// This filter does not function on PQ non-linear data.
//  - Due to the shape of PQ, the positive side of the ring created by the negative lobe tends to become over-bright.
//------------------------------------------------------------------------------------------------------------------------------
// INPUT FORMAT SPECIFIC CASES
// ===========================
//  - FP16 with all non-negative values ranging {0 to 1}.
//     - Use as is, filter is designed for linear input and output ranging {0 to 1}.
// ---------------------------
//  - UNORM with linear conversion approximation.
//     - This could be used for both sRGB or FreeSync2 native (gamma 2.2) cases.
//     - Load/store with either 10:10:10:2 UNORM or 8:8:8:8 UNORM (aka VK_FORMAT_R8G8B8A8_UNORM).
//     - Use gamma 2.0 conversion in CasInput(), as an approximation.
//     - Modifications: 
//        // Change the CasInput*() function to square the inputs.
//        void CasInput(inout AF1 r,inout AF1 g,inout AF1 b){r*=r;g*=g;b*=b;}
//        void CasInputH(inout AH2 r,inout AH2 g,inout AH2 b){r*=r;g*=g;b*=b;}
//        ...
//        // Do linear to gamma 2.0 before store.
//        // Since it will be common to do processing after CAS, the filter function returns linear.
//        c.r=sqrt(c.r);c.g=sqrt(c.g);c.b=sqrt(c.b);
//        imageStore(imgDst,ASU2(gxy),c);
//        ...
//        // And for packed.
//        CasFilterH(cR,cG,cB,gxy,const0,const1,true);
//        cR=sqrt(cR);cG=sqrt(cG);cB=sqrt(cB);
//        CasDepack(c0,c1,cR,cG,cB);
//        imageStore(img[0],ASU2(gxy),AF4(c0));
//        imageStore(img[0],ASU2(gxy+AU2(8,0)),AF4(c1));
// ---------------------------
//  - sRGB with slightly better quality and higher cost.
//     - Use texelFetch() with sRGB format (VK_FORMAT_R8G8B8A8_SRGB) for loads (gets linear into shader).
//     - Store to destination using UNORM (not sRGB) stores and do the linear to sRGB conversion in the shader.
//     - Modifications:
//        // Use texel fetch instead of image load (on GCN this will translate into an image load in the driver).
//        // Hardware has sRGB to linear on loads (but in API only for read-only, aka texture instead of UAV/image).
//        AF3 CasLoad(ASU2 p){return texelFetch(texSrc,p,0).rgb;}
//        ...
//        // Do linear to sRGB before store (GPU lacking hardware conversion support for linear to sRGB on store).
//        c.r=AToSrgbF1(c.r);c.g=AToSrgbF1(c.g);c.b=AToSrgbF1(c.b);
//        imageStore(imgDst,ASU2(gxy),c);
//        ...
//        // And for packed.
//        CasFilterH(cR,cG,cB,gxy,const0,const1,true);
//        cR=AToSrgbH2(cR);cG=AToSrgbH2(cG);cB=AToSrgbH2(cB);
//        CasDepack(c0,c1,cR,cG,cB);
//        imageStore(img[0],ASU2(gxy),AF4(c0));
//        imageStore(img[0],ASU2(gxy+AU2(8,0)),AF4(c1));
// ---------------------------
//  - HDR10 output via scRGB.
//     - Pass before CAS needs to write out linear Rec.2020 colorspace output (all positive values).
//        - Write to FP16 with {0 to 1} mapped to {0 to maxNits} nits.
//           - Where 'maxNits' is typically not 10000.
//           - Instead set 'maxNits' to the nits level that the HDR TV starts to clip white.
//           - This can be even as low as 1000 nits on some HDR TVs.
//     - After CAS do matrix multiply to take Rec.2020 back to sRGB and multiply by 'maxNits/80.0'.
//        - Showing GPU code below to generate constants, likely most need to use CPU code instead.
//           - Keeping the GPU code here because it is easier to read in these docs.
//        - Can use 'lpm.h' source to generate the conversion matrix for Rec.2020 to sRGB:
//           // Output conversion matrix from sRGB to Rec.2020.
//           AF3 conR,conG,conB;
//           // Working space temporaries (Rec.2020).
//           AF3 rgbToXyzXW;AF3 rgbToXyzYW;AF3 rgbToXyzZW;
//           LpmColRgbToXyz(rgbToXyzXW,rgbToXyzYW,rgbToXyzZW,lpmCol2020R,lpmCol2020G,lpmCol2020B,lpmColD65);
//           // Output space temporaries (Rec.709, same as sRGB primaries).
//           AF3 rgbToXyzXO;AF3 rgbToXyzYO;AF3 rgbToXyzZO;
//           LpmColRgbToXyz(rgbToXyzXO,rgbToXyzYO,rgbToXyzZO,lpmCol709R,lpmCol709G,lpmCol709B,lpmColD65);
//           AF3 xyzToRgbRO;AF3 xyzToRgbGO;AF3 xyzToRgbBO;
//           LpmMatInv3x3(xyzToRgbRO,xyzToRgbGO,xyzToRgbBO,rgbToXyzXO,rgbToXyzYO,rgbToXyzZO);
//           // Generate the matrix.
//           LpmMatMul3x3(conR,conG,conB,xyzToRgbRO,xyzToRgbGO,xyzToRgbBO,rgbToXyzXW,rgbToXyzYW,rgbToXyzZW);
//        - Adjust the conversion matrix for the multiply by 'maxNits/80.0'.
//           // After this the constants can be stored into a constant buffer.
//           AF1 conScale=maxNits*ARcpF1(80.0);
//           conR*=conScale;conG*=conScale;conB*=conScale;
//        - After CAS do the matrix multiply (passing the fetched constants into the shader).
//           outputR=dot(AF3(colorR,colorG,colorB),conR);
//           outputG=dot(AF3(colorR,colorG,colorB),conG);
//           outputB=dot(AF3(colorR,colorG,colorB),conB);
//     - Hopefully no developer is taking scRGB as input to CAS.
//        - If that was the case, the conversion matrix from sRGB to Rec.2020 can be built changing the above code.
//           - Swap the 'lpmCol709*' and 'lpmCol2020*' inputs to LpmColRgbToXyz().
//           - Then scale by '80.0/maxNits' instead of 'maxNits/80.0'.
// ---------------------------
//  - HDR10 output via native 10:10:10:2.
//     - Pass before CAS needs to write out linear Rec.2020 colorspace output (all positive values).
//        - Write to FP16 with {0 to 1} mapped to {0 to maxNits} nits.
//           - Where 'maxNits' is typically not 10000.
//           - Instead set 'maxNits' to the nits level that the HDR TV starts to clip white.
//           - This can be even as low as 1000 nits on some HDR TVs.
//        - Hopefully no developer needs to take PQ as input here, but if so can use A to convert PQ to linear:
//           // Where 'k0' is a constant of 'maxNits/10000.0'.
//           colorR=AFromPqF1(colorR*k0);
//           colorG=AFromPqF1(colorG*k0);
//           colorB=AFromPqF1(colorB*k0);
//     - After CAS convert from linear to PQ.
//        // Where 'k1' is a constant of '10000.0/maxNits'.
//        colorR=AToPqF1(colorR*k1);
//        colorG=AToPqF1(colorG*k1);
//        colorB=AToPqF1(colorB*k1);
// ---------------------------
//  - Example of a bad idea for CAS input design.
//     - Have the pass before CAS store out in 10:10:10:2 UNORM with gamma 2.0.
//     - Store the output of CAS with sRGB to linear conversion, or with a gamma 2.2 conversion for FreeSync2 native.
//     - This will drop precision because the inputs had been quantized to 10-bit,
//       and the output is using a different tonal transform,
//       so inputs and outputs won't align for similar values.
//     - It might be "ok" for 8-bit/channel CAS output, but definately not a good idea for 10-bit/channel output.
//------------------------------------------------------------------------------------------------------------------------------
// ALGORITHM DESCRIPTION
// =====================
// This describes the algorithm with CAS_BETTER_DIAGONALS defined.
// The default is with CAS_BETTER_DIAGONALS not defined (which is faster).
// Starting with no scaling.
// CAS fetches a 3x3 neighborhood around the pixel 'e',
//  a b c
//  d(e)f
//  g h i
// It then computes a 'soft' minimum and maximum,
//  a b c             b
//  d e f * 0.5  +  d e f * 0.5
//  g h i             h
// The minimum and maximums give an idea of local contrast.
//  --- 1.0     ^
//   |          |  <-- This minimum distance to the signal limit is divided by MAX to get a base sharpening amount 'A'.
//  --- MAX     v
//   |
//   |
//  --- MIN     ^
//   |          |  <-- The MIN side is more distant in this example so it is not used, but for dark colors it would be used.
//   |          |
//  --- 0.0     v
// The base sharpening amount 'A' from above is shaped with a sqrt().
// This 'A' ranges from 0 := no sharpening, to 1 := full sharpening.
// Then 'A' is scaled by the sharpness knob while being transformed to a negative lobe (values from -1/5 to -1/8 for A=1).
// The final filter kernel looks like this,
//  0 A 0 
//  A 1 A  <-- Center is always 1.0, followed by the negative lobe 'A' in a ring, and windowed into a circle with the 0.0s.
//  0 A 0
// The local neighborhood is then multiplied by the kernel weights, summed and divided by the sum of the kernel weights.
// The high quality path computes filter weights per channel.
// The low quality path uses the green channel's filter weights to compute the 'A' factor for all channels.
// ---------------------
// The scaling path is a little more complex.
// It starts by fetching the 4x4 neighborhood around the pixel centered between centers of pixels {f,g,j,k},
//  a b c d
//  e(f g)h
//  i(j k)l
//  m n o p
// The algorithm then computes the no-scaling result for {f,g,j,k}.
// It then interpolates between those no-scaling results.
// The interpolation is adaptive.
// To hide bilinear interpolation and restore diagonals, it weights bilinear weights by 1/(const+contrast).
// Where 'contrast' is the soft 'max-min'.
// This makes edges thin out a little.
// ---------------------
// Without CAS_BETTER_DIAGONALS defined, the algorithm is a little faster.
// Instead of using the 3x3 "box" with the 5-tap "circle" this uses just the "circle".
// Drops to 5 texture fetches for no-scaling.
// Drops to 12 texture fetches for scaling.
// Drops a bunch of math.
//------------------------------------------------------------------------------------------------------------------------------
// IDEAS FOR FUTURE
// ================
//  - Avoid V_CVT's by using denormals.
//  - Manually pack FP16 literals.
//------------------------------------------------------------------------------------------------------------------------------
// CHANGE LOG
// ==========
// 20190610 - Misc documentation cleanup.
// 20190609 - Removed lowQuality bool, improved scaling logic.
// 20190530 - Unified CPU/GPU setup code, using new ffx_a.h, faster, define CAS_BETTER_DIAGONALS to get older slower one.
// 20190529 - Missing a good way to re-interpret packed in HLSL, so disabling approximation optimizations for now.
// 20190528 - Fixed so GPU CasSetup() generates half data all the time.
// 20190527 - Implemented approximations for rcp() and sqrt().
// 20190524 - New algorithm, adjustable sharpness, scaling to 4x area. Fixed checker debug for no-scaling only.
// 20190521 - Updated file naming.
// 20190516 - Updated docs, fixed workaround, fixed no-scaling quality issue, removed gamma2 and generalized as CasInput*().
// 20190510 - Made the dispatch example safely round up for images that are not a multiple of 16x16.
// 20190507 - Fixed typo bug in CAS_DEBUG_CHECKER, fixed sign typo in the docs.
// 20190503 - Setup temporary workaround for compiler bug.
// 20190502 - Added argument for 'gamma2' path so input transform in that case runs packed.
// 20190426 - Improved documentation on format specific cases, etc.
// 20190425 - Updated/corrected documentation.
// 20190405 - Added CAS_PACKED_ONLY, misc bug fixes.
// 20190404 - Updated for the new a.h header.
//==============================================================================================================================
// This is the practical limit for the algorithm's scaling ability (quality is limited by 3x3 taps). Example resolutions,
//  1280x720  -> 1080p = 2.25x area
//  1536x864  -> 1080p = 1.56x area
//  1792x1008 -> 1440p = 2.04x area
//  1920x1080 -> 1440p = 1.78x area
//  1920x1080 ->    4K =  4.0x area
//  2048x1152 -> 1440p = 1.56x area
//  2560x1440 ->    4K = 2.25x area
//  3072x1728 ->    4K = 1.56x area
#define CAS_AREA_LIMIT 4.0
//------------------------------------------------------------------------------------------------------------------------------
// Pass in output and input resolution in pixels.
// This returns true if CAS supports scaling in the given configuration.
AP1 CasSupportScaling(AF1 outX,AF1 outY,AF1 inX,AF1 inY){return ((outX*outY)*ARcpF1(inX*inY))<=CAS_AREA_LIMIT;}
//==============================================================================================================================
// Call to setup required constant values (works on CPU or GPU).
A_STATIC void CasSetup(
 outAU4 const0,
 outAU4 const1,
 AF1 sharpness, // 0 := default (lower ringing), 1 := maximum (higest ringing)
 AF1 inputSizeInPixelsX,
 AF1 inputSizeInPixelsY,
 AF1 outputSizeInPixelsX,
 AF1 outputSizeInPixelsY){
  // Scaling terms.
  const0[0]=AU1_AF1(inputSizeInPixelsX*ARcpF1(outputSizeInPixelsX));
  const0[1]=AU1_AF1(inputSizeInPixelsY*ARcpF1(outputSizeInPixelsY));
  const0[2]=AU1_AF1(AF1_(0.5)*inputSizeInPixelsX*ARcpF1(outputSizeInPixelsX)-AF1_(0.5));
  const0[3]=AU1_AF1(AF1_(0.5)*inputSizeInPixelsY*ARcpF1(outputSizeInPixelsY)-AF1_(0.5));
  // Sharpness value.
  AF1 sharp=-ARcpF1(ALerpF1(8.0,5.0,ASatF1(sharpness)));
  varAF2(hSharp)=initAF2(sharp,0.0);
  const1[0]=AU1_AF1(sharp);
  const1[1]=AU1_AH2_AF2(hSharp);
  const1[2]=AU1_AF1(AF1_(8.0)*inputSizeInPixelsX*ARcpF1(outputSizeInPixelsX));
  const1[3]=0;}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                     NON-PACKED VERSION
//==============================================================================================================================
#ifdef A_GPU
 #ifdef CAS_PACKED_ONLY
  // Avoid compiler error.
  AF3 CasLoad(ASU2 p){return AF3(0.0,0.0,0.0);}
  void CasInput(inout AF1 r,inout AF1 g,inout AF1 b){}
 #endif
//------------------------------------------------------------------------------------------------------------------------------
 void CasFilter(
 out AF1 pixR, // Output values, non-vector so port between CasFilter() and CasFilterH() is easy.
 out AF1 pixG,
 out AF1 pixB,
 AU2 ip, // Integer pixel position in output.
 AU4 const0, // Constants generated by CasSetup().
 AU4 const1,
 AP1 noScaling){ // Must be a compile-time literal value, true = sharpen only (no resize).
//------------------------------------------------------------------------------------------------------------------------------
  // Debug a checker pattern of on/off tiles for visual inspection.
  #ifdef CAS_DEBUG_CHECKER
   if((((ip.x^ip.y)>>8u)&1u)==0u){AF3 pix0=CasLoad(ASU2(ip));
    pixR=pix0.r;pixG=pix0.g;pixB=pix0.b;CasInput(pixR,pixG,pixB);return;}
  #endif 
//------------------------------------------------------------------------------------------------------------------------------
  // No scaling algorithm uses minimal 3x3 pixel neighborhood.
  if(noScaling){
   // a b c 
   // d e f
   // g h i
   ASU2 sp=ASU2(ip);
   AF3 a=CasLoad(sp+ASU2(-1,-1));
   AF3 b=CasLoad(sp+ASU2( 0,-1));
   AF3 c=CasLoad(sp+ASU2( 1,-1));
   AF3 d=CasLoad(sp+ASU2(-1, 0));
   AF3 e=CasLoad(sp);
   AF3 f=CasLoad(sp+ASU2( 1, 0));
   AF3 g=CasLoad(sp+ASU2(-1, 1));
   AF3 h=CasLoad(sp+ASU2( 0, 1));
   AF3 i=CasLoad(sp+ASU2( 1, 1));
   // Run optional input transform.
   CasInput(a.r,a.g,a.b);
   CasInput(b.r,b.g,b.b);
   CasInput(c.r,c.g,c.b);
   CasInput(d.r,d.g,d.b);
   CasInput(e.r,e.g,e.b);
   CasInput(f.r,f.g,f.b);
   CasInput(g.r,g.g,g.b);
   CasInput(h.r,h.g,h.b);
   CasInput(i.r,i.g,i.b);
   // Soft min and max.
   //  a b c             b
   //  d e f * 0.5  +  d e f * 0.5
   //  g h i             h
   // These are 2.0x bigger (factored out the extra multiply).
   AF1 mnR=AMin3F1(AMin3F1(d.r,e.r,f.r),b.r,h.r);
   AF1 mnG=AMin3F1(AMin3F1(d.g,e.g,f.g),b.g,h.g);
   AF1 mnB=AMin3F1(AMin3F1(d.b,e.b,f.b),b.b,h.b);
   #ifdef CAS_BETTER_DIAGONALS
    AF1 mnR2=AMin3F1(AMin3F1(mnR,a.r,c.r),g.r,i.r);
    AF1 mnG2=AMin3F1(AMin3F1(mnG,a.g,c.g),g.g,i.g);
    AF1 mnB2=AMin3F1(AMin3F1(mnB,a.b,c.b),g.b,i.b);
    mnR=mnR+mnR2;
    mnG=mnG+mnG2;
    mnB=mnB+mnB2;
   #endif
   AF1 mxR=AMax3F1(AMax3F1(d.r,e.r,f.r),b.r,h.r);
   AF1 mxG=AMax3F1(AMax3F1(d.g,e.g,f.g),b.g,h.g);
   AF1 mxB=AMax3F1(AMax3F1(d.b,e.b,f.b),b.b,h.b);
   #ifdef CAS_BETTER_DIAGONALS
    AF1 mxR2=AMax3F1(AMax3F1(mxR,a.r,c.r),g.r,i.r);
    AF1 mxG2=AMax3F1(AMax3F1(mxG,a.g,c.g),g.g,i.g);
    AF1 mxB2=AMax3F1(AMax3F1(mxB,a.b,c.b),g.b,i.b);
    mxR=mxR+mxR2;
    mxG=mxG+mxG2;
    mxB=mxB+mxB2;
   #endif
   // Smooth minimum distance to signal limit divided by smooth max.
   #ifdef CAS_GO_SLOWER
    AF1 rcpMR=ARcpF1(mxR);
    AF1 rcpMG=ARcpF1(mxG);
    AF1 rcpMB=ARcpF1(mxB);
   #else
    AF1 rcpMR=APrxLoRcpF1(mxR);
    AF1 rcpMG=APrxLoRcpF1(mxG);
    AF1 rcpMB=APrxLoRcpF1(mxB);
   #endif
   #ifdef CAS_BETTER_DIAGONALS
    AF1 ampR=ASatF1(min(mnR,AF1_(2.0)-mxR)*rcpMR);
    AF1 ampG=ASatF1(min(mnG,AF1_(2.0)-mxG)*rcpMG);
    AF1 ampB=ASatF1(min(mnB,AF1_(2.0)-mxB)*rcpMB);
   #else
    AF1 ampR=ASatF1(min(mnR,AF1_(1.0)-mxR)*rcpMR);
    AF1 ampG=ASatF1(min(mnG,AF1_(1.0)-mxG)*rcpMG);
    AF1 ampB=ASatF1(min(mnB,AF1_(1.0)-mxB)*rcpMB);
   #endif
   // Shaping amount of sharpening.
   #ifdef CAS_GO_SLOWER
    ampR=sqrt(ampR);
    ampG=sqrt(ampG);
    ampB=sqrt(ampB);
   #else
    ampR=APrxLoSqrtF1(ampR);
    ampG=APrxLoSqrtF1(ampG);
    ampB=APrxLoSqrtF1(ampB);
   #endif
   // Filter shape.
   //  0 w 0
   //  w 1 w
   //  0 w 0
   AF1 peak=AF1_AU1(const1.x);
   AF1 wR=ampR*peak;
   AF1 wG=ampG*peak;
   AF1 wB=ampB*peak;
   // Filter.
   #ifndef CAS_SLOW
    // Using green coef only, depending on dead code removal to strip out the extra overhead.
    #ifdef CAS_GO_SLOWER
     AF1 rcpWeight=ARcpF1(AF1_(1.0)+AF1_(4.0)*wG);
    #else
     AF1 rcpWeight=APrxMedRcpF1(AF1_(1.0)+AF1_(4.0)*wG);
    #endif
    pixR=ASatF1((b.r*wG+d.r*wG+f.r*wG+h.r*wG+e.r)*rcpWeight);
    pixG=ASatF1((b.g*wG+d.g*wG+f.g*wG+h.g*wG+e.g)*rcpWeight);
    pixB=ASatF1((b.b*wG+d.b*wG+f.b*wG+h.b*wG+e.b)*rcpWeight);
   #else
    #ifdef CAS_GO_SLOWER
     AF1 rcpWeightR=ARcpF1(AF1_(1.0)+AF1_(4.0)*wR);
     AF1 rcpWeightG=ARcpF1(AF1_(1.0)+AF1_(4.0)*wG);
     AF1 rcpWeightB=ARcpF1(AF1_(1.0)+AF1_(4.0)*wB);
    #else
     AF1 rcpWeightR=APrxMedRcpF1(AF1_(1.0)+AF1_(4.0)*wR);
     AF1 rcpWeightG=APrxMedRcpF1(AF1_(1.0)+AF1_(4.0)*wG);
     AF1 rcpWeightB=APrxMedRcpF1(AF1_(1.0)+AF1_(4.0)*wB);
    #endif
    pixR=ASatF1((b.r*wR+d.r*wR+f.r*wR+h.r*wR+e.r)*rcpWeightR);
    pixG=ASatF1((b.g*wG+d.g*wG+f.g*wG+h.g*wG+e.g)*rcpWeightG);
    pixB=ASatF1((b.b*wB+d.b*wB+f.b*wB+h.b*wB+e.b)*rcpWeightB);
   #endif
   return;}
//------------------------------------------------------------------------------------------------------------------------------
  // Scaling algorithm adaptively interpolates between nearest 4 results of the non-scaling algorithm.
  //  a b c d
  //  e f g h
  //  i j k l
  //  m n o p
  // Working these 4 results.
  //  +-----+-----+
  //  |     |     |
  //  |  f..|..g  |
  //  |  .  |  .  |
  //  +-----+-----+
  //  |  .  |  .  |
  //  |  j..|..k  |
  //  |     |     |
  //  +-----+-----+
  AF2 pp=AF2(ip)*AF2_AU2(const0.xy)+AF2_AU2(const0.zw);
  AF2 fp=floor(pp);
  pp-=fp;
  ASU2 sp=ASU2(fp);
  AF3 a=CasLoad(sp+ASU2(-1,-1));
  AF3 b=CasLoad(sp+ASU2( 0,-1));
  AF3 e=CasLoad(sp+ASU2(-1, 0));
  AF3 f=CasLoad(sp);
  AF3 c=CasLoad(sp+ASU2( 1,-1));
  AF3 d=CasLoad(sp+ASU2( 2,-1));
  AF3 g=CasLoad(sp+ASU2( 1, 0));
  AF3 h=CasLoad(sp+ASU2( 2, 0));
  AF3 i=CasLoad(sp+ASU2(-1, 1));
  AF3 j=CasLoad(sp+ASU2( 0, 1));
  AF3 m=CasLoad(sp+ASU2(-1, 2));
  AF3 n=CasLoad(sp+ASU2( 0, 2));
  AF3 k=CasLoad(sp+ASU2( 1, 1));
  AF3 l=CasLoad(sp+ASU2( 2, 1));
  AF3 o=CasLoad(sp+ASU2( 1, 2));
  AF3 p=CasLoad(sp+ASU2( 2, 2));
  // Run optional input transform.
  CasInput(a.r,a.g,a.b);
  CasInput(b.r,b.g,b.b);
  CasInput(c.r,c.g,c.b);
  CasInput(d.r,d.g,d.b);
  CasInput(e.r,e.g,e.b);
  CasInput(f.r,f.g,f.b);
  CasInput(g.r,g.g,g.b);
  CasInput(h.r,h.g,h.b);
  CasInput(i.r,i.g,i.b);
  CasInput(j.r,j.g,j.b);
  CasInput(k.r,k.g,k.b);
  CasInput(l.r,l.g,l.b);
  CasInput(m.r,m.g,m.b);
  CasInput(n.r,n.g,n.b);
  CasInput(o.r,o.g,o.b);
  CasInput(p.r,p.g,p.b);
  // Soft min and max.
  // These are 2.0x bigger (factored out the extra multiply).
  //  a b c             b
  //  e f g * 0.5  +  e f g * 0.5  [F]
  //  i j k             j
  AF1 mnfR=AMin3F1(AMin3F1(b.r,e.r,f.r),g.r,j.r);
  AF1 mnfG=AMin3F1(AMin3F1(b.g,e.g,f.g),g.g,j.g);
  AF1 mnfB=AMin3F1(AMin3F1(b.b,e.b,f.b),g.b,j.b);
  #ifdef CAS_BETTER_DIAGONALS
   AF1 mnfR2=AMin3F1(AMin3F1(mnfR,a.r,c.r),i.r,k.r);
   AF1 mnfG2=AMin3F1(AMin3F1(mnfG,a.g,c.g),i.g,k.g);
   AF1 mnfB2=AMin3F1(AMin3F1(mnfB,a.b,c.b),i.b,k.b);
   mnfR=mnfR+mnfR2;
   mnfG=mnfG+mnfG2;
   mnfB=mnfB+mnfB2;
  #endif
  AF1 mxfR=AMax3F1(AMax3F1(b.r,e.r,f.r),g.r,j.r);
  AF1 mxfG=AMax3F1(AMax3F1(b.g,e.g,f.g),g.g,j.g);
  AF1 mxfB=AMax3F1(AMax3F1(b.b,e.b,f.b),g.b,j.b);
  #ifdef CAS_BETTER_DIAGONALS
   AF1 mxfR2=AMax3F1(AMax3F1(mxfR,a.r,c.r),i.r,k.r);
   AF1 mxfG2=AMax3F1(AMax3F1(mxfG,a.g,c.g),i.g,k.g);
   AF1 mxfB2=AMax3F1(AMax3F1(mxfB,a.b,c.b),i.b,k.b);
   mxfR=mxfR+mxfR2;
   mxfG=mxfG+mxfG2;
   mxfB=mxfB+mxfB2;
  #endif
  //  b c d             c
  //  f g h * 0.5  +  f g h * 0.5  [G]
  //  j k l             k
  AF1 mngR=AMin3F1(AMin3F1(c.r,f.r,g.r),h.r,k.r);
  AF1 mngG=AMin3F1(AMin3F1(c.g,f.g,g.g),h.g,k.g);
  AF1 mngB=AMin3F1(AMin3F1(c.b,f.b,g.b),h.b,k.b);
  #ifdef CAS_BETTER_DIAGONALS
   AF1 mngR2=AMin3F1(AMin3F1(mngR,b.r,d.r),j.r,l.r);
   AF1 mngG2=AMin3F1(AMin3F1(mngG,b.g,d.g),j.g,l.g);
   AF1 mngB2=AMin3F1(AMin3F1(mngB,b.b,d.b),j.b,l.b);
   mngR=mngR+mngR2;
   mngG=mngG+mngG2;
   mngB=mngB+mngB2;
  #endif
  AF1 mxgR=AMax3F1(AMax3F1(c.r,f.r,g.r),h.r,k.r);
  AF1 mxgG=AMax3F1(AMax3F1(c.g,f.g,g.g),h.g,k.g);
  AF1 mxgB=AMax3F1(AMax3F1(c.b,f.b,g.b),h.b,k.b);
  #ifdef CAS_BETTER_DIAGONALS
   AF1 mxgR2=AMax3F1(AMax3F1(mxgR,b.r,d.r),j.r,l.r);
   AF1 mxgG2=AMax3F1(AMax3F1(mxgG,b.g,d.g),j.g,l.g);
   AF1 mxgB2=AMax3F1(AMax3F1(mxgB,b.b,d.b),j.b,l.b);
   mxgR=mxgR+mxgR2;
   mxgG=mxgG+mxgG2;
   mxgB=mxgB+mxgB2;
  #endif
  //  e f g             f
  //  i j k * 0.5  +  i j k * 0.5  [J]
  //  m n o             n
  AF1 mnjR=AMin3F1(AMin3F1(f.r,i.r,j.r),k.r,n.r);
  AF1 mnjG=AMin3F1(AMin3F1(f.g,i.g,j.g),k.g,n.g);
  AF1 mnjB=AMin3F1(AMin3F1(f.b,i.b,j.b),k.b,n.b);
  #ifdef CAS_BETTER_DIAGONALS
   AF1 mnjR2=AMin3F1(AMin3F1(mnjR,e.r,g.r),m.r,o.r);
   AF1 mnjG2=AMin3F1(AMin3F1(mnjG,e.g,g.g),m.g,o.g);
   AF1 mnjB2=AMin3F1(AMin3F1(mnjB,e.b,g.b),m.b,o.b);
   mnjR=mnjR+mnjR2;
   mnjG=mnjG+mnjG2;
   mnjB=mnjB+mnjB2;
  #endif
  AF1 mxjR=AMax3F1(AMax3F1(f.r,i.r,j.r),k.r,n.r);
  AF1 mxjG=AMax3F1(AMax3F1(f.g,i.g,j.g),k.g,n.g);
  AF1 mxjB=AMax3F1(AMax3F1(f.b,i.b,j.b),k.b,n.b);
  #ifdef CAS_BETTER_DIAGONALS
   AF1 mxjR2=AMax3F1(AMax3F1(mxjR,e.r,g.r),m.r,o.r);
   AF1 mxjG2=AMax3F1(AMax3F1(mxjG,e.g,g.g),m.g,o.g);
   AF1 mxjB2=AMax3F1(AMax3F1(mxjB,e.b,g.b),m.b,o.b);
   mxjR=mxjR+mxjR2;
   mxjG=mxjG+mxjG2;
   mxjB=mxjB+mxjB2;
  #endif
  //  f g h             g
  //  j k l * 0.5  +  j k l * 0.5  [K]
  //  n o p             o
  AF1 mnkR=AMin3F1(AMin3F1(g.r,j.r,k.r),l.r,o.r);
  AF1 mnkG=AMin3F1(AMin3F1(g.g,j.g,k.g),l.g,o.g);
  AF1 mnkB=AMin3F1(AMin3F1(g.b,j.b,k.b),l.b,o.b);
  #ifdef CAS_BETTER_DIAGONALS
   AF1 mnkR2=AMin3F1(AMin3F1(mnkR,f.r,h.r),n.r,p.r);
   AF1 mnkG2=AMin3F1(AMin3F1(mnkG,f.g,h.g),n.g,p.g);
   AF1 mnkB2=AMin3F1(AMin3F1(mnkB,f.b,h.b),n.b,p.b);
   mnkR=mnkR+mnkR2;
   mnkG=mnkG+mnkG2;
   mnkB=mnkB+mnkB2;
  #endif
  AF1 mxkR=AMax3F1(AMax3F1(g.r,j.r,k.r),l.r,o.r);
  AF1 mxkG=AMax3F1(AMax3F1(g.g,j.g,k.g),l.g,o.g);
  AF1 mxkB=AMax3F1(AMax3F1(g.b,j.b,k.b),l.b,o.b);
  #ifdef CAS_BETTER_DIAGONALS
   AF1 mxkR2=AMax3F1(AMax3F1(mxkR,f.r,h.r),n.r,p.r);
   AF1 mxkG2=AMax3F1(AMax3F1(mxkG,f.g,h.g),n.g,p.g);
   AF1 mxkB2=AMax3F1(AMax3F1(mxkB,f.b,h.b),n.b,p.b);
   mxkR=mxkR+mxkR2;
   mxkG=mxkG+mxkG2;
   mxkB=mxkB+mxkB2;
  #endif
  // Smooth minimum distance to signal limit divided by smooth max.
  #ifdef CAS_GO_SLOWER
   AF1 rcpMfR=ARcpF1(mxfR);
   AF1 rcpMfG=ARcpF1(mxfG);
   AF1 rcpMfB=ARcpF1(mxfB);
   AF1 rcpMgR=ARcpF1(mxgR);
   AF1 rcpMgG=ARcpF1(mxgG);
   AF1 rcpMgB=ARcpF1(mxgB);
   AF1 rcpMjR=ARcpF1(mxjR);
   AF1 rcpMjG=ARcpF1(mxjG);
   AF1 rcpMjB=ARcpF1(mxjB);
   AF1 rcpMkR=ARcpF1(mxkR);
   AF1 rcpMkG=ARcpF1(mxkG);
   AF1 rcpMkB=ARcpF1(mxkB);
  #else
   AF1 rcpMfR=APrxLoRcpF1(mxfR);
   AF1 rcpMfG=APrxLoRcpF1(mxfG);
   AF1 rcpMfB=APrxLoRcpF1(mxfB);
   AF1 rcpMgR=APrxLoRcpF1(mxgR);
   AF1 rcpMgG=APrxLoRcpF1(mxgG);
   AF1 rcpMgB=APrxLoRcpF1(mxgB);
   AF1 rcpMjR=APrxLoRcpF1(mxjR);
   AF1 rcpMjG=APrxLoRcpF1(mxjG);
   AF1 rcpMjB=APrxLoRcpF1(mxjB);
   AF1 rcpMkR=APrxLoRcpF1(mxkR);
   AF1 rcpMkG=APrxLoRcpF1(mxkG);
   AF1 rcpMkB=APrxLoRcpF1(mxkB);
  #endif
  #ifdef CAS_BETTER_DIAGONALS
   AF1 ampfR=ASatF1(min(mnfR,AF1_(2.0)-mxfR)*rcpMfR);
   AF1 ampfG=ASatF1(min(mnfG,AF1_(2.0)-mxfG)*rcpMfG);
   AF1 ampfB=ASatF1(min(mnfB,AF1_(2.0)-mxfB)*rcpMfB);
   AF1 ampgR=ASatF1(min(mngR,AF1_(2.0)-mxgR)*rcpMgR);
   AF1 ampgG=ASatF1(min(mngG,AF1_(2.0)-mxgG)*rcpMgG);
   AF1 ampgB=ASatF1(min(mngB,AF1_(2.0)-mxgB)*rcpMgB);
   AF1 ampjR=ASatF1(min(mnjR,AF1_(2.0)-mxjR)*rcpMjR);
   AF1 ampjG=ASatF1(min(mnjG,AF1_(2.0)-mxjG)*rcpMjG);
   AF1 ampjB=ASatF1(min(mnjB,AF1_(2.0)-mxjB)*rcpMjB);
   AF1 ampkR=ASatF1(min(mnkR,AF1_(2.0)-mxkR)*rcpMkR);
   AF1 ampkG=ASatF1(min(mnkG,AF1_(2.0)-mxkG)*rcpMkG);
   AF1 ampkB=ASatF1(min(mnkB,AF1_(2.0)-mxkB)*rcpMkB);
  #else
   AF1 ampfR=ASatF1(min(mnfR,AF1_(1.0)-mxfR)*rcpMfR);
   AF1 ampfG=ASatF1(min(mnfG,AF1_(1.0)-mxfG)*rcpMfG);
   AF1 ampfB=ASatF1(min(mnfB,AF1_(1.0)-mxfB)*rcpMfB);
   AF1 ampgR=ASatF1(min(mngR,AF1_(1.0)-mxgR)*rcpMgR);
   AF1 ampgG=ASatF1(min(mngG,AF1_(1.0)-mxgG)*rcpMgG);
   AF1 ampgB=ASatF1(min(mngB,AF1_(1.0)-mxgB)*rcpMgB);
   AF1 ampjR=ASatF1(min(mnjR,AF1_(1.0)-mxjR)*rcpMjR);
   AF1 ampjG=ASatF1(min(mnjG,AF1_(1.0)-mxjG)*rcpMjG);
   AF1 ampjB=ASatF1(min(mnjB,AF1_(1.0)-mxjB)*rcpMjB);
   AF1 ampkR=ASatF1(min(mnkR,AF1_(1.0)-mxkR)*rcpMkR);
   AF1 ampkG=ASatF1(min(mnkG,AF1_(1.0)-mxkG)*rcpMkG);
   AF1 ampkB=ASatF1(min(mnkB,AF1_(1.0)-mxkB)*rcpMkB);
  #endif
  // Shaping amount of sharpening.
  #ifdef CAS_GO_SLOWER
   ampfR=sqrt(ampfR);
   ampfG=sqrt(ampfG);
   ampfB=sqrt(ampfB);
   ampgR=sqrt(ampgR);
   ampgG=sqrt(ampgG);
   ampgB=sqrt(ampgB);
   ampjR=sqrt(ampjR);
   ampjG=sqrt(ampjG);
   ampjB=sqrt(ampjB);
   ampkR=sqrt(ampkR);
   ampkG=sqrt(ampkG);
   ampkB=sqrt(ampkB);
  #else
   ampfR=APrxLoSqrtF1(ampfR);
   ampfG=APrxLoSqrtF1(ampfG);
   ampfB=APrxLoSqrtF1(ampfB);
   ampgR=APrxLoSqrtF1(ampgR);
   ampgG=APrxLoSqrtF1(ampgG);
   ampgB=APrxLoSqrtF1(ampgB);
   ampjR=APrxLoSqrtF1(ampjR);
   ampjG=APrxLoSqrtF1(ampjG);
   ampjB=APrxLoSqrtF1(ampjB);
   ampkR=APrxLoSqrtF1(ampkR);
   ampkG=APrxLoSqrtF1(ampkG);
   ampkB=APrxLoSqrtF1(ampkB);
  #endif
  // Filter shape.
  //  0 w 0
  //  w 1 w
  //  0 w 0
  AF1 peak=AF1_AU1(const1.x);
  AF1 wfR=ampfR*peak;
  AF1 wfG=ampfG*peak;
  AF1 wfB=ampfB*peak;
  AF1 wgR=ampgR*peak;
  AF1 wgG=ampgG*peak;
  AF1 wgB=ampgB*peak;
  AF1 wjR=ampjR*peak;
  AF1 wjG=ampjG*peak;
  AF1 wjB=ampjB*peak;
  AF1 wkR=ampkR*peak;
  AF1 wkG=ampkG*peak;
  AF1 wkB=ampkB*peak;
  // Blend between 4 results.
  //  s t
  //  u v
  AF1 s=(AF1_(1.0)-pp.x)*(AF1_(1.0)-pp.y);
  AF1 t=           pp.x *(AF1_(1.0)-pp.y);
  AF1 u=(AF1_(1.0)-pp.x)*           pp.y ;
  AF1 v=           pp.x *           pp.y ;
  // Thin edges to hide bilinear interpolation (helps diagonals).
  AF1 thinB=1.0/32.0;
  #ifdef CAS_GO_SLOWER
   s*=ARcpF1(thinB+(mxfG-mnfG));
   t*=ARcpF1(thinB+(mxgG-mngG));
   u*=ARcpF1(thinB+(mxjG-mnjG));
   v*=ARcpF1(thinB+(mxkG-mnkG));
  #else
   s*=APrxLoRcpF1(thinB+(mxfG-mnfG));
   t*=APrxLoRcpF1(thinB+(mxgG-mngG));
   u*=APrxLoRcpF1(thinB+(mxjG-mnjG));
   v*=APrxLoRcpF1(thinB+(mxkG-mnkG));
  #endif
  // Final weighting.
  //    b c
  //  e f g h
  //  i j k l
  //    n o
  //  _____  _____  _____  _____
  //         fs        gt 
  //
  //  _____  _____  _____  _____
  //  fs      s gt  fs  t     gt
  //         ju        kv
  //  _____  _____  _____  _____
  //         fs        gt
  //  ju      u kv  ju  v     kv
  //  _____  _____  _____  _____
  //
  //         ju        kv
  AF1 qbeR=wfR*s;
  AF1 qbeG=wfG*s;
  AF1 qbeB=wfB*s;
  AF1 qchR=wgR*t;
  AF1 qchG=wgG*t;
  AF1 qchB=wgB*t;
  AF1 qfR=wgR*t+wjR*u+s;
  AF1 qfG=wgG*t+wjG*u+s;
  AF1 qfB=wgB*t+wjB*u+s;
  AF1 qgR=wfR*s+wkR*v+t;
  AF1 qgG=wfG*s+wkG*v+t;
  AF1 qgB=wfB*s+wkB*v+t;
  AF1 qjR=wfR*s+wkR*v+u;
  AF1 qjG=wfG*s+wkG*v+u;
  AF1 qjB=wfB*s+wkB*v+u;
  AF1 qkR=wgR*t+wjR*u+v;
  AF1 qkG=wgG*t+wjG*u+v;
  AF1 qkB=wgB*t+wjB*u+v;
  AF1 qinR=wjR*u;
  AF1 qinG=wjG*u;
  AF1 qinB=wjB*u;
  AF1 qloR=wkR*v;
  AF1 qloG=wkG*v;
  AF1 qloB=wkB*v;
  // Filter.
  #ifndef CAS_SLOW
   // Using green coef only, depending on dead code removal to strip out the extra overhead.
   #ifdef CAS_GO_SLOWER
    AF1 rcpWG=ARcpF1(AF1_(2.0)*qbeG+AF1_(2.0)*qchG+AF1_(2.0)*qinG+AF1_(2.0)*qloG+qfG+qgG+qjG+qkG);
   #else
    AF1 rcpWG=APrxMedRcpF1(AF1_(2.0)*qbeG+AF1_(2.0)*qchG+AF1_(2.0)*qinG+AF1_(2.0)*qloG+qfG+qgG+qjG+qkG);
   #endif
   pixR=ASatF1((b.r*qbeG+e.r*qbeG+c.r*qchG+h.r*qchG+i.r*qinG+n.r*qinG+l.r*qloG+o.r*qloG+f.r*qfG+g.r*qgG+j.r*qjG+k.r*qkG)*rcpWG);
   pixG=ASatF1((b.g*qbeG+e.g*qbeG+c.g*qchG+h.g*qchG+i.g*qinG+n.g*qinG+l.g*qloG+o.g*qloG+f.g*qfG+g.g*qgG+j.g*qjG+k.g*qkG)*rcpWG);
   pixB=ASatF1((b.b*qbeG+e.b*qbeG+c.b*qchG+h.b*qchG+i.b*qinG+n.b*qinG+l.b*qloG+o.b*qloG+f.b*qfG+g.b*qgG+j.b*qjG+k.b*qkG)*rcpWG);
  #else
   #ifdef CAS_GO_SLOWER
    AF1 rcpWR=ARcpF1(AF1_(2.0)*qbeR+AF1_(2.0)*qchR+AF1_(2.0)*qinR+AF1_(2.0)*qloR+qfR+qgR+qjR+qkR);
    AF1 rcpWG=ARcpF1(AF1_(2.0)*qbeG+AF1_(2.0)*qchG+AF1_(2.0)*qinG+AF1_(2.0)*qloG+qfG+qgG+qjG+qkG);
    AF1 rcpWB=ARcpF1(AF1_(2.0)*qbeB+AF1_(2.0)*qchB+AF1_(2.0)*qinB+AF1_(2.0)*qloB+qfB+qgB+qjB+qkB);
   #else
    AF1 rcpWR=APrxMedRcpF1(AF1_(2.0)*qbeR+AF1_(2.0)*qchR+AF1_(2.0)*qinR+AF1_(2.0)*qloR+qfR+qgR+qjR+qkR);
    AF1 rcpWG=APrxMedRcpF1(AF1_(2.0)*qbeG+AF1_(2.0)*qchG+AF1_(2.0)*qinG+AF1_(2.0)*qloG+qfG+qgG+qjG+qkG);
    AF1 rcpWB=APrxMedRcpF1(AF1_(2.0)*qbeB+AF1_(2.0)*qchB+AF1_(2.0)*qinB+AF1_(2.0)*qloB+qfB+qgB+qjB+qkB);
   #endif
   pixR=ASatF1((b.r*qbeR+e.r*qbeR+c.r*qchR+h.r*qchR+i.r*qinR+n.r*qinR+l.r*qloR+o.r*qloR+f.r*qfR+g.r*qgR+j.r*qjR+k.r*qkR)*rcpWR);
   pixG=ASatF1((b.g*qbeG+e.g*qbeG+c.g*qchG+h.g*qchG+i.g*qinG+n.g*qinG+l.g*qloG+o.g*qloG+f.g*qfG+g.g*qgG+j.g*qjG+k.g*qkG)*rcpWG);
   pixB=ASatF1((b.b*qbeB+e.b*qbeB+c.b*qchB+h.b*qchB+i.b*qinB+n.b*qinB+l.b*qloB+o.b*qloB+f.b*qfB+g.b*qgB+j.b*qjB+k.b*qkB)*rcpWB);
  #endif
 }
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                       PACKED VERSION
//==============================================================================================================================
#if defined(A_GPU) && defined(A_HALF)
 // Missing a way to do packed re-interpetation, so must disable approximation optimizations.
 #ifdef A_HLSL
  #ifndef CAS_GO_SLOWER
   #define CAS_GO_SLOWER 1
  #endif
 #endif
//==============================================================================================================================
 // Can be used to convert from packed SOA to AOS for store.
 void CasDepack(out AH4 pix0,out AH4 pix1,AH2 pixR,AH2 pixG,AH2 pixB){
  #ifdef A_HLSL
   // Invoke a slower path for DX only, since it won't allow uninitialized values.
   pix0.a=pix1.a=0.0;
  #endif
  pix0.rgb=AH3(pixR.x,pixG.x,pixB.x);
  pix1.rgb=AH3(pixR.y,pixG.y,pixB.y);}
//==============================================================================================================================
 void CasFilterH(
 // Output values are for 2 8x8 tiles in a 16x8 region.
 //  pix<R,G,B>.x = right 8x8 tile
 //  pix<R,G,B>.y =  left 8x8 tile
 // This enables later processing to easily be packed as well.
 out AH2 pixR,
 out AH2 pixG,
 out AH2 pixB,
 AU2 ip, // Integer pixel position in output.
 AU4 const0, // Constants generated by CasSetup().
 AU4 const1,
 AP1 noScaling){ // Must be a compile-time literal value, true = sharpen only (no resize).
//------------------------------------------------------------------------------------------------------------------------------
  // Debug a checker pattern of on/off tiles for visual inspection.
  #ifdef CAS_DEBUG_CHECKER
   if((((ip.x^ip.y)>>8u)&1u)==0u){AH3 pix0=CasLoadH(ASW2(ip));AH3 pix1=CasLoadH(ASW2(ip)+ASW2(8,0));
    pixR=AH2(pix0.r,pix1.r);pixG=AH2(pix0.g,pix1.g);pixB=AH2(pix0.b,pix1.b);CasInputH(pixR,pixG,pixB);return;}
  #endif 
//------------------------------------------------------------------------------------------------------------------------------
  // No scaling algorithm uses minimal 3x3 pixel neighborhood.
  if(noScaling){
   ASW2 sp0=ASW2(ip);
   AH3 a0=CasLoadH(sp0+ASW2(-1,-1));
   AH3 b0=CasLoadH(sp0+ASW2( 0,-1));
   AH3 c0=CasLoadH(sp0+ASW2( 1,-1));
   AH3 d0=CasLoadH(sp0+ASW2(-1, 0));
   AH3 e0=CasLoadH(sp0);
   AH3 f0=CasLoadH(sp0+ASW2( 1, 0));
   AH3 g0=CasLoadH(sp0+ASW2(-1, 1));
   AH3 h0=CasLoadH(sp0+ASW2( 0, 1));
   AH3 i0=CasLoadH(sp0+ASW2( 1, 1));
   ASW2 sp1=sp0+ASW2(8,0);
   AH3 a1=CasLoadH(sp1+ASW2(-1,-1));
   AH3 b1=CasLoadH(sp1+ASW2( 0,-1));
   AH3 c1=CasLoadH(sp1+ASW2( 1,-1));
   AH3 d1=CasLoadH(sp1+ASW2(-1, 0));
   AH3 e1=CasLoadH(sp1);
   AH3 f1=CasLoadH(sp1+ASW2( 1, 0));
   AH3 g1=CasLoadH(sp1+ASW2(-1, 1));
   AH3 h1=CasLoadH(sp1+ASW2( 0, 1));
   AH3 i1=CasLoadH(sp1+ASW2( 1, 1));
   // AOS to SOA conversion.
   AH2 aR=AH2(a0.r,a1.r);
   AH2 aG=AH2(a0.g,a1.g);
   AH2 aB=AH2(a0.b,a1.b);
   AH2 bR=AH2(b0.r,b1.r);
   AH2 bG=AH2(b0.g,b1.g);
   AH2 bB=AH2(b0.b,b1.b);
   AH2 cR=AH2(c0.r,c1.r);
   AH2 cG=AH2(c0.g,c1.g);
   AH2 cB=AH2(c0.b,c1.b);
   AH2 dR=AH2(d0.r,d1.r);
   AH2 dG=AH2(d0.g,d1.g);
   AH2 dB=AH2(d0.b,d1.b);
   AH2 eR=AH2(e0.r,e1.r);
   AH2 eG=AH2(e0.g,e1.g);
   AH2 eB=AH2(e0.b,e1.b);
   AH2 fR=AH2(f0.r,f1.r);
   AH2 fG=AH2(f0.g,f1.g);
   AH2 fB=AH2(f0.b,f1.b);
   AH2 gR=AH2(g0.r,g1.r);
   AH2 gG=AH2(g0.g,g1.g);
   AH2 gB=AH2(g0.b,g1.b);
   AH2 hR=AH2(h0.r,h1.r);
   AH2 hG=AH2(h0.g,h1.g);
   AH2 hB=AH2(h0.b,h1.b);
   AH2 iR=AH2(i0.r,i1.r);
   AH2 iG=AH2(i0.g,i1.g);
   AH2 iB=AH2(i0.b,i1.b);
   // Run optional input transform.
   CasInputH(aR,aG,aB);
   CasInputH(bR,bG,bB);
   CasInputH(cR,cG,cB);
   CasInputH(dR,dG,dB);
   CasInputH(eR,eG,eB);
   CasInputH(fR,fG,fB);
   CasInputH(gR,gG,gB);
   CasInputH(hR,hG,hB);
   CasInputH(iR,iG,iB);
   // Soft min and max.
   AH2 mnR=min(min(fR,hR),min(min(bR,dR),eR));
   AH2 mnG=min(min(fG,hG),min(min(bG,dG),eG));
   AH2 mnB=min(min(fB,hB),min(min(bB,dB),eB));
   #ifdef CAS_BETTER_DIAGONALS
    AH2 mnR2=min(min(gR,iR),min(min(aR,cR),mnR));
    AH2 mnG2=min(min(gG,iG),min(min(aG,cG),mnG));
    AH2 mnB2=min(min(gB,iB),min(min(aB,cB),mnB));
    mnR=mnR+mnR2;
    mnG=mnG+mnG2;
    mnB=mnB+mnB2;
   #endif
   AH2 mxR=max(max(fR,hR),max(max(bR,dR),eR));
   AH2 mxG=max(max(fG,hG),max(max(bG,dG),eG));
   AH2 mxB=max(max(fB,hB),max(max(bB,dB),eB));
   #ifdef CAS_BETTER_DIAGONALS
    AH2 mxR2=max(max(gR,iR),max(max(aR,cR),mxR));
    AH2 mxG2=max(max(gG,iG),max(max(aG,cG),mxG));
    AH2 mxB2=max(max(gB,iB),max(max(aB,cB),mxB));
    mxR=mxR+mxR2;
    mxG=mxG+mxG2;
    mxB=mxB+mxB2;
   #endif
   // Smooth minimum distance to signal limit divided by smooth max.
   #ifdef CAS_GO_SLOWER
    AH2 rcpMR=ARcpH2(mxR);
    AH2 rcpMG=ARcpH2(mxG);
    AH2 rcpMB=ARcpH2(mxB);
   #else
    AH2 rcpMR=APrxLoRcpH2(mxR);
    AH2 rcpMG=APrxLoRcpH2(mxG);
    AH2 rcpMB=APrxLoRcpH2(mxB);
   #endif
   #ifdef CAS_BETTER_DIAGONALS
    AH2 ampR=ASatH2(min(mnR,AH2_(2.0)-mxR)*rcpMR);
    AH2 ampG=ASatH2(min(mnG,AH2_(2.0)-mxG)*rcpMG);
    AH2 ampB=ASatH2(min(mnB,AH2_(2.0)-mxB)*rcpMB);
   #else
    AH2 ampR=ASatH2(min(mnR,AH2_(1.0)-mxR)*rcpMR);
    AH2 ampG=ASatH2(min(mnG,AH2_(1.0)-mxG)*rcpMG);
    AH2 ampB=ASatH2(min(mnB,AH2_(1.0)-mxB)*rcpMB);
   #endif
   // Shaping amount of sharpening.
   #ifdef CAS_GO_SLOWER
    ampR=sqrt(ampR);
    ampG=sqrt(ampG);
    ampB=sqrt(ampB);
   #else
    ampR=APrxLoSqrtH2(ampR);
    ampG=APrxLoSqrtH2(ampG);
    ampB=APrxLoSqrtH2(ampB);
   #endif
   // Filter shape.
   AH1 peak=AH2_AU1(const1.y).x;
   AH2 wR=ampR*AH2_(peak);
   AH2 wG=ampG*AH2_(peak);
   AH2 wB=ampB*AH2_(peak);
   // Filter.
   #ifndef CAS_SLOW
    #ifdef CAS_GO_SLOWER
     AH2 rcpWeight=ARcpH2(AH2_(1.0)+AH2_(4.0)*wG);
    #else
     AH2 rcpWeight=APrxMedRcpH2(AH2_(1.0)+AH2_(4.0)*wG);
    #endif
    pixR=ASatH2((bR*wG+dR*wG+fR*wG+hR*wG+eR)*rcpWeight);
    pixG=ASatH2((bG*wG+dG*wG+fG*wG+hG*wG+eG)*rcpWeight);
    pixB=ASatH2((bB*wG+dB*wG+fB*wG+hB*wG+eB)*rcpWeight);
   #else
    #ifdef CAS_GO_SLOWER
     AH2 rcpWeightR=ARcpH2(AH2_(1.0)+AH2_(4.0)*wR);
     AH2 rcpWeightG=ARcpH2(AH2_(1.0)+AH2_(4.0)*wG);
     AH2 rcpWeightB=ARcpH2(AH2_(1.0)+AH2_(4.0)*wB);
    #else
     AH2 rcpWeightR=APrxMedRcpH2(AH2_(1.0)+AH2_(4.0)*wR);
     AH2 rcpWeightG=APrxMedRcpH2(AH2_(1.0)+AH2_(4.0)*wG);
     AH2 rcpWeightB=APrxMedRcpH2(AH2_(1.0)+AH2_(4.0)*wB);
    #endif
    pixR=ASatH2((bR*wR+dR*wR+fR*wR+hR*wR+eR)*rcpWeightR);
    pixG=ASatH2((bG*wG+dG*wG+fG*wG+hG*wG+eG)*rcpWeightG);
    pixB=ASatH2((bB*wB+dB*wB+fB*wB+hB*wB+eB)*rcpWeightB);
   #endif
   return;}
//------------------------------------------------------------------------------------------------------------------------------
  // Scaling algorithm adaptively interpolates between nearest 4 results of the non-scaling algorithm.
  AF2 pp=AF2(ip)*AF2_AU2(const0.xy)+AF2_AU2(const0.zw);
  // Tile 0.
  // Fractional position is needed in high precision here.
  AF2 fp0=floor(pp);
  AH2 ppX;
  ppX.x=AH1(pp.x-fp0.x);
  AH1 ppY=AH1(pp.y-fp0.y);
  ASW2 sp0=ASW2(fp0);
  AH3 a0=CasLoadH(sp0+ASW2(-1,-1));
  AH3 b0=CasLoadH(sp0+ASW2( 0,-1));
  AH3 e0=CasLoadH(sp0+ASW2(-1, 0));
  AH3 f0=CasLoadH(sp0);
  AH3 c0=CasLoadH(sp0+ASW2( 1,-1));
  AH3 d0=CasLoadH(sp0+ASW2( 2,-1));
  AH3 g0=CasLoadH(sp0+ASW2( 1, 0));
  AH3 h0=CasLoadH(sp0+ASW2( 2, 0));
  AH3 i0=CasLoadH(sp0+ASW2(-1, 1));
  AH3 j0=CasLoadH(sp0+ASW2( 0, 1));
  AH3 m0=CasLoadH(sp0+ASW2(-1, 2));
  AH3 n0=CasLoadH(sp0+ASW2( 0, 2));
  AH3 k0=CasLoadH(sp0+ASW2( 1, 1));
  AH3 l0=CasLoadH(sp0+ASW2( 2, 1));
  AH3 o0=CasLoadH(sp0+ASW2( 1, 2));
  AH3 p0=CasLoadH(sp0+ASW2( 2, 2));
  // Tile 1 (offset only in x).
  AF1 pp1=pp.x+AF1_AU1(const1.z);
  AF1 fp1=floor(pp1);
  ppX.y=AH1(pp1-fp1);
  ASW2 sp1=ASW2(fp1,sp0.y);
  AH3 a1=CasLoadH(sp1+ASW2(-1,-1));
  AH3 b1=CasLoadH(sp1+ASW2( 0,-1));
  AH3 e1=CasLoadH(sp1+ASW2(-1, 0));
  AH3 f1=CasLoadH(sp1);
  AH3 c1=CasLoadH(sp1+ASW2( 1,-1));
  AH3 d1=CasLoadH(sp1+ASW2( 2,-1));
  AH3 g1=CasLoadH(sp1+ASW2( 1, 0));
  AH3 h1=CasLoadH(sp1+ASW2( 2, 0));
  AH3 i1=CasLoadH(sp1+ASW2(-1, 1));
  AH3 j1=CasLoadH(sp1+ASW2( 0, 1));
  AH3 m1=CasLoadH(sp1+ASW2(-1, 2));
  AH3 n1=CasLoadH(sp1+ASW2( 0, 2));
  AH3 k1=CasLoadH(sp1+ASW2( 1, 1));
  AH3 l1=CasLoadH(sp1+ASW2( 2, 1));
  AH3 o1=CasLoadH(sp1+ASW2( 1, 2));
  AH3 p1=CasLoadH(sp1+ASW2( 2, 2));
  // AOS to SOA conversion.
  AH2 aR=AH2(a0.r,a1.r);
  AH2 aG=AH2(a0.g,a1.g);
  AH2 aB=AH2(a0.b,a1.b);
  AH2 bR=AH2(b0.r,b1.r);
  AH2 bG=AH2(b0.g,b1.g);
  AH2 bB=AH2(b0.b,b1.b);
  AH2 cR=AH2(c0.r,c1.r);
  AH2 cG=AH2(c0.g,c1.g);
  AH2 cB=AH2(c0.b,c1.b);
  AH2 dR=AH2(d0.r,d1.r);
  AH2 dG=AH2(d0.g,d1.g);
  AH2 dB=AH2(d0.b,d1.b);
  AH2 eR=AH2(e0.r,e1.r);
  AH2 eG=AH2(e0.g,e1.g);
  AH2 eB=AH2(e0.b,e1.b);
  AH2 fR=AH2(f0.r,f1.r);
  AH2 fG=AH2(f0.g,f1.g);
  AH2 fB=AH2(f0.b,f1.b);
  AH2 gR=AH2(g0.r,g1.r);
  AH2 gG=AH2(g0.g,g1.g);
  AH2 gB=AH2(g0.b,g1.b);
  AH2 hR=AH2(h0.r,h1.r);
  AH2 hG=AH2(h0.g,h1.g);
  AH2 hB=AH2(h0.b,h1.b);
  AH2 iR=AH2(i0.r,i1.r);
  AH2 iG=AH2(i0.g,i1.g);
  AH2 iB=AH2(i0.b,i1.b);
  AH2 jR=AH2(j0.r,j1.r);
  AH2 jG=AH2(j0.g,j1.g);
  AH2 jB=AH2(j0.b,j1.b);
  AH2 kR=AH2(k0.r,k1.r);
  AH2 kG=AH2(k0.g,k1.g);
  AH2 kB=AH2(k0.b,k1.b);
  AH2 lR=AH2(l0.r,l1.r);
  AH2 lG=AH2(l0.g,l1.g);
  AH2 lB=AH2(l0.b,l1.b);
  AH2 mR=AH2(m0.r,m1.r);
  AH2 mG=AH2(m0.g,m1.g);
  AH2 mB=AH2(m0.b,m1.b);
  AH2 nR=AH2(n0.r,n1.r);
  AH2 nG=AH2(n0.g,n1.g);
  AH2 nB=AH2(n0.b,n1.b);
  AH2 oR=AH2(o0.r,o1.r);
  AH2 oG=AH2(o0.g,o1.g);
  AH2 oB=AH2(o0.b,o1.b);
  AH2 pR=AH2(p0.r,p1.r);
  AH2 pG=AH2(p0.g,p1.g);
  AH2 pB=AH2(p0.b,p1.b);
  // Run optional input transform.
  CasInputH(aR,aG,aB);
  CasInputH(bR,bG,bB);
  CasInputH(cR,cG,cB);
  CasInputH(dR,dG,dB);
  CasInputH(eR,eG,eB);
  CasInputH(fR,fG,fB);
  CasInputH(gR,gG,gB);
  CasInputH(hR,hG,hB);
  CasInputH(iR,iG,iB);
  CasInputH(jR,jG,jB);
  CasInputH(kR,kG,kB);
  CasInputH(lR,lG,lB);
  CasInputH(mR,mG,mB);
  CasInputH(nR,nG,nB);
  CasInputH(oR,oG,oB);
  CasInputH(pR,pG,pB);
  // Soft min and max.
  // These are 2.0x bigger (factored out the extra multiply).
  //  a b c             b
  //  e f g * 0.5  +  e f g * 0.5  [F]
  //  i j k             j
  AH2 mnfR=AMin3H2(AMin3H2(bR,eR,fR),gR,jR);
  AH2 mnfG=AMin3H2(AMin3H2(bG,eG,fG),gG,jG);
  AH2 mnfB=AMin3H2(AMin3H2(bB,eB,fB),gB,jB);
  #ifdef CAS_BETTER_DIAGONALS
   AH2 mnfR2=AMin3H2(AMin3H2(mnfR,aR,cR),iR,kR);
   AH2 mnfG2=AMin3H2(AMin3H2(mnfG,aG,cG),iG,kG);
   AH2 mnfB2=AMin3H2(AMin3H2(mnfB,aB,cB),iB,kB);
   mnfR=mnfR+mnfR2;
   mnfG=mnfG+mnfG2;
   mnfB=mnfB+mnfB2;
  #endif
  AH2 mxfR=AMax3H2(AMax3H2(bR,eR,fR),gR,jR);
  AH2 mxfG=AMax3H2(AMax3H2(bG,eG,fG),gG,jG);
  AH2 mxfB=AMax3H2(AMax3H2(bB,eB,fB),gB,jB);
  #ifdef CAS_BETTER_DIAGONALS
   AH2 mxfR2=AMax3H2(AMax3H2(mxfR,aR,cR),iR,kR);
   AH2 mxfG2=AMax3H2(AMax3H2(mxfG,aG,cG),iG,kG);
   AH2 mxfB2=AMax3H2(AMax3H2(mxfB,aB,cB),iB,kB);
   mxfR=mxfR+mxfR2;
   mxfG=mxfG+mxfG2;
   mxfB=mxfB+mxfB2;
  #endif
  //  b c d             c
  //  f g h * 0.5  +  f g h * 0.5  [G]
  //  j k l             k
  AH2 mngR=AMin3H2(AMin3H2(cR,fR,gR),hR,kR);
  AH2 mngG=AMin3H2(AMin3H2(cG,fG,gG),hG,kG);
  AH2 mngB=AMin3H2(AMin3H2(cB,fB,gB),hB,kB);
  #ifdef CAS_BETTER_DIAGONALS
   AH2 mngR2=AMin3H2(AMin3H2(mngR,bR,dR),jR,lR);
   AH2 mngG2=AMin3H2(AMin3H2(mngG,bG,dG),jG,lG);
   AH2 mngB2=AMin3H2(AMin3H2(mngB,bB,dB),jB,lB);
   mngR=mngR+mngR2;
   mngG=mngG+mngG2;
   mngB=mngB+mngB2;
  #endif
  AH2 mxgR=AMax3H2(AMax3H2(cR,fR,gR),hR,kR);
  AH2 mxgG=AMax3H2(AMax3H2(cG,fG,gG),hG,kG);
  AH2 mxgB=AMax3H2(AMax3H2(cB,fB,gB),hB,kB);
  #ifdef CAS_BETTER_DIAGONALS
   AH2 mxgR2=AMax3H2(AMax3H2(mxgR,bR,dR),jR,lR);
   AH2 mxgG2=AMax3H2(AMax3H2(mxgG,bG,dG),jG,lG);
   AH2 mxgB2=AMax3H2(AMax3H2(mxgB,bB,dB),jB,lB);
   mxgR=mxgR+mxgR2;
   mxgG=mxgG+mxgG2;
   mxgB=mxgB+mxgB2;
  #endif
  //  e f g             f
  //  i j k * 0.5  +  i j k * 0.5  [J]
  //  m n o             n
  AH2 mnjR=AMin3H2(AMin3H2(fR,iR,jR),kR,nR);
  AH2 mnjG=AMin3H2(AMin3H2(fG,iG,jG),kG,nG);
  AH2 mnjB=AMin3H2(AMin3H2(fB,iB,jB),kB,nB);
  #ifdef CAS_BETTER_DIAGONALS
   AH2 mnjR2=AMin3H2(AMin3H2(mnjR,eR,gR),mR,oR);
   AH2 mnjG2=AMin3H2(AMin3H2(mnjG,eG,gG),mG,oG);
   AH2 mnjB2=AMin3H2(AMin3H2(mnjB,eB,gB),mB,oB);
   mnjR=mnjR+mnjR2;
   mnjG=mnjG+mnjG2;
   mnjB=mnjB+mnjB2;
  #endif
  AH2 mxjR=AMax3H2(AMax3H2(fR,iR,jR),kR,nR);
  AH2 mxjG=AMax3H2(AMax3H2(fG,iG,jG),kG,nG);
  AH2 mxjB=AMax3H2(AMax3H2(fB,iB,jB),kB,nB);
  #ifdef CAS_BETTER_DIAGONALS
   AH2 mxjR2=AMax3H2(AMax3H2(mxjR,eR,gR),mR,oR);
   AH2 mxjG2=AMax3H2(AMax3H2(mxjG,eG,gG),mG,oG);
   AH2 mxjB2=AMax3H2(AMax3H2(mxjB,eB,gB),mB,oB);
   mxjR=mxjR+mxjR2;
   mxjG=mxjG+mxjG2;
   mxjB=mxjB+mxjB2;
  #endif
  //  f g h             g
  //  j k l * 0.5  +  j k l * 0.5  [K]
  //  n o p             o
  AH2 mnkR=AMin3H2(AMin3H2(gR,jR,kR),lR,oR);
  AH2 mnkG=AMin3H2(AMin3H2(gG,jG,kG),lG,oG);
  AH2 mnkB=AMin3H2(AMin3H2(gB,jB,kB),lB,oB);
  #ifdef CAS_BETTER_DIAGONALS
   AH2 mnkR2=AMin3H2(AMin3H2(mnkR,fR,hR),nR,pR);
   AH2 mnkG2=AMin3H2(AMin3H2(mnkG,fG,hG),nG,pG);
   AH2 mnkB2=AMin3H2(AMin3H2(mnkB,fB,hB),nB,pB);
   mnkR=mnkR+mnkR2;
   mnkG=mnkG+mnkG2;
   mnkB=mnkB+mnkB2;
  #endif
  AH2 mxkR=AMax3H2(AMax3H2(gR,jR,kR),lR,oR);
  AH2 mxkG=AMax3H2(AMax3H2(gG,jG,kG),lG,oG);
  AH2 mxkB=AMax3H2(AMax3H2(gB,jB,kB),lB,oB);
  #ifdef CAS_BETTER_DIAGONALS
   AH2 mxkR2=AMax3H2(AMax3H2(mxkR,fR,hR),nR,pR);
   AH2 mxkG2=AMax3H2(AMax3H2(mxkG,fG,hG),nG,pG);
   AH2 mxkB2=AMax3H2(AMax3H2(mxkB,fB,hB),nB,pB);
   mxkR=mxkR+mxkR2;
   mxkG=mxkG+mxkG2;
   mxkB=mxkB+mxkB2;
  #endif
  // Smooth minimum distance to signal limit divided by smooth max.
  #ifdef CAS_GO_SLOWER
   AH2 rcpMfR=ARcpH2(mxfR);
   AH2 rcpMfG=ARcpH2(mxfG);
   AH2 rcpMfB=ARcpH2(mxfB);
   AH2 rcpMgR=ARcpH2(mxgR);
   AH2 rcpMgG=ARcpH2(mxgG);
   AH2 rcpMgB=ARcpH2(mxgB);
   AH2 rcpMjR=ARcpH2(mxjR);
   AH2 rcpMjG=ARcpH2(mxjG);
   AH2 rcpMjB=ARcpH2(mxjB);
   AH2 rcpMkR=ARcpH2(mxkR);
   AH2 rcpMkG=ARcpH2(mxkG);
   AH2 rcpMkB=ARcpH2(mxkB);
  #else
   AH2 rcpMfR=APrxLoRcpH2(mxfR);
   AH2 rcpMfG=APrxLoRcpH2(mxfG);
   AH2 rcpMfB=APrxLoRcpH2(mxfB);
   AH2 rcpMgR=APrxLoRcpH2(mxgR);
   AH2 rcpMgG=APrxLoRcpH2(mxgG);
   AH2 rcpMgB=APrxLoRcpH2(mxgB);
   AH2 rcpMjR=APrxLoRcpH2(mxjR);
   AH2 rcpMjG=APrxLoRcpH2(mxjG);
   AH2 rcpMjB=APrxLoRcpH2(mxjB);
   AH2 rcpMkR=APrxLoRcpH2(mxkR);
   AH2 rcpMkG=APrxLoRcpH2(mxkG);
   AH2 rcpMkB=APrxLoRcpH2(mxkB);
  #endif
  #ifdef CAS_BETTER_DIAGONALS
   AH2 ampfR=ASatH2(min(mnfR,AH2_(2.0)-mxfR)*rcpMfR);
   AH2 ampfG=ASatH2(min(mnfG,AH2_(2.0)-mxfG)*rcpMfG);
   AH2 ampfB=ASatH2(min(mnfB,AH2_(2.0)-mxfB)*rcpMfB);
   AH2 ampgR=ASatH2(min(mngR,AH2_(2.0)-mxgR)*rcpMgR);
   AH2 ampgG=ASatH2(min(mngG,AH2_(2.0)-mxgG)*rcpMgG);
   AH2 ampgB=ASatH2(min(mngB,AH2_(2.0)-mxgB)*rcpMgB);
   AH2 ampjR=ASatH2(min(mnjR,AH2_(2.0)-mxjR)*rcpMjR);
   AH2 ampjG=ASatH2(min(mnjG,AH2_(2.0)-mxjG)*rcpMjG);
   AH2 ampjB=ASatH2(min(mnjB,AH2_(2.0)-mxjB)*rcpMjB);
   AH2 ampkR=ASatH2(min(mnkR,AH2_(2.0)-mxkR)*rcpMkR);
   AH2 ampkG=ASatH2(min(mnkG,AH2_(2.0)-mxkG)*rcpMkG);
   AH2 ampkB=ASatH2(min(mnkB,AH2_(2.0)-mxkB)*rcpMkB);
  #else
   AH2 ampfR=ASatH2(min(mnfR,AH2_(1.0)-mxfR)*rcpMfR);
   AH2 ampfG=ASatH2(min(mnfG,AH2_(1.0)-mxfG)*rcpMfG);
   AH2 ampfB=ASatH2(min(mnfB,AH2_(1.0)-mxfB)*rcpMfB);
   AH2 ampgR=ASatH2(min(mngR,AH2_(1.0)-mxgR)*rcpMgR);
   AH2 ampgG=ASatH2(min(mngG,AH2_(1.0)-mxgG)*rcpMgG);
   AH2 ampgB=ASatH2(min(mngB,AH2_(1.0)-mxgB)*rcpMgB);
   AH2 ampjR=ASatH2(min(mnjR,AH2_(1.0)-mxjR)*rcpMjR);
   AH2 ampjG=ASatH2(min(mnjG,AH2_(1.0)-mxjG)*rcpMjG);
   AH2 ampjB=ASatH2(min(mnjB,AH2_(1.0)-mxjB)*rcpMjB);
   AH2 ampkR=ASatH2(min(mnkR,AH2_(1.0)-mxkR)*rcpMkR);
   AH2 ampkG=ASatH2(min(mnkG,AH2_(1.0)-mxkG)*rcpMkG);
   AH2 ampkB=ASatH2(min(mnkB,AH2_(1.0)-mxkB)*rcpMkB);
  #endif
  // Shaping amount of sharpening.
  #ifdef CAS_GO_SLOWER
   ampfR=sqrt(ampfR);
   ampfG=sqrt(ampfG);
   ampfB=sqrt(ampfB);
   ampgR=sqrt(ampgR);
   ampgG=sqrt(ampgG);
   ampgB=sqrt(ampgB);
   ampjR=sqrt(ampjR);
   ampjG=sqrt(ampjG);
   ampjB=sqrt(ampjB);
   ampkR=sqrt(ampkR);
   ampkG=sqrt(ampkG);
   ampkB=sqrt(ampkB);
  #else
   ampfR=APrxLoSqrtH2(ampfR);
   ampfG=APrxLoSqrtH2(ampfG);
   ampfB=APrxLoSqrtH2(ampfB);
   ampgR=APrxLoSqrtH2(ampgR);
   ampgG=APrxLoSqrtH2(ampgG);
   ampgB=APrxLoSqrtH2(ampgB);
   ampjR=APrxLoSqrtH2(ampjR);
   ampjG=APrxLoSqrtH2(ampjG);
   ampjB=APrxLoSqrtH2(ampjB);
   ampkR=APrxLoSqrtH2(ampkR);
   ampkG=APrxLoSqrtH2(ampkG);
   ampkB=APrxLoSqrtH2(ampkB);
  #endif
  // Filter shape.
  AH1 peak=AH2_AU1(const1.y).x;
  AH2 wfR=ampfR*AH2_(peak);
  AH2 wfG=ampfG*AH2_(peak);
  AH2 wfB=ampfB*AH2_(peak);
  AH2 wgR=ampgR*AH2_(peak);
  AH2 wgG=ampgG*AH2_(peak);
  AH2 wgB=ampgB*AH2_(peak);
  AH2 wjR=ampjR*AH2_(peak);
  AH2 wjG=ampjG*AH2_(peak);
  AH2 wjB=ampjB*AH2_(peak);
  AH2 wkR=ampkR*AH2_(peak);
  AH2 wkG=ampkG*AH2_(peak);
  AH2 wkB=ampkB*AH2_(peak);
  // Blend between 4 results.
  AH2 s=(AH2_(1.0)-ppX)*(AH2_(1.0)-AH2_(ppY));
  AH2 t=           ppX *(AH2_(1.0)-AH2_(ppY));
  AH2 u=(AH2_(1.0)-ppX)*           AH2_(ppY) ;
  AH2 v=           ppX *           AH2_(ppY) ;
  // Thin edges to hide bilinear interpolation (helps diagonals).
  AH2 thinB=AH2_(1.0/32.0);
  #ifdef CAS_GO_SLOWER
   s*=ARcpH2(thinB+(mxfG-mnfG));
   t*=ARcpH2(thinB+(mxgG-mngG));
   u*=ARcpH2(thinB+(mxjG-mnjG));
   v*=ARcpH2(thinB+(mxkG-mnkG));
  #else
   s*=APrxLoRcpH2(thinB+(mxfG-mnfG));
   t*=APrxLoRcpH2(thinB+(mxgG-mngG));
   u*=APrxLoRcpH2(thinB+(mxjG-mnjG));
   v*=APrxLoRcpH2(thinB+(mxkG-mnkG));
  #endif
  // Final weighting.
  AH2 qbeR=wfR*s;
  AH2 qbeG=wfG*s;
  AH2 qbeB=wfB*s;
  AH2 qchR=wgR*t;
  AH2 qchG=wgG*t;
  AH2 qchB=wgB*t;
  AH2 qfR=wgR*t+wjR*u+s;
  AH2 qfG=wgG*t+wjG*u+s;
  AH2 qfB=wgB*t+wjB*u+s;
  AH2 qgR=wfR*s+wkR*v+t;
  AH2 qgG=wfG*s+wkG*v+t;
  AH2 qgB=wfB*s+wkB*v+t;
  AH2 qjR=wfR*s+wkR*v+u;
  AH2 qjG=wfG*s+wkG*v+u;
  AH2 qjB=wfB*s+wkB*v+u;
  AH2 qkR=wgR*t+wjR*u+v;
  AH2 qkG=wgG*t+wjG*u+v;
  AH2 qkB=wgB*t+wjB*u+v;
  AH2 qinR=wjR*u;
  AH2 qinG=wjG*u;
  AH2 qinB=wjB*u;
  AH2 qloR=wkR*v;
  AH2 qloG=wkG*v;
  AH2 qloB=wkB*v;
  // Filter.
  #ifndef CAS_SLOW
   #ifdef CAS_GO_SLOWER
    AH2 rcpWG=ARcpH2(AH2_(2.0)*qbeG+AH2_(2.0)*qchG+AH2_(2.0)*qinG+AH2_(2.0)*qloG+qfG+qgG+qjG+qkG);
   #else
    AH2 rcpWG=APrxMedRcpH2(AH2_(2.0)*qbeG+AH2_(2.0)*qchG+AH2_(2.0)*qinG+AH2_(2.0)*qloG+qfG+qgG+qjG+qkG);
   #endif
   pixR=ASatH2((bR*qbeG+eR*qbeG+cR*qchG+hR*qchG+iR*qinG+nR*qinG+lR*qloG+oR*qloG+fR*qfG+gR*qgG+jR*qjG+kR*qkG)*rcpWG);
   pixG=ASatH2((bG*qbeG+eG*qbeG+cG*qchG+hG*qchG+iG*qinG+nG*qinG+lG*qloG+oG*qloG+fG*qfG+gG*qgG+jG*qjG+kG*qkG)*rcpWG);
   pixB=ASatH2((bB*qbeG+eB*qbeG+cB*qchG+hB*qchG+iB*qinG+nB*qinG+lB*qloG+oB*qloG+fB*qfG+gB*qgG+jB*qjG+kB*qkG)*rcpWG);
  #else
   #ifdef CAS_GO_SLOWER
    AH2 rcpWR=ARcpH2(AH2_(2.0)*qbeR+AH2_(2.0)*qchR+AH2_(2.0)*qinR+AH2_(2.0)*qloR+qfR+qgR+qjR+qkR);
    AH2 rcpWG=ARcpH2(AH2_(2.0)*qbeG+AH2_(2.0)*qchG+AH2_(2.0)*qinG+AH2_(2.0)*qloG+qfG+qgG+qjG+qkG);
    AH2 rcpWB=ARcpH2(AH2_(2.0)*qbeB+AH2_(2.0)*qchB+AH2_(2.0)*qinB+AH2_(2.0)*qloB+qfB+qgB+qjB+qkB);
   #else
    AH2 rcpWR=APrxMedRcpH2(AH2_(2.0)*qbeR+AH2_(2.0)*qchR+AH2_(2.0)*qinR+AH2_(2.0)*qloR+qfR+qgR+qjR+qkR);
    AH2 rcpWG=APrxMedRcpH2(AH2_(2.0)*qbeG+AH2_(2.0)*qchG+AH2_(2.0)*qinG+AH2_(2.0)*qloG+qfG+qgG+qjG+qkG);
    AH2 rcpWB=APrxMedRcpH2(AH2_(2.0)*qbeB+AH2_(2.0)*qchB+AH2_(2.0)*qinB+AH2_(2.0)*qloB+qfB+qgB+qjB+qkB);
   #endif
   pixR=ASatH2((bR*qbeR+eR*qbeR+cR*qchR+hR*qchR+iR*qinR+nR*qinR+lR*qloR+oR*qloR+fR*qfR+gR*qgR+jR*qjR+kR*qkR)*rcpWR);
   pixG=ASatH2((bG*qbeG+eG*qbeG+cG*qchG+hG*qchG+iG*qinG+nG*qinG+lG*qloG+oG*qloG+fG*qfG+gG*qgG+jG*qjG+kG*qkG)*rcpWG);
   pixB=ASatH2((bB*qbeB+eB*qbeB+cB*qchB+hB*qchB+iB*qinB+nB*qinB+lB*qloB+oB*qloB+fB*qfB+gB*qgB+jB*qjB+kB*qkB)*rcpWB);
  #endif
 }
#endif

//GiGaHeRz's SPU2 Driver
//Copyright (c) 2003-2008, David Quintana <gigaherz@gmail.com>
//
//This library is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 2.1 of the License, or (at your option) any later version.
//
//This library is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//Lesser General Public License for more details.
//
//You should have received a copy of the GNU Lesser General Public
//License along with this library; if not, write to the Free Software
//Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
#include "spu2.h"

#include <assert.h>
#include <math.h>
#include <float.h>
#include "lowpass.h"

extern void	spdif_update();

void ADMAOutLogWrite(void *lpData, u32 ulSize);

extern void VoiceStop(int core,int vc);

double pow_2_31 = pow(2.0,31.0);

LPF_data L,R;

extern u32 core;

u32 core, voice;

extern u8 callirq;

double srate_pv=1.0;

extern u32 PsxRates[160];

void InitADSR()                                    // INIT ADSR
{
	for (int i=0; i<(32+128); i++)
		{
			int shift=(i-32)>>2;
			__int64 rate=(i&3)+4;
			if (shift<0)
			{
				rate>>=-shift;
			} else
			{
				rate<<=shift;
			}
			PsxRates[i]=(int)min(rate,0x3fffffff);
		}
}

#define VOL(x) (((s32)x)) //24.8 volume

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

const s32 f[5][2] ={{    0,   0 },
					{   60,   0 },
					{  115, -52 },
					{   98, -55 },
					{  122, -60 }};

s32 __forceinline XA_decode(s32 pred1, s32 pred2, s32 shift, s32& prev1, s32& prev2, s32 data)
{
	s32 pcm =data>>shift;
	pcm+=((pred1*prev1)+(pred2*prev2))>>6;
	if(pcm> 32767) pcm= 32767;
	if(pcm<-32768) pcm=-32768;
	prev2=prev1;
	prev1=pcm;
	return pcm;
}

s32 __forceinline XA_decode_block(s32* buffer, s16* block, s32& prev1, s32& prev2)
{
	s32 data=*block;
	s32 Shift	 =  ((data>> 0)&0xF)+16;
	s32 Predict1 = f[(data>> 4)&0xF][0];
	s32 Predict2 = f[(data>> 4)&0xF][1];

	for(int i=0;i<7;i++)
	{
		s32 SampleData=block[i+1];

		*(buffer++) = XA_decode(Predict1, Predict2, Shift, prev1, prev2, (SampleData<<28)&0xF0000000);
		*(buffer++) = XA_decode(Predict1, Predict2, Shift, prev1, prev2, (SampleData<<24)&0xF0000000);
		*(buffer++) = XA_decode(Predict1, Predict2, Shift, prev1, prev2, (SampleData<<20)&0xF0000000);
		*(buffer++) = XA_decode(Predict1, Predict2, Shift, prev1, prev2, (SampleData<<16)&0xF0000000);
	}

	return data;
}

void __forceinline IncrementNextA()
{
	V_Voice& vc(Cores[core].Voices[voice]);

	if((vc.NextA==Cores[core].IRQA)&&(Cores[core].IRQEnable)) { 
		ConLog(" * SPU2: IRQ Called (IRQ passed).\n"); 
		Spdif.Info=4<<core;
		SetIrqCall();
	}

	vc.NextA++;
	vc.NextA&=0xFFFFF;
}

void __fastcall GetNextDataBuffered(s32& Data) 
{
	static s32 pcm=0;
	static s32 data=0;

	V_Voice& vc(Cores[core].Voices[voice]);

	if (vc.SCurrent>=28) 
	{
		if(vc.LoopEnd)
		{
			if(vc.Loop)
			{
				vc.NextA=vc.LoopStartA;
			}
			else
			{
				if(MsgVoiceOff) ConLog(" * SPU2: Voice Off by EndPoint: %d \n", voice);
				VoiceStop(core,voice);
				Cores[core].Regs.ENDX|=1<<voice;
				vc.lastStopReason = 1;
			}
		}

		data = XA_decode_block(vc.SBuffer,GetMemPtr(vc.NextA&0xFFFFF), vc.Prev1, vc.Prev2);
		vc.LoopEnd  =   (data>> 8)&1;
		vc.Loop     =   (data>> 9)&1;
		vc.LoopStart=   (data>>10)&1;
		vc.FirstBlock=0;
		vc.SCurrent = 0;

		if (vc.LoopStart&&!vc.LoopMode)
		{
			vc.LoopStartA=vc.NextA; 
		}

		IncrementNextA();
	}

	Data=vc.SBuffer[vc.SCurrent];

	if((vc.SCurrent&3)==3)
	{
		IncrementNextA();
	}
	vc.SCurrent++;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

const int InvExpOffsets[] = { 0,4,6,8,9,10,11,12 };

void __forceinline CalculateADSR() 
{
	V_ADSR& env(Cores[core].Voices[voice].ADSR);
	u32 SLevel=((u32)env.Sl)<<27;
	u32 off=InvExpOffsets[(env.Value>>28)&7];

	if(env.Releasing)
	{
		if((env.Phase>0)&&(env.Phase<5))
		{
			env.Phase=5;
		}
	}

	switch (env.Phase)
	{
		case 1: // attack
			if (env.Am) // pseudo exponential
			{
				if (env.Value<0x60000000) // below 75%
				{
					env.Value+=PsxRates[(env.Ar^0x7f)-0x10+32];
				} 
				else // above 75%
				{
					env.Value+=PsxRates[(env.Ar^0x7f)-0x18+32];
				}
			}
			else // linear
			{
				env.Value+=PsxRates[(env.Ar^0x7f)-0x10+32];
			}

			if (env.Value>=0x7fffffff)
			{
				env.Phase++;
				env.Value=0x7fffffff;
			}

			break;

		case 2: // decay
			env.Value-=PsxRates[((env.Dr^0x1f)<<2)-0x18+off+32];

			if ((env.Value<=SLevel) || (env.Value>0x7fffffff))
			{
				if (env.Value>0x7fffffff)	env.Value = 0;
				else						env.Value = SLevel;
				env.Phase++;
			}

			break;

		case 3: // sustain
			if (env.Sm&2) // decreasing
			{
				if (env.Sm&4) // exponential
				{
					env.Value-=PsxRates[(env.Sr^0x7f)-0x1b+off+32];
				} 
				else // linear
				{
					env.Value-=PsxRates[(env.Sr^0x7f)-0xf+32];
				}
			} 
			else // increasing
			{
				if (env.Sm&4) // pseudo exponential
				{
					if (env.Value<0x60000000) // below 75%
					{
						env.Value+=PsxRates[(env.Sr^0x7f)-0x10+32];
					} 
					else // above 75%
					{
						env.Value+=PsxRates[(env.Sr^0x7f)-0x18+32];
					}
				}
				else
				{
					// linear

					env.Value+=PsxRates[(env.Sr^0x7f)-0x10+32];
				}
			}

			if ((env.Value>=0x7fffffff) || (env.Value==0))
			{
				env.Value=(env.Sm&2)?0:0x7fffffff;
				env.Phase++;
			}

			break;

		case 4: // sustain end
			env.Value=(env.Sm&2)?0:0x7fffffff;
			if(env.Value==0)
				env.Phase=6;
			break;

		case 5: // release
			{
				if (env.Rm) // exponential
				{
					env.Value-=PsxRates[((env.Rr^0x1f)<<2)-0x18+off+32];
				} 
				else // linear
				{
					env.Value-=PsxRates[((env.Rr^0x1f)<<2)-0xc+32];
				}
			}

			if ((env.Value>0x7fffffff) || (env.Value==0))
			{
				env.Phase++;
				env.Value=0;
			}

			break;

		case 6: // release end
			env.Value=0;
			break;
	}

	if (env.Phase==6) {
		if(MsgVoiceOff) ConLog(" * SPU2: Voice Off by ADSR: %d \n", voice);
		VoiceStop(core,voice);
		Cores[core].Regs.ENDX|=(1<<voice);
		env.Phase=0;
		Cores[core].Voices[voice].lastStopReason = 2;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //
s32 Seed = 0x41595321;

void __forceinline GetNoiseValues(s32& VD) 
{
	if(Seed&0x100) VD =(s32)((Seed&0xff)<<8);
	else if(!(Seed&0xffff)) VD = (s32)0x8000;
	else VD = (s32)0x7fff;

	__asm {
		MOV eax,Seed
		ROR eax,5
		XOR eax,0x9a
		MOV ebx,eax
		ROL eax,2
		ADD eax,ebx
		XOR eax,ebx
		ROR eax,3
		MOV Seed,eax
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

void LowPassFilterInit()
{
	LPF_init(&L,11000,48000);
	LPF_init(&R,11000,48000);
}

void LowPass(s32& VL, s32& VR)
{
	VL = (s32)(LPF(&L,(VL)/pow_2_31)*pow_2_31);
	VR = (s32)(LPF(&R,(VR)/pow_2_31)*pow_2_31);
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

void GetVoiceValues(s32& Value) {
	s64 Data=0;
	s32 DT=0;
	s32 pitch;

	V_Voice& vc(Cores[core].Voices[voice]);

	if((voice==0)||(vc.Modulated==0))
		pitch=vc.Pitch;
	else
		pitch=(vc.Pitch*(32768 + Cores[core].Voices[voice-1].OutX))>>15;
	
	vc.SP+=pitch;
	while(vc.SP>=4096) 
	{
		//this isn't needed.  it's garaunteed to be assigned a valid value.
		//DT=0;

		if(vc.Noise) 
			GetNoiseValues(DT);
		else
			GetNextDataBuffered(DT);

		vc.PV4=vc.PV3;
		vc.PV3=vc.PV2;
		vc.PV2=vc.PV1;
		vc.PV1=DT<<16; //32bit processing

		vc.SP-=4096;
	}

	CalculateADSR();

	if(vc.ADSR.Phase==0)
	{
		Value=0;
		vc.OutX=0;
	}
	else
	{
		// if SP is zero then we landed perfectly on a sample source, no
		// interpolation necessary (this is important too, since the
		// interpolator will pick the wrong sample to mix).

		if(Interpolation==0 || vc.SP == 0) {
			Data = vc.PV1;
		} 
		else if(Interpolation==1) //linear
		{
			// [Air]: Inverted the interpolation delta.  The old way was generating
			// inverted waveforms.
			s64 t0 = vc.PV2 - vc.PV1;
			s64 t1 = vc.PV1;
			Data = (((t0*vc.SP)>>12) + t1);
		}
		else if(Interpolation==2) //cubic
		{
			s64 a0 = vc.PV1 - vc.PV2 - vc.PV4 + vc.PV3;
			s64 a1 = vc.PV4 - vc.PV3 - a0;
			s64 a2 = vc.PV1 - vc.PV4;
			s64 a3 = vc.PV2;
			s64 mu = 4096-vc.SP;

			s64 t0 = ((a0   )*mu)>>18; //all 3 were >>12, was causing ugly, loud overflow sound
			s64 t1 = ((t0+a1)*mu)>>18;
			s64 t2 = ((t1+a2)*mu)>>18;
			s64 t3 = ((t2+a3));

			Data = t3;
		}

		Value=(s32)((Data*vc.ADSR.Value)>>48); //32bit ADSR + convert to 16bit

		vc.OutX=abs(Value);
	}
	//if(vc.PeakX<vc.OutX) vc.PeakX=vc.OutX;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

void __fastcall ReadInput(s32& PDataL,s32& PDataR) 
{
	if((Cores[core].AutoDMACtrl&(core+1))==(core+1))
	{
		s32 tl,tr;

		if((core==1)&&((PlayMode&8)==8))
		{
			Cores[core].InputPos&=~1;

			//CDDA mode
#ifdef PCM24_S1_INTERLEAVE
			*PDataL=*(((s32*)(Cores[core].ADMATempBuffer+(Cores[core].InputPos<<1))));
			*PDataR=*(((s32*)(Cores[core].ADMATempBuffer+(Cores[core].InputPos<<1)+2)));
#else
			s32 *pl=(s32*)&(Cores[core].ADMATempBuffer[Cores[core].InputPos]);
			s32 *pr=(s32*)&(Cores[core].ADMATempBuffer[Cores[core].InputPos+0x200]);
			PDataL=*pl;
			PDataR=*pr;
#endif

			PDataL>>=4; //give 16.8 data
			PDataR>>=4;

			Cores[core].InputPos+=2;
			if((Cores[core].InputPos==0x100)||(Cores[core].InputPos>=0x200)) {
				Cores[core].AdmaInProgress=0;
				if(Cores[core].InputDataLeft>=0x200)
				{
					u8 k=Cores[core].InputDataLeft>=Cores[core].InputDataProgress;

#ifdef PCM24_S1_INTERLEAVE
					AutoDMAReadBuffer(core,1);
#else
					AutoDMAReadBuffer(core,0);
#endif
					Cores[core].AdmaInProgress=1;

					Cores[core].TSA=(core<<10)+Cores[core].InputPos;

					if (Cores[core].InputDataLeft<0x200) 
					{
						FileLog("[%10d] AutoDMA%c block end.\n",Cycles, (core==0)?'4':'7');

						if(Cores[core].InputDataLeft>0)
						{
							if(MsgAutoDMA) ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
						}
						Cores[core].InputDataLeft=0;
						Cores[core].DMAICounter=1;
					}
				}
				Cores[core].InputPos&=0x1ff;
			}

		}
		else if((core==0)&&((PlayMode&4)==4))
		{
			Cores[core].InputPos&=~1;

			s32 *pl=(s32*)&(Cores[core].ADMATempBuffer[Cores[core].InputPos]);
			s32 *pr=(s32*)&(Cores[core].ADMATempBuffer[Cores[core].InputPos+0x200]);
			PDataL=*pl;
			PDataR=*pr;

			Cores[core].InputPos+=2;
			if(Cores[core].InputPos>=0x200) {
				Cores[core].AdmaInProgress=0;
				if(Cores[core].InputDataLeft>=0x200)
				{
					u8 k=Cores[core].InputDataLeft>=Cores[core].InputDataProgress;

					AutoDMAReadBuffer(core,0);

					Cores[core].AdmaInProgress=1;

					Cores[core].TSA=(core<<10)+Cores[core].InputPos;

					if (Cores[core].InputDataLeft<0x200) 
					{
						FileLog("[%10d] Spdif AutoDMA%c block end.\n",Cycles, (core==0)?'4':'7');

						if(Cores[core].InputDataLeft>0)
						{
							if(MsgAutoDMA) ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
						}
						Cores[core].InputDataLeft=0;
						Cores[core].DMAICounter=1;
					}
				}
				Cores[core].InputPos&=0x1ff;
			}

		}
		else
		{
			if((core==1)&&((PlayMode&2)!=0))
			{
				tl=0;
				tr=0;
			}
			else
			{
				// Using the temporary buffer because this area gets overwritten by some other code.
				//*PDataL=(s32)*(s16*)(spu2mem+0x2000+(core<<10)+Cores[core].InputPos);
				//*PDataR=(s32)*(s16*)(spu2mem+0x2200+(core<<10)+Cores[core].InputPos);

				tl=(s32)Cores[core].ADMATempBuffer[Cores[core].InputPos];
				tr=(s32)Cores[core].ADMATempBuffer[Cores[core].InputPos+0x200];

			}

			PDataL=tl;
			PDataR=tr;

			Cores[core].InputPos++;
			if((Cores[core].InputPos==0x100)||(Cores[core].InputPos>=0x200)) {
				Cores[core].AdmaInProgress=0;
				if(Cores[core].InputDataLeft>=0x200)
				{
					u8 k=Cores[core].InputDataLeft>=Cores[core].InputDataProgress;

					AutoDMAReadBuffer(core,0);

					Cores[core].AdmaInProgress=1;

					Cores[core].TSA=(core<<10)+Cores[core].InputPos;

					if (Cores[core].InputDataLeft<0x200) 
					{
						FileLog("[%10d] AutoDMA%c block end.\n",Cycles, (core==0)?'4':'7');

						Cores[core].AutoDMACtrl |=~3;

						if(Cores[core].InputDataLeft>0)
						{
							if(MsgAutoDMA) ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
						}
						Cores[core].InputDataLeft=0;
						Cores[core].DMAICounter=1;
					}
				}
				Cores[core].InputPos&=0x1ff;
			}
		}
	}
	else {
		PDataL=0;
		PDataR=0;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

void ReadInputPV(s32& ValL,s32& ValR) 
{
	s32 DL=0, DR=0;

	u32 pitch=AutoDMAPlayRate[core];

	if(pitch==0) pitch=48000;
	
	Cores[core].ADMAPV+=pitch;
	while(Cores[core].ADMAPV>=48000) 
	{
		ReadInput(DL,DR);
		Cores[core].ADMAPV-=48000;
		Cores[core].ADMAPL=DL;
		Cores[core].ADMAPR=DR;
	}

	ValL=Cores[core].ADMAPL;
	ValR=Cores[core].ADMAPR;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

void __forceinline UpdateVolume(V_Volume& Vol) {
	s32 NVal;

	// TIMINGS ARE FAKE!!! Need to investigate.

	int reverse_phase = Vol.Mode&1;
	int exponential   = Vol.Mode&4;
	int decrement     = Vol.Mode&2;
	int slide_enable  = Vol.Mode&8;
	
	if (!slide_enable) return;

	NVal=Vol.Value;
	if(reverse_phase) NVal = -NVal;

	if (decrement) { // Decrement

		if(exponential)
		{
			NVal=NVal * Vol.Increment >> 7;
		}
		else
		{
			NVal-=Vol.Increment;
		}
		NVal-=((32768*5)>>(Vol.Increment));
		if (NVal<0) {
			Vol.Value=0;
			Vol.Mode=0;
		}
		else Vol.Value=NVal & 0xffff;
	}
	else { // Increment
		if(exponential)
		{
			int T = Vol.Increment>>(NVal>>12);
			NVal+=T;
		}
		else
		{
			NVal+=Vol.Increment;
		}
	}

	if((NVal<0)||(NVal>0x7fff))
	{
		NVal=decrement?0:0x7fff;
		Vol.Mode=0; // disable slide
	}

	if(reverse_phase) NVal = -NVal;

	Vol.Value=NVal;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

s32 __forceinline clamp(s32 x)
{
	if (x>0x00ffffff)  return 0x00ffffff;
	if (x<0xff000000)  return 0xff000000;
	return x;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

void DoReverb(s32& OutL, s32& OutR, s32 InL, s32 InR)
{
	static s32 INPUT_SAMPLE_L,INPUT_SAMPLE_R;
	static s32 OUTPUT_SAMPLE_L,OUTPUT_SAMPLE_R;

	if(!(Cores[core].FxEnable&&EffectsEnabled))
	{
		OUTPUT_SAMPLE_L=0;
		OUTPUT_SAMPLE_R=0;
	}
	else if((Cycles&1)==0) 
	{
		INPUT_SAMPLE_L=InL;
		INPUT_SAMPLE_R=InR;
	}
	else  
	{
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
		s32 IIR_INPUT_A0,IIR_INPUT_A1,IIR_INPUT_B0,IIR_INPUT_B1;
		s32 ACC0,ACC1;
		s32 FB_A0,FB_A1,FB_B0,FB_B1;
		s32 buffsize=Cores[core].EffectsEndA-Cores[core].EffectsStartA+1;

		if(buffsize<0)
		{
			buffsize = Cores[core].EffectsEndA;
			Cores[core].EffectsEndA=Cores[core].EffectsStartA;
			Cores[core].EffectsStartA=buffsize;
			buffsize=Cores[core].EffectsEndA-Cores[core].EffectsStartA+1;
		}

		//filter the 2 samples (prev then current)
		LowPass(INPUT_SAMPLE_L, INPUT_SAMPLE_R);
		LowPass(InL, InR);

		INPUT_SAMPLE_L=(INPUT_SAMPLE_L+InL)>>9;
		INPUT_SAMPLE_R=(INPUT_SAMPLE_R+InR)>>9;

#define BUFFER(x)	((s32)(*GetMemPtr(Cores[core].EffectsStartA + ((Cores[core].ReverbX + buffsize-((x)<<2))%buffsize))))
#define SBUFFER(x)	(*GetMemPtr(Cores[core].EffectsStartA + ((Cores[core].ReverbX + buffsize-((x)<<2))%buffsize)))
		
		Cores[core].ReverbX=((Cores[core].ReverbX + 4)%buffsize);

		IIR_INPUT_A0 = (BUFFER(Cores[core].Revb.IIR_SRC_A0) * Cores[core].Revb.IIR_COEF + INPUT_SAMPLE_L * Cores[core].Revb.IN_COEF_L)>>16;
		IIR_INPUT_A1 = (BUFFER(Cores[core].Revb.IIR_SRC_A1) * Cores[core].Revb.IIR_COEF + INPUT_SAMPLE_R * Cores[core].Revb.IN_COEF_R)>>16;
		IIR_INPUT_B0 = (BUFFER(Cores[core].Revb.IIR_SRC_B0) * Cores[core].Revb.IIR_COEF + INPUT_SAMPLE_L * Cores[core].Revb.IN_COEF_L)>>16;
		IIR_INPUT_B1 = (BUFFER(Cores[core].Revb.IIR_SRC_B1) * Cores[core].Revb.IIR_COEF + INPUT_SAMPLE_R * Cores[core].Revb.IN_COEF_R)>>16;

		SBUFFER(Cores[core].Revb.IIR_DEST_A0 + 4) = clamp((IIR_INPUT_A0 * Cores[core].Revb.IIR_ALPHA + BUFFER(Cores[core].Revb.IIR_DEST_A0) * (65535 - Cores[core].Revb.IIR_ALPHA))>>16);
		SBUFFER(Cores[core].Revb.IIR_DEST_A1 + 4) = clamp((IIR_INPUT_A1 * Cores[core].Revb.IIR_ALPHA + BUFFER(Cores[core].Revb.IIR_DEST_A1) * (65535 - Cores[core].Revb.IIR_ALPHA))>>16);
		SBUFFER(Cores[core].Revb.IIR_DEST_B0 + 4) = clamp((IIR_INPUT_B0 * Cores[core].Revb.IIR_ALPHA + BUFFER(Cores[core].Revb.IIR_DEST_B0) * (65535 - Cores[core].Revb.IIR_ALPHA))>>16);
		SBUFFER(Cores[core].Revb.IIR_DEST_B1 + 4) = clamp((IIR_INPUT_B1 * Cores[core].Revb.IIR_ALPHA + BUFFER(Cores[core].Revb.IIR_DEST_B1) * (65535 - Cores[core].Revb.IIR_ALPHA))>>16);

		ACC0 = (s32)(BUFFER(Cores[core].Revb.ACC_SRC_A0) * Cores[core].Revb.ACC_COEF_A +
				BUFFER(Cores[core].Revb.ACC_SRC_B0) * Cores[core].Revb.ACC_COEF_B +
				BUFFER(Cores[core].Revb.ACC_SRC_C0) * Cores[core].Revb.ACC_COEF_C +
				BUFFER(Cores[core].Revb.ACC_SRC_D0) * Cores[core].Revb.ACC_COEF_D)>>16;
		ACC1 = (s32)(BUFFER(Cores[core].Revb.ACC_SRC_A1) * Cores[core].Revb.ACC_COEF_A +
				BUFFER(Cores[core].Revb.ACC_SRC_B1) * Cores[core].Revb.ACC_COEF_B +
				BUFFER(Cores[core].Revb.ACC_SRC_C1) * Cores[core].Revb.ACC_COEF_C +
				BUFFER(Cores[core].Revb.ACC_SRC_D1) * Cores[core].Revb.ACC_COEF_D)>>16;

		FB_A0 = BUFFER(Cores[core].Revb.MIX_DEST_A0 - Cores[core].Revb.FB_SRC_A);
		FB_A1 = BUFFER(Cores[core].Revb.MIX_DEST_A1 - Cores[core].Revb.FB_SRC_A);
		FB_B0 = BUFFER(Cores[core].Revb.MIX_DEST_B0 - Cores[core].Revb.FB_SRC_B);
		FB_B1 = BUFFER(Cores[core].Revb.MIX_DEST_B1 - Cores[core].Revb.FB_SRC_B);

		SBUFFER(Cores[core].Revb.MIX_DEST_A0) = clamp((ACC0 - FB_A0 * Cores[core].Revb.FB_ALPHA)>>16);
		SBUFFER(Cores[core].Revb.MIX_DEST_A1) = clamp((ACC1 - FB_A1 * Cores[core].Revb.FB_ALPHA)>>16);
		SBUFFER(Cores[core].Revb.MIX_DEST_B0) = clamp(((Cores[core].Revb.FB_ALPHA * ACC0) - FB_A0 * (65535 - Cores[core].Revb.FB_ALPHA) - FB_B0 * Cores[core].Revb.FB_X)>>16);
		SBUFFER(Cores[core].Revb.MIX_DEST_B1) = clamp(((Cores[core].Revb.FB_ALPHA * ACC1) - FB_A1 * (65535 - Cores[core].Revb.FB_ALPHA) - FB_B1 * Cores[core].Revb.FB_X)>>16);

		OUTPUT_SAMPLE_L=clamp((BUFFER(Cores[core].Revb.MIX_DEST_A0)+BUFFER(Cores[core].Revb.MIX_DEST_B0))>>2);
		OUTPUT_SAMPLE_R=clamp((BUFFER(Cores[core].Revb.MIX_DEST_B1)+BUFFER(Cores[core].Revb.MIX_DEST_B1))>>2);
	} 
	OutL=OUTPUT_SAMPLE_L;
	OutR=OUTPUT_SAMPLE_R;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //
bool inited = false;
s32 count=0;
s32 maxpeak=0;
double rfactor=1;
double cfactor=1;
double diff=0;

s32 __forceinline ApplyVolume(s32 data, s32 volume)
{
	return (volume * data);
}

void __forceinline MixVoice(s32& VValL, s32& VValR)
{
	V_Voice& vc(Cores[core].Voices[voice]);

	s32 Value=0;

	VValL=0;
	VValR=0;

	UpdateVolume(vc.VolumeL);
	UpdateVolume(vc.VolumeR);

	if (Cores[core].Voices[voice].ADSR.Phase>0) 
	{
		GetVoiceValues(Value);

		vc.displayPeak = max(vc.displayPeak,abs(Value));

		VValL=ApplyVolume(Value,(vc.VolumeL.Value));
		VValR=ApplyVolume(Value,(vc.VolumeR.Value));
	}

	if (voice==1)      spu2Ms16(0x400 + (core<<12) + OutPos)=(s16)((Value));
	else if (voice==3) spu2Ms16(0x600 + (core<<12) + OutPos)=(s16)((Value));

}

__forceinline void MixCore(s32& OutL, s32& OutR, s32 ExtL, s32 ExtR)
{
	s32 InpL=0, InpR=0;

	s32 RVL,RVR;
	s32 TDL=0,TDR=0,TWL=0,TWR=0;
	s32 SDL=0,SDR=0,SWL=0,SWR=0;

	TDL=TDR=TWL=TWR=(s32)0;

	if (core == 1) { //Core 0 doesn't have External input
		spu2Ms16(0x800 + OutPos)=(s16)(ExtL>>16);
		spu2Ms16(0xA00 + OutPos)=(s16)(ExtR>>16);
	}
	
	if((core==0)&&((PlayMode&4)!=4))
	{
		ReadInputPV(InpL,InpR);	// get input data from input buffers
	}
	if((core==1)&&((PlayMode&8)!=8))
	{
		ReadInputPV(InpL,InpR);	// get input data from input buffers
	}

	s32 InputPeak = max(abs(InpL),abs(InpR));
	if(Cores[core].AutoDMAPeak<InputPeak) Cores[core].AutoDMAPeak=InputPeak;
	
	InpL = MulDiv(InpL,(Cores[core].InpL),1<<1);
	InpR = MulDiv(InpR,(Cores[core].InpR),1<<1);

	ExtL = MulDiv(ExtL,(Cores[core].ExtL),1<<12);
	ExtR = MulDiv(ExtR,(Cores[core].ExtR),1<<12);

	SDL=SDR=SWL=SWR=(s32)0;

	for (voice=0;voice<24;voice++) 
	{
		s32 VValL,VValR;

		MixVoice(VValL,VValR);

		SDL += VValL * Cores[core].Voices[voice].DryL;
		SDR += VValR * Cores[core].Voices[voice].DryR;
		SWL += VValL * Cores[core].Voices[voice].WetL;
		SWR += VValR * Cores[core].Voices[voice].WetR;
	}

	//Write To Output Area
	spu2Ms16(0x1000 + (core<<12) + OutPos)=(s16)(SDL>>16);
	spu2Ms16(0x1200 + (core<<12) + OutPos)=(s16)(SDR>>16);
	spu2Ms16(0x1400 + (core<<12) + OutPos)=(s16)(SWL>>16);
	spu2Ms16(0x1600 + (core<<12) + OutPos)=(s16)(SWR>>16);

	// Mix in the Voice data
	TDL += SDL * Cores[core].SndDryL;
	TDR += SDR * Cores[core].SndDryR;
	TWL += SWL * Cores[core].SndWetL;
	TWR += SWR * Cores[core].SndWetR;

	// Mix in the Input data
	TDL += InpL * Cores[core].InpDryL;
	TDR += InpR * Cores[core].InpDryR;
	TWL += InpL * Cores[core].InpWetL;
	TWR += InpR * Cores[core].InpWetR;

	// Mix in the External (nothing/core0) data
	TDL += ExtL * Cores[core].ExtDryL;
	TDR += ExtR * Cores[core].ExtDryR;
	TWL += ExtL * Cores[core].ExtWetL; 
	TWR += ExtR * Cores[core].ExtWetR;
	
	if(EffectsEnabled)
	{
		//Apply Effects
		DoReverb(RVL,RVR,TWL>>16,TWR>>16);

		TWL=ApplyVolume(RVL,VOL(Cores[core].FxL));
		TWR=ApplyVolume(RVR,VOL(Cores[core].FxR));
	}
	else
	{
		TWL=0;
		TWR=0;
	}

	//Mix Wet,Dry
	OutL=(TDL + TWL);
	OutR=(TDR + TWR);

	//Apply Master Volume
	UpdateVolume(Cores[core].MasterL);
	UpdateVolume(Cores[core].MasterR);

	if (Cores[core].Mute==0) {
		OutL=MulDiv(OutL,Cores[core].MasterL.Value,1<<16);
		OutR=MulDiv(OutR,Cores[core].MasterR.Value,1<<16);
	}
	else 
	{
		OutL=0;
		OutR=0;
	}

	if((core==1)&&(PlayMode&8))
	{
		ReadInput(OutL,OutR);
	}

	if((core==0)&&(PlayMode&4))
	{
		OutL=0;
		OutR=0;
	}
}

void __fastcall Mix() 
{
	s32 ExtL=0, ExtR=0, OutL, OutR;

	static s32 Peak0,Peak1;
	static s32 PCount;

	core=0;
	MixCore(ExtL,ExtR,0,0);

	Peak0 = max(Peak0,max(ExtL,ExtR));

	core=1;
	MixCore(OutL,OutR,ExtL,ExtR);

	Peak1 = max(Peak1,max(OutL,OutR));

	ExtL=MulDiv(OutL,VolumeMultiplier,VolumeDivisor<<6);
	ExtR=MulDiv(OutR,VolumeMultiplier,VolumeDivisor<<6);

	// Update spdif (called each sample)
	if(PlayMode&4)
	{
		spdif_update();
	}

	// AddToBuffer
	SndWrite(ExtL,ExtR);
	OutPos++;
	if (OutPos>=0x200) OutPos=0;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //
u32 PsxRates[160]={
	
	//for +Lin: PsxRates[value+8]
	//for -Lin: PsxRates[value+7]

	0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
	0xD744FCCB,0xB504F334,0x9837F052,0x80000000,0x6BA27E65,0x5A82799A,0x4C1BF829,0x40000000,
	0x35D13F33,0x2D413CCD,0x260DFC14,0x20000000,0x1AE89F99,0x16A09E66,0x1306FE0A,0x10000000,
	0x0D744FCD,0x0B504F33,0x09837F05,0x08000000,0x06BA27E6,0x05A8279A,0x04C1BF83,0x04000000,
	0x035D13F3,0x02D413CD,0x0260DFC1,0x02000000,0x01AE89FA,0x016A09E6,0x01306FE1,0x01000000,
	0x00D744FD,0x00B504F3,0x009837F0,0x00800000,0x006BA27E,0x005A827A,0x004C1BF8,0x00400000,
	0x0035D13F,0x002D413D,0x00260DFC,0x00200000,0x001AE8A0,0x0016A09E,0x001306FE,0x00100000,
	0x000D7450,0x000B504F,0x0009837F,0x00080000,0x0006BA28,0x0005A828,0x0004C1C0,0x00040000,
	0x00035D14,0x0002D414,0x000260E0,0x00020000,0x0001AE8A,0x00016A0A,0x00013070,0x00010000,
	0x0000D745,0x0000B505,0x00009838,0x00008000,0x00006BA2,0x00005A82,0x00004C1C,0x00004000,
	0x000035D1,0x00002D41,0x0000260E,0x00002000,0x00001AE9,0x000016A1,0x00001307,0x00001000,
	0x00000D74,0x00000B50,0x00000983,0x00000800,0x000006BA,0x000005A8,0x000004C2,0x00000400,
	0x0000035D,0x000002D4,0x00000261,0x00000200,0x000001AF,0x0000016A,0x00000130,0x00000100,
	0x000000D7,0x000000B5,0x00000098,0x00000080,0x0000006C,0x0000005B,0x0000004C,0x00000040,
	0x00000036,0x0000002D,0x00000026,0x00000020,0x0000001B,0x00000017,0x00000013,0x00000010,
	0x0000000D,0x0000000B,0x0000000A,0x00000008,0x00000007,0x00000006,0x00000005,0x00000004,
	0x00000003,0x00000003,0x00000002,0x00000002,0x00000002,0x00000001,0x00000001,0x00000000,

	//128+8
	0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
};

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

/*
-----------------------------------------------------------------------------
PSX reverb hardware notes
by Neill Corlett
-----------------------------------------------------------------------------

Yadda yadda disclaimer yadda probably not perfect yadda well it's okay anyway
yadda yadda.

-----------------------------------------------------------------------------

Basics
------

- The reverb buffer is 22khz 16-bit mono PCM.
- It starts at the reverb address given by 1DA2, extends to
  the end of sound RAM, and wraps back to the 1DA2 address.

Setting the address at 1DA2 resets the current reverb work address.

This work address ALWAYS increments every 1/22050 sec., regardless of
whether reverb is enabled (bit 7 of 1DAA set).

And the contents of the reverb buffer ALWAYS play, scaled by the
"reverberation depth left/right" volumes (1D84/1D86).
(which, by the way, appear to be scaled so 3FFF=approx. 1.0, 4000=-1.0)

-----------------------------------------------------------------------------

Register names
--------------

These are probably not their real names.
These are probably not even correct names.
We will use them anyway, because we can.

1DC0: FB_SRC_A       (offset)
1DC2: FB_SRC_B       (offset)
1DC4: IIR_ALPHA      (coef.)
1DC6: ACC_COEF_A     (coef.)
1DC8: ACC_COEF_B     (coef.)
1DCA: ACC_COEF_C     (coef.)
1DCC: ACC_COEF_D     (coef.)
1DCE: IIR_COEF       (coef.)
1DD0: FB_ALPHA       (coef.)
1DD2: FB_X           (coef.)
1DD4: IIR_DEST_A0    (offset)
1DD6: IIR_DEST_A1    (offset)
1DD8: ACC_SRC_A0     (offset)
1DDA: ACC_SRC_A1     (offset)
1DDC: ACC_SRC_B0     (offset)
1DDE: ACC_SRC_B1     (offset)
1DE0: IIR_SRC_A0     (offset)
1DE2: IIR_SRC_A1     (offset)
1DE4: IIR_DEST_B0    (offset)
1DE6: IIR_DEST_B1    (offset)
1DE8: ACC_SRC_C0     (offset)
1DEA: ACC_SRC_C1     (offset)
1DEC: ACC_SRC_D0     (offset)
1DEE: ACC_SRC_D1     (offset)
1DF0: IIR_SRC_B1     (offset)
1DF2: IIR_SRC_B0     (offset)
1DF4: MIX_DEST_A0    (offset)
1DF6: MIX_DEST_A1    (offset)
1DF8: MIX_DEST_B0    (offset)
1DFA: MIX_DEST_B1    (offset)
1DFC: IN_COEF_L      (coef.)
1DFE: IN_COEF_R      (coef.)

The coefficients are signed fractional values.
-32768 would be -1.0
 32768 would be  1.0 (if it were possible... the highest is of course 32767)

The offsets are (byte/8) offsets into the reverb buffer.
i.e. you multiply them by 8, you get byte offsets.
You can also think of them as (samples/4) offsets.
They appear to be signed.  They can be negative.
None of the documented presets make them negative, though.

Yes, 1DF0 and 1DF2 appear to be backwards.  Not a typo.

-----------------------------------------------------------------------------

What it does
------------

We take all reverb sources:
- regular channels that have the reverb bit on
- cd and external sources, if their reverb bits are on
and mix them into one stereo 44100hz signal.

Lowpass/downsample that to 22050hz.  The PSX uses a proper bandlimiting
algorithm here, but I haven't figured out the hysterically exact specifics.
I use an 8-tap filter with these coefficients, which are nice but probably
not the real ones:

0.037828187894
0.157538631280
0.321159685278
0.449322115345
0.449322115345
0.321159685278
0.157538631280
0.037828187894

So we have two input samples (INPUT_SAMPLE_L, INPUT_SAMPLE_R) every 22050hz.

* IN MY EMULATION, I divide these by 2 to make it clip less.
  (and of course the L/R output coefficients are adjusted to compensate)
  The real thing appears to not do this.

At every 22050hz tick:
- If the reverb bit is enabled (bit 7 of 1DAA), execute the reverb
  steady-state algorithm described below
- AFTERWARDS, retrieve the "wet out" L and R samples from the reverb buffer
  (This part may not be exactly right and I guessed at the coefs. TODO: check later.)
  L is: 0.333 * (buffer[MIX_DEST_A0] + buffer[MIX_DEST_B0])
  R is: 0.333 * (buffer[MIX_DEST_A1] + buffer[MIX_DEST_B1])
- Advance the current buffer position by 1 sample

The wet out L and R are then upsampled to 44100hz and played at the
"reverberation depth left/right" (1D84/1D86) volume, independent of the main
volume.

-----------------------------------------------------------------------------

Reverb steady-state
-------------------

The reverb steady-state algorithm is fairly clever, and of course by
"clever" I mean "batshit insane".

buffer[x] is relative to the current buffer position, not the beginning of
the buffer.  Note that all buffer offsets must wrap around so they're
contained within the reverb work area.

Clipping is performed at the end... maybe also sooner, but definitely at
the end.

IIR_INPUT_A0 = buffer[IIR_SRC_A0] * IIR_COEF + INPUT_SAMPLE_L * IN_COEF_L;
IIR_INPUT_A1 = buffer[IIR_SRC_A1] * IIR_COEF + INPUT_SAMPLE_R * IN_COEF_R;
IIR_INPUT_B0 = buffer[IIR_SRC_B0] * IIR_COEF + INPUT_SAMPLE_L * IN_COEF_L;
IIR_INPUT_B1 = buffer[IIR_SRC_B1] * IIR_COEF + INPUT_SAMPLE_R * IN_COEF_R;

IIR_A0 = IIR_INPUT_A0 * IIR_ALPHA + buffer[IIR_DEST_A0] * (1.0 - IIR_ALPHA);
IIR_A1 = IIR_INPUT_A1 * IIR_ALPHA + buffer[IIR_DEST_A1] * (1.0 - IIR_ALPHA);
IIR_B0 = IIR_INPUT_B0 * IIR_ALPHA + buffer[IIR_DEST_B0] * (1.0 - IIR_ALPHA);
IIR_B1 = IIR_INPUT_B1 * IIR_ALPHA + buffer[IIR_DEST_B1] * (1.0 - IIR_ALPHA);

buffer[IIR_DEST_A0 + 1sample] = IIR_A0;
buffer[IIR_DEST_A1 + 1sample] = IIR_A1;
buffer[IIR_DEST_B0 + 1sample] = IIR_B0;
buffer[IIR_DEST_B1 + 1sample] = IIR_B1;

ACC0 = buffer[ACC_SRC_A0] * ACC_COEF_A +
       buffer[ACC_SRC_B0] * ACC_COEF_B +
       buffer[ACC_SRC_C0] * ACC_COEF_C +
       buffer[ACC_SRC_D0] * ACC_COEF_D;
ACC1 = buffer[ACC_SRC_A1] * ACC_COEF_A +
       buffer[ACC_SRC_B1] * ACC_COEF_B +
       buffer[ACC_SRC_C1] * ACC_COEF_C +
       buffer[ACC_SRC_D1] * ACC_COEF_D;

FB_A0 = buffer[MIX_DEST_A0 - FB_SRC_A];
FB_A1 = buffer[MIX_DEST_A1 - FB_SRC_A];
FB_B0 = buffer[MIX_DEST_B0 - FB_SRC_B];
FB_B1 = buffer[MIX_DEST_B1 - FB_SRC_B];

buffer[MIX_DEST_A0] = ACC0 - FB_A0 * FB_ALPHA;
buffer[MIX_DEST_A1] = ACC1 - FB_A1 * FB_ALPHA;
buffer[MIX_DEST_B0] = (FB_ALPHA * ACC0) - FB_A0 * (FB_ALPHA^0x8000) - FB_B0 * FB_X;
buffer[MIX_DEST_B1] = (FB_ALPHA * ACC1) - FB_A1 * (FB_ALPHA^0x8000) - FB_B1 * FB_X;

-----------------------------------------------------------------------------
*/

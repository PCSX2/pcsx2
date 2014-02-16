#include <jni.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>  

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "types.h"
#include "profiler/profiler.h"
#include "cfg/cfg.h"
#include "rend/TexCache.h"
#include "hw/maple/maple_devs.h"
#include "hw/maple/maple_if.h"
#include "hw/pvr/zHack.h"

#include "util.h"

extern "C"
{
  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_config(JNIEnv *env,jobject obj,jstring dirName)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_init(JNIEnv *env,jobject obj,jstring fileName)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_run(JNIEnv *env,jobject obj,jobject track)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_stop(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));

  JNIEXPORT jint JNICALL Java_com_reicast_emulator_JNIdc_send(JNIEnv *env,jobject obj,jint id, jint v)  __attribute__((visibility("default")));
  JNIEXPORT jint JNICALL Java_com_reicast_emulator_JNIdc_data(JNIEnv *env,jobject obj,jint id, jbyteArray d)  __attribute__((visibility("default")));

  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_rendinit(JNIEnv *env,jobject obj,jint w,jint h)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_rendframe(JNIEnv *env,jobject obj)  __attribute__((visibility("default")));

  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_zHack(JNIEnv *env, jobject obj, jboolean enable_zhack)  __attribute__((visibility("default")));

  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_kcode(JNIEnv * env, jobject obj, jintArray k_code, jintArray l_t, jintArray r_t, jintArray jx, jintArray jy)  __attribute__((visibility("default")));
  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_vjoy(JNIEnv * env, jobject obj,u32 id,float x, float y, float w, float h)  __attribute__((visibility("default")));
  //JNIEXPORT jint JNICALL Java_com_reicast_emulator_JNIdc_play(JNIEnv *env,jobject obj,jshortArray result,jint size);

  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_initControllers(JNIEnv *env, jobject obj, jbooleanArray controllers)  __attribute__((visibility("default")));
  
  JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)  __attribute__((visibility("default")));
};


void egl_stealcntx();
void SetApplicationPath(wchar *path);
int dc_init(int argc,wchar* argv[]);
void dc_run();
void dc_term();
void mcfg_Create(MapleDeviceType type,u32 bus,u32 port);

bool VramLockedWrite(u8* address);

bool rend_single_frame();
bool gles_init();

//extern cResetEvent rs,re;
extern int screen_width,screen_height;

static u64 tvs_base;
static char CurFileName[256];

// Additonal controllers 2, 3 and 4 connected ?
static bool add_controllers[3] = { false, false, false };

u16 kcode[4];
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];
float vjoy_pos[14][8];

extern bool print_stats;

void os_DoEvents()
{
  // @@@ Nothing here yet
}

//
// Native thread that runs the actual nullDC emulator
//
static void *ThreadHandler(void *UserData)
{
  char *Args[3];
  const char *P;

  // Make up argument list
  P       = (const char *)UserData;
  Args[0] = "dc";
  Args[1] = "-config";
  Args[2] = P&&P[0]? (char *)malloc(strlen(P)+32):0;

  if(Args[2])
  {
    strcpy(Args[2],"config:image=");
    strcat(Args[2],P);
  }

  // Add additonal controllers
  for (int i = 0; i < 3; i++)
  {
    if (add_controllers[i])
      mcfg_Create(MDT_SegaController,i+1,5);
  }

  // Run nullDC emulator
  dc_init(Args[2]? 3:1,Args);
}

//
// Platform-specific NullDC functions
//

int msgboxf(const wchar* Text,unsigned int Type,...)
{
  wchar S[2048];
  va_list Args;

  va_start(Args,Type);
  vsprintf(S,Text,Args);
  va_end(Args);

  puts(S);
  return(MBX_OK);
}

void UpdateInputState(u32 Port)
{
  // @@@ Nothing here yet
}

void *libPvr_GetRenderTarget() 
{
  // No X11 window in Android 
  return(0);
}

void *libPvr_GetRenderSurface() 
{ 
  // No X11 display in Android 
  return(0);
}

void common_linux_setup();

void os_SetWindowText(char const *Text)
{
	putinf(Text);
}
JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_config(JNIEnv *env,jobject obj,jstring dirName)
{
  // Set home directory based on User config
  const char* D = dirName? env->GetStringUTFChars(dirName,0):0;
  SetHomeDir(D);
  printf("Home dir is: '%s'\n",GetPath("/").c_str());
  env->ReleaseStringUTFChars(dirName,D);
}
JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_init(JNIEnv *env,jobject obj,jstring fileName)
{

  // Get filename string from Java
  const char* P = fileName? env->GetStringUTFChars(fileName,0):0;
  if(!P) CurFileName[0] = '\0';
  else
  {
    printf("Got URI: '%s'\n",P);
    strncpy(CurFileName,(strlen(P)>=7)&&!memcmp(P,"file://",7)? P+7:P,sizeof(CurFileName));
    CurFileName[sizeof(CurFileName)-1] = '\0';
    env->ReleaseStringUTFChars(fileName,P);
  }

  printf("Opening file: '%s'\n",CurFileName);

  // Initialize platform-specific stuff
  common_linux_setup();

  // Set configuration
  settings.profile.run_counts = 0;
  

/*
  // Start native thread
  pthread_attr_init(&PTAttr);
  pthread_attr_setdetachstate(&PTAttr,PTHREAD_CREATE_DETACHED);
  pthread_create(&PThread,&PTAttr,ThreadHandler,CurFileName);
  pthread_attr_destroy(&PTAttr);
  */

  ThreadHandler(CurFileName);
}

#define SAMPLE_COUNT 512

JNIEnv* jenv;
jshortArray jsamples;
jmethodID writemid;
jobject track;

jobject sipemu;
jmethodID getmicdata;

JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_run(JNIEnv *env,jobject obj,jobject trk)
{
	install_prof_handler(0);

	jenv=env;
	track=trk;

	jsamples=env->NewShortArray(SAMPLE_COUNT*2);
	writemid=env->GetMethodID(env->GetObjectClass(track),"WriteBuffer","([SI)I");

	dc_run();
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_setupMic(JNIEnv *env,jobject obj,jobject sip)
{
	sipemu = env->NewGlobalRef(sip);
	getmicdata = env->GetMethodID(env->GetObjectClass(sipemu),"getData","()[B");	
	delete MapleDevices[0][1];
	mcfg_Create(MDT_Microphone,0,1);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_stop(JNIEnv *env,jobject obj)
{
	dc_term();
}

JNIEXPORT jint JNICALL Java_com_reicast_emulator_JNIdc_send(JNIEnv *env,jobject obj,jint cmd, jint param)
{
	if (cmd==0)
	{
		if (param==0)
		{
			KillTex=true;
			printf("Killing texture cache\n");
		}

		if (param==1)
		{
	  settings.pvr.ta_skip^=1;
	  printf("settings.pvr.ta_skip: %d\n",settings.pvr.ta_skip);
		}
		if (param==2)
		{
			print_stats=true;
			printf("Storing blocks ...\n");
		}
	}
	else if (cmd==1)
	{
    if (param==0)
      sample_Stop();
    else
      sample_Start(param);
	}
	else if (cmd==2)
	{
	}
}

JNIEXPORT jint JNICALL Java_com_reicast_emulator_JNIdc_data(JNIEnv *env,jobject obj,jint id, jbyteArray d)
{
	if (id==1)
	{
		printf("Loading symtable (%p,%p,%p,%p)\n",env,obj,id,d);
		int len=env->GetArrayLength(d);
		u8* syms=(u8*)malloc(len);
		printf("Loading symtable to %p, %d\n",syms,len);
		env->GetByteArrayRegion(d,0,len,(jbyte*)syms);
		sample_Syms(syms,len);
	}
}


JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_rendframe(JNIEnv *env,jobject obj)
{
	while(!rend_single_frame()) ;
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_kcode(JNIEnv * env, jobject obj, jintArray k_code, jintArray l_t, jintArray r_t, jintArray jx, jintArray jy)
{
	jint *k_code_body = env->GetIntArrayElements(k_code, 0);
	jint *l_t_body = env->GetIntArrayElements(l_t, 0);
	jint *r_t_body = env->GetIntArrayElements(r_t, 0);
	jint *jx_body = env->GetIntArrayElements(jx, 0);
	jint *jy_body = env->GetIntArrayElements(jy, 0);

	for(int i = 0; i < 4; i++)
	{
		kcode[i] = k_code_body[i];	
		lt[i] = l_t_body[i];
		rt[i] = r_t_body[i];
		joyx[i] = jx_body[i];
		joyy[i] = jy_body[i];
	}

	env->ReleaseIntArrayElements(k_code, k_code_body, 0);
	env->ReleaseIntArrayElements(l_t, l_t_body, 0);
	env->ReleaseIntArrayElements(r_t, r_t_body, 0);
	env->ReleaseIntArrayElements(jx, jx_body, 0);
	env->ReleaseIntArrayElements(jy, jy_body, 0);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_rendinit(JNIEnv * env, jobject obj, jint w,jint h)
{             
  screen_width  = w;
  screen_height = h;

  //gles_term();

  egl_stealcntx();

  if (!gles_init())
	die("OPENGL FAILED");

  install_prof_handler(1);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_zHack(JNIEnv *env, jobject obj, jboolean enable_zhack)
{
    configure_zHack(enable_zhack);
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_vjoy(JNIEnv * env, jobject obj,u32 id,float x, float y, float w, float h)
{
  if(id<sizeof(vjoy_pos)/sizeof(vjoy_pos[0]))
  {
    vjoy_pos[id][0] = x;
    vjoy_pos[id][1] = y;
	vjoy_pos[id][2] = w;
	vjoy_pos[id][3] = h;
  }
}

JNIEXPORT void JNICALL Java_com_reicast_emulator_JNIdc_initControllers(JNIEnv *env, jobject obj, jbooleanArray controllers)
{
	jboolean *controllers_body = env->GetBooleanArrayElements(controllers, 0);
	memcpy(add_controllers, controllers_body, 3);
	env->ReleaseBooleanArrayElements(controllers, controllers_body, 0);
}

u32 os_Push(void* frame, u32 amt, bool wait)
{
	verify(amt==SAMPLE_COUNT);
	//yeah, do some audio piping magic here !
	jenv->SetShortArrayRegion(jsamples,0,amt*2,(jshort*)frame);
	return jenv->CallIntMethod(track,writemid,jsamples,wait);
}

bool os_IsAudioBuffered()
{
    return jenv->CallIntMethod(track,writemid,jsamples,-1)==0;
}

int get_mic_data(u8* buffer)
{
	jbyteArray jdata = (jbyteArray)jenv->CallObjectMethod(sipemu,getmicdata);
	if(jdata==NULL){
		//LOGW("get_mic_data NULL");
		return 0;
	}
	jenv->GetByteArrayRegion(jdata, 0, SIZE_OF_MIC_DATA, (jbyte*)buffer);
	jenv->DeleteLocalRef(jdata);
	return 1;
}

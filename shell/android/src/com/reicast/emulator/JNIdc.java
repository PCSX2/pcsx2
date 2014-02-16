package com.reicast.emulator;


public class JNIdc
{
  static { System.loadLibrary("dc"); }
	
  public static native void config(String dirName);
  public static native void init(String fileName);
  public static native void run(Object track);
  public static native void stop();
  
  public static native int send(int cmd, int opt);
  public static native int data(int cmd, byte[] data);
  
  public static native void rendinit(int w, int y);
  public static native void rendframe();

  public static native void zHack(boolean enable_zhack);
  
  public static native void kcode(int[] kcode, int[] lt, int[] rt, int[] jx, int[] jy);
  
  public static native void vjoy(int id,float x, float y, float w, float h);
  //public static native int play(short result[],int size);

  public static native void initControllers(boolean[] controllers);
  
  public static native void setupMic(Object sip);

  public static void show_osd() {
    JNIdc.vjoy(13, 1,0,0,0);
  }

  public static void hide_osd() {
   JNIdc.vjoy(13, 0,0,0,0); 
  }
}

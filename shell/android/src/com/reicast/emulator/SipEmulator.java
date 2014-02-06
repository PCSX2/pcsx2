package com.reicast.emulator;

import java.util.concurrent.ConcurrentLinkedQueue;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.util.Log;

public class SipEmulator extends Thread {
	
	static final String TAG = "SipEmulator";
	
	//one second of audio data in bytes
	static final int BUFFER_SIZE = 22050;
	//this needs to get set to the amount the mic normally sends per data request
	//...cant be bigger than a maple packet
	// 240 16 (or 14) bit samples
	static final int ONE_BLIP_SIZE = 480; //ALSO DEFINED IN maple_devs.h
	
	private AudioRecord record;
	private ConcurrentLinkedQueue<byte[]> bytesReadBuffer;
	
	private boolean continueRecording;
	private boolean firstGet;
	
	/*
	16-bit PCM @ 11025 hz
	== 176.4 kbit/s
	== 22050 bytes/s
	*/
	
	public SipEmulator(){
		
		Log.d(TAG, "SipEmulator constructor called");
		
		init();
		
	}
	
	private void init(){
		Log.d(TAG, "SipEmulator init called");

		record = new AudioRecord(
				MediaRecorder.AudioSource.MIC, 
				11025, 
				AudioFormat.CHANNEL_IN_MONO, 
				AudioFormat.ENCODING_PCM_16BIT, 
				BUFFER_SIZE);
		
		bytesReadBuffer = new ConcurrentLinkedQueue<byte[]>();
		
		continueRecording = false;
		firstGet = true;
	}
	
	public void startRecording(){
		Log.d(TAG, "SipEmulator startRecording called");
		if(continueRecording){
			return;
		}
		record.startRecording();
		continueRecording = true;
		this.start();
	}
	
	public void stopRecording(){
		Log.d(TAG, "SipEmulator stopRecording called");
		continueRecording = false;
		record.stop();
	}
	
	public byte[] getData(){
		//Log.d(TAG, "SipEmulator getData called");
		//Log.d(TAG, "SipEmulator getData bytesReadBuffer size: "+bytesReadBuffer.size());
		if(firstGet || bytesReadBuffer.size()>50){//50 blips is about 2 seconds!
			firstGet = false;
			return catchUp();
		}
		return bytesReadBuffer.poll();
	}
	
	private byte[] catchUp(){
		Log.d(TAG, "SipEmulator catchUp");
		byte[] last = bytesReadBuffer.poll();
		bytesReadBuffer.clear();
		return last;
	}
	
	public void configSomething(int what, int setting){
		Log.d(TAG, "SipEmulator configSomething called");
		
	}
	
	public void run() {
		Log.d(TAG, "recordThread starting");
		
		while(continueRecording){
			byte[] freshData = new byte[ONE_BLIP_SIZE];
			// read blocks
			int bytesRead = record.read(freshData, 0, ONE_BLIP_SIZE); 
			//Log.d(TAG, "recordThread recorded: "+bytesRead);
			if(!firstGet){
				bytesReadBuffer.add(freshData);
			}
		}
	}

}

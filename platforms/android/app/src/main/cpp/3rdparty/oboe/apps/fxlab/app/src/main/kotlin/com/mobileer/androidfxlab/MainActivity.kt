/*
 * Copyright  2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.mobileer.androidfxlab

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiManager
import android.media.midi.MidiReceiver
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.view.SubMenu
import android.view.WindowManager
import android.widget.PopupMenu
import android.widget.SeekBar
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.mobileer.androidfxlab.databinding.ActivityMainBinding
import com.mobileer.androidfxlab.datatype.Effect

class MainActivity : AppCompatActivity() {

    private var TAG: String = this.toString()
    lateinit var binding: ActivityMainBinding
    private var isAudioEnabled: Boolean = false

    val MY_PERMISSIONS_RECORD_AUDIO = 17

    @RequiresApi(Build.VERSION_CODES.M)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setSupportActionBar(binding.toolbar)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            != PackageManager.PERMISSION_GRANTED
        ) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(Manifest.permission.RECORD_AUDIO),
                MY_PERMISSIONS_RECORD_AUDIO
            )
        }

        binding.effectListView.adapter = EffectsAdapter

        binding.floatingAddButton.setOnClickListener { view ->
            val popup = PopupMenu(this, view)
            popup.menuInflater.inflate(R.menu.add_menu, popup.menu)
            val menuMap = HashMap<String, SubMenu>()
            for (effectName in NativeInterface.effectDescriptionMap.keys) {
                val cat = NativeInterface.effectDescriptionMap.getValue(effectName).category
                if (cat == "None") {
                    popup.menu.add(effectName)
                } else {
                    val subMenu = menuMap[cat] ?: popup.menu.addSubMenu(cat)
                    subMenu.add(effectName)
                    menuMap[cat] = subMenu
                }
            }
            popup.setOnMenuItemClickListener { menuItem ->
                NativeInterface.effectDescriptionMap[menuItem.title]?.let {
                    val toAdd = Effect(it)
                    EffectsAdapter.effectList.add(toAdd)
                    NativeInterface.addEffect(toAdd)
                    EffectsAdapter.notifyItemInserted(EffectsAdapter.effectList.size - 1)
                    true
                }
                false
            }
            popup.show()
        }
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            handleMidiDevices()
        }

    }

    override fun onDestroy() {
        // Clear the FX UI
        EffectsAdapter.effectList.clear()
        EffectsAdapter.notifyDataSetChanged()
        super.onDestroy()
    }

    override fun onPause() {
        // Shutdown Engine
        NativeInterface.destroyAudioEngine()
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
        // Startup Engine
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            == PackageManager.PERMISSION_GRANTED
        ) {
            NativeInterface.createAudioEngine()
            if (EffectsAdapter.effectList.isEmpty()) {
                addDefaultEffects()
            }
            NativeInterface.enable(isAudioEnabled)
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        when (requestCode) {
            MY_PERMISSIONS_RECORD_AUDIO -> {
                if ((grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED)) {
                    NativeInterface.createAudioEngine()
                } else {
                    val builder = AlertDialog.Builder(this).apply {
                        setMessage(
                            "Audio effects require audio input permissions! \n" +
                                    "Enable permissions and restart app to use."
                        )
                        setTitle("Permission Error")
                    }
                    builder.create().show()
                }
                return
            }
        }
    }

    private fun addDefaultEffects() {
        val defaultEffects = listOf("Gain", "Echo", "Tremolo")
        for (effectName in defaultEffects) {
            NativeInterface.effectDescriptionMap[effectName]?.let {
                val toAdd = Effect(it)
                EffectsAdapter.effectList.add(toAdd)
                NativeInterface.addEffect(toAdd)
            }
        }
        EffectsAdapter.notifyDataSetChanged()
    }

    @RequiresApi(Build.VERSION_CODES.M)
    private fun handleMidiDevices() {

        val midiManager = getSystemService(Context.MIDI_SERVICE) as MidiManager
        midiManager.registerDeviceCallback(object : MidiManager.DeviceCallback() {
            override fun onDeviceAdded(device: MidiDeviceInfo) {

                // open this device
                midiManager.openDevice(device, {
                    Log.d(TAG, "Opened MIDI device")

                    val targetSeekBar = findViewById<SeekBar>(R.id.seekBar)
                    if (targetSeekBar != null) {

                        val midiReceiver = MyMidiReceiver(targetSeekBar)
                        val outputPort = it.openOutputPort(0)
                        outputPort?.connect(midiReceiver)
                    }

                }, Handler())
            }
        }, Handler())
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        getMenuInflater().inflate(R.menu.toolbar_menu, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem) = when (item.itemId) {
        R.id.action_toggle_mute -> {
            isAudioEnabled = !isAudioEnabled
            NativeInterface.enable(isAudioEnabled)

            if (isAudioEnabled) {
                item.setIcon(R.drawable.ic_baseline_audio_is_enabled_24)
            } else {
                item.setIcon(R.drawable.ic_baseline_audio_is_disabled_24)
            }
            true
        }
        else -> {
            super.onOptionsItemSelected(item)
        }
    }

    @RequiresApi(Build.VERSION_CODES.M)
    class MyMidiReceiver(var seekBar: SeekBar) : MidiReceiver() {

        private val TAG: String = "MyMidiReceiver"

        override fun onSend(data: ByteArray?, offset: Int, count: Int, timestamp: Long) {

            Log.d(TAG, "Got midi message, offset " + offset + " count " + count)
            Log.d(TAG, "Byte 0 " + Integer.toHexString(data!![offset].toInt()))
            Log.d(TAG, "Byte 1 " + Integer.toHexString(data[offset+1].toInt()))
            Log.d(TAG, "Byte 2 " + data[offset+2].toInt())

            val CONTROL_CHANGE_CH1 : Byte = 0xB0.toByte()

            if (data[offset] == CONTROL_CHANGE_CH1){
                seekBar.progress = (data[offset+2].toInt() / 1.27).toInt()
            }
        }
    }

}

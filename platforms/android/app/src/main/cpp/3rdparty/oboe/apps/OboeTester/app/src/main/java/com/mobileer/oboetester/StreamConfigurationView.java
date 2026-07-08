/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.mobileer.oboetester;

import android.content.Context;
import android.media.AudioManager;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.AutomaticGainControl;
import android.media.audiofx.BassBoost;
import android.media.audiofx.LoudnessEnhancer;
import android.media.audiofx.NoiseSuppressor;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TableRow;
import android.widget.TextView;

import com.mobileer.audio_device.AudioDeviceListEntry;
import com.mobileer.audio_device.AudioDeviceSpinner;

import java.util.Locale;

/**
 * View for Editing a requested StreamConfiguration
 * and displaying the actual StreamConfiguration.
 */

public class StreamConfigurationView extends LinearLayout {
    private static final String TAG = "StreamConfigurationView";

    protected Spinner mNativeApiSpinner;
    private TextView mActualNativeApiView;
    private TextView mActualDeviceIdView;

    private TextView mActualMMapView;
    private CheckBox mRequestedMMapView;
    private TextView mActualExclusiveView;
    private TextView mActualPerformanceView;
    private Spinner  mPerformanceSpinner;
    private CheckBox mRequestedExclusiveView;
    private CheckBox mChannelConversionBox;
    private CheckBox mFormatConversionBox;
    private Spinner  mChannelCountSpinner;
    private TextView mActualChannelCountView;
    private Spinner mChannelMaskSpinner;
    private TextView mActualChannelMaskView;
    private TextView mActualFormatView;
    private Spinner  mCapacitySpinner;
    private TextView mActualCapacityView;
    private TableRow mInputPresetTableRow;
    private Spinner  mInputPresetSpinner;
    private TextView mActualInputPresetView;

    private TableRow mUsageTableRow;
    private Spinner  mUsageSpinner;
    private TextView mActualUsageView;

    private TableRow mContentTypeTableRow;
    private Spinner  mContentTypeSpinner;
    private TextView mActualContentTypeView;

    private TableRow mSpatializationBehaviorTableRow;
    private Spinner  mSpatializationBehaviorSpinner;
    private TextView mActualSpatializationBehaviorView;

    private TableRow mPackageNameTableRow;
    private Spinner  mPackageNameSpinner;
    private TextView mActualPackageNameView;

    private TableRow mAttributionTagTableRow;
    private Spinner  mAttributionTagSpinner;
    private TextView mActualAttributionTagView;

    private Spinner  mFormatSpinner;
    private Spinner  mSampleRateSpinner;
    private Spinner  mRateConversionQualitySpinner;
    private TextView mActualSampleRateView;
    private LinearLayout mHideableView;

    private AudioDeviceSpinner mDeviceSpinner;
    private TextView mActualSessionIdView;
    private CheckBox mRequestAudioEffect;
    private CheckBox mRequestSessionId;

    private TextView mStreamInfoView;
    private TextView mStreamStatusView;
    private TextView mOptionExpander;
    private String mHideSettingsText;
    private String mShowSettingsText;

    private LinearLayout mInputEffectsLayout;
    private LinearLayout mOutputEffectsLayout;

    private CheckBox mAutomaticGainControlCheckBox;
    private CharSequence mAutomaticGainControlText;
    private CheckBox mAcousticEchoCancelerCheckBox;
    private CharSequence mAcousticEchoCancelerText;
    private CheckBox mNoiseSuppressorCheckBox;
    private CharSequence mNoiseSuppressorText;
    private CheckBox mBassBoostCheckBox;
    private SeekBar mBassBoostSeekBar;
    private CheckBox mLoudnessEnhancerCheckBox;
    private SeekBar mLoudnessEnhancerSeekBar;

    private boolean mIsChannelMaskLastSelected;

    private boolean misOutput;

    private BassBoost mBassBoost;
    private LoudnessEnhancer mLoudnessEnhancer;
    private AutomaticGainControl mAutomaticGainControl;
    private AcousticEchoCanceler mAcousticEchoCanceler;
    private NoiseSuppressor mNoiseSuppressor;

    // Create an anonymous implementation of OnClickListener
    private View.OnClickListener mToggleListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (mHideableView.isShown()) {
                hideSettingsView();
            } else {
                showSettingsView();
            }
        }
    };

    public static String yesOrNo(boolean b) {
        return b ?  "YES" : "NO";
    }

    private void updateSettingsViewText() {
        if (mHideableView.isShown()) {
            mOptionExpander.setText(mHideSettingsText);
        } else {
            mOptionExpander.setText(mShowSettingsText);
        }
    }

    public void showSettingsView() {
        mHideableView.setVisibility(View.VISIBLE);
        updateSettingsViewText();
    }

    public void hideSampleRateMenu() {
        if (mSampleRateSpinner != null) {
            mSampleRateSpinner.setVisibility(View.GONE);
        }
    }

    public void hideSettingsView() {
        mHideableView.setVisibility(View.GONE);
        updateSettingsViewText();
    }

    public StreamConfigurationView(Context context) {
        super(context);
        initializeViews(context);
    }

    public StreamConfigurationView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context);
    }

    public StreamConfigurationView(Context context,
                                   AttributeSet attrs,
                                   int defStyle) {
        super(context, attrs, defStyle);
        initializeViews(context);
    }

    /**
     * Inflates the views in the layout.
     *
     * @param context
     *           the current context for the view.
     */
    private void initializeViews(Context context) {
        LayoutInflater inflater = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.stream_config, this);

        mHideSettingsText = getResources().getString(R.string.hint_hide_settings);
        mShowSettingsText = getResources().getString(R.string.hint_show_settings);

        mHideableView = (LinearLayout) findViewById(R.id.hideableView);

        mOptionExpander = (TextView) findViewById(R.id.toggle_stream_config);
        mOptionExpander.setOnClickListener(mToggleListener);

        mNativeApiSpinner = (Spinner) findViewById(R.id.spinnerNativeApi);
        mNativeApiSpinner.setSelection(StreamConfiguration.NATIVE_API_UNSPECIFIED);

        mActualNativeApiView = (TextView) findViewById(R.id.actualNativeApi);

        mActualDeviceIdView = (TextView) findViewById(R.id.actualDeviceId);

        mChannelConversionBox = (CheckBox) findViewById(R.id.checkChannelConversion);

        mFormatConversionBox = (CheckBox) findViewById(R.id.checkFormatConversion);

        mActualMMapView = (TextView) findViewById(R.id.actualMMap);
        mRequestedMMapView = (CheckBox) findViewById(R.id.requestedMMapEnable);
        boolean mmapSupported = NativeEngine.isMMapSupported();
        mRequestedMMapView.setEnabled(mmapSupported);
        mRequestedMMapView.setChecked(mmapSupported);

        mActualExclusiveView = (TextView) findViewById(R.id.actualExclusiveMode);
        mRequestedExclusiveView = (CheckBox) findViewById(R.id.requestedExclusiveMode);

        boolean mmapExclusiveSupported = NativeEngine.isMMapExclusiveSupported();
        mRequestedExclusiveView.setEnabled(mmapExclusiveSupported);
        mRequestedExclusiveView.setChecked(mmapExclusiveSupported);

        mRequestSessionId = (CheckBox) findViewById(R.id.requestSessionId);
        mActualSessionIdView = (TextView) findViewById(R.id.sessionId);
        mRequestAudioEffect = (CheckBox) findViewById(R.id.requestAudioEffect);

        mOutputEffectsLayout = (LinearLayout) findViewById(R.id.outputEffects);
        mInputEffectsLayout = (LinearLayout) findViewById(R.id.inputEffects);

        mRequestAudioEffect.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onRequestAudioEffectClicked(((CheckBox) view).isChecked());
            }
        });

        mAutomaticGainControlCheckBox = (CheckBox) findViewById(R.id.checkBoxAutomaticGainControl);
        mAcousticEchoCancelerCheckBox = (CheckBox) findViewById(R.id.checkBoxAcousticEchoCanceler);
        mNoiseSuppressorCheckBox = (CheckBox) findViewById(R.id.checkBoxNoiseSuppressor);
        mBassBoostCheckBox = (CheckBox) findViewById(R.id.checkBoxBassBoost);
        mBassBoostSeekBar = (SeekBar) findViewById(R.id.seekBarBassBoost);
        mBassBoostSeekBar.setEnabled(mBassBoostCheckBox.isChecked());
        mLoudnessEnhancerCheckBox = (CheckBox) findViewById(R.id.checkBoxLoudnessEnhancer);
        mLoudnessEnhancerSeekBar = (SeekBar) findViewById(R.id.seekBarLoudnessEnhancer);
        mLoudnessEnhancerSeekBar.setEnabled(mLoudnessEnhancerCheckBox.isChecked());

        mAutomaticGainControlCheckBox.setEnabled(AutomaticGainControl.isAvailable());
        mAutomaticGainControlText = mAutomaticGainControlCheckBox.getText();
        mAcousticEchoCancelerCheckBox.setEnabled(AcousticEchoCanceler.isAvailable());
        mAcousticEchoCancelerText = mAcousticEchoCancelerCheckBox.getText();
        mNoiseSuppressorCheckBox.setEnabled(NoiseSuppressor.isAvailable());
        mNoiseSuppressorText = mNoiseSuppressorCheckBox.getText();

        mBassBoostSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                onBassBoostSeekBarChanged(progress);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        mLoudnessEnhancerSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                onLoudnessEnhancerSeekBarChanged(progress);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        mAutomaticGainControlCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                onAutomaticGainControlCheckBoxChanged(isChecked);
            }
        });

        mAcousticEchoCancelerCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                onAcousticEchoCancelerCheckBoxChanged(isChecked);
            }
        });
        mNoiseSuppressorCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                onNoiseSuppressorCheckBoxChanged(isChecked);
            }
        });
        mBassBoostCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                onBassBoostCheckBoxChanged(isChecked);
            }
        });
        mLoudnessEnhancerCheckBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                onLoudnessEnhancerCheckBoxChanged(isChecked);
            }
        });

        mActualSampleRateView = (TextView) findViewById(R.id.actualSampleRate);
        mSampleRateSpinner = (Spinner) findViewById(R.id.spinnerSampleRate);
        mActualChannelCountView = (TextView) findViewById(R.id.actualChannelCount);
        mChannelCountSpinner = (Spinner) findViewById(R.id.spinnerChannelCount);
        mChannelCountSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                onChannelCountSpinnerSelected();
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
                // no-op
            }
        });
        mActualFormatView = (TextView) findViewById(R.id.actualAudioFormat);
        mFormatSpinner = (Spinner) findViewById(R.id.spinnerFormat);

        mActualCapacityView = (TextView) findViewById(R.id.actualCapacity);
        mCapacitySpinner = (Spinner) findViewById(R.id.spinnerCapacity);

        mRateConversionQualitySpinner = (Spinner) findViewById(R.id.spinnerSRCQuality);

        mActualPerformanceView = (TextView) findViewById(R.id.actualPerformanceMode);
        mPerformanceSpinner = (Spinner) findViewById(R.id.spinnerPerformanceMode);
        mPerformanceSpinner.setSelection(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY
                - StreamConfiguration.PERFORMANCE_MODE_NONE);

        mInputPresetTableRow = (TableRow) findViewById(R.id.rowInputPreset);
        mActualInputPresetView = (TextView) findViewById(R.id.actualInputPreset);
        mInputPresetSpinner = (Spinner) findViewById(R.id.spinnerInputPreset);
        mInputPresetSpinner.setSelection(2); // TODO need better way to select voice recording default

        mUsageTableRow = (TableRow) findViewById(R.id.rowUsage);
        mActualUsageView = (TextView) findViewById(R.id.actualUsage);
        mUsageSpinner = (Spinner) findViewById(R.id.spinnerUsage);

        mContentTypeTableRow = (TableRow) findViewById(R.id.rowContentType);
        mActualContentTypeView = (TextView) findViewById(R.id.actualContentType);
        mContentTypeSpinner = (Spinner) findViewById(R.id.spinnerContentType);

        mSpatializationBehaviorTableRow = (TableRow) findViewById(R.id.rowSpatializationBehavior);
        mActualSpatializationBehaviorView = (TextView) findViewById(R.id.actualSpatializationBehavior);
        mSpatializationBehaviorSpinner = (Spinner) findViewById(R.id.spinnerSpatializationBehavior);

        mPackageNameTableRow = (TableRow) findViewById(R.id.rowPackageName);
        mActualPackageNameView = (TextView) findViewById(R.id.actualPackageName);
        mPackageNameSpinner = (Spinner) findViewById(R.id.spinnerPackageName);

        mAttributionTagTableRow = (TableRow) findViewById(R.id.rowAttributionTag);
        mActualAttributionTagView = (TextView) findViewById(R.id.actualAttributionTag);
        mAttributionTagSpinner = (Spinner) findViewById(R.id.spinnerAttributionTag);

        mStreamInfoView = (TextView) findViewById(R.id.streamInfo);

        mStreamStatusView = (TextView) findViewById(R.id.statusView);

        mDeviceSpinner = (AudioDeviceSpinner) findViewById(R.id.devices_spinner);

        mActualChannelMaskView = (TextView) findViewById(R.id.actualChannelMask);
        mChannelMaskSpinner = (Spinner) findViewById(R.id.spinnerChannelMask);
        ArrayAdapter<String> channelMaskSpinnerArrayAdapter = new ArrayAdapter<String>(context,
                android.R.layout.simple_spinner_item,
                StreamConfiguration.getAllChannelMasks());
        mChannelMaskSpinner.setAdapter(channelMaskSpinnerArrayAdapter);
        mChannelMaskSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                onChannelMaskSpinnerSelected();
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
                // no-op
            }
        });

        showSettingsView();
    }

    public void setOutput(boolean output) {
        misOutput = output;
        String ioText;
        if (output) {
            mDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_OUTPUTS);
            ioText = "OUTPUT";
        } else {
            mDeviceSpinner.setDirectionType(AudioManager.GET_DEVICES_INPUTS);
            ioText = "INPUT";
        }
        mHideSettingsText = getResources().getString(R.string.hint_hide_settings) + " - " + ioText;
        mShowSettingsText = getResources().getString(R.string.hint_show_settings) + " - " + ioText;
        updateSettingsViewText();

        // Don't show InputPresets for output streams.
        mInputPresetTableRow.setVisibility(output ? View.GONE : View.VISIBLE);
        // Don't show Usage and Content Type for input streams.
        mUsageTableRow.setVisibility(output ? View.VISIBLE : View.GONE);
        mContentTypeTableRow.setVisibility(output ? View.VISIBLE : View.GONE);
    }

    public void applyToModel(StreamConfiguration config) {
        // Menu position matches actual enum value for these properties.
        config.setNativeApi(mNativeApiSpinner.getSelectedItemPosition());
        config.setFormat(mFormatSpinner.getSelectedItemPosition());
        config.setRateConversionQuality(mRateConversionQualitySpinner.getSelectedItemPosition());

        int id =  ((AudioDeviceListEntry) mDeviceSpinner.getSelectedItem()).getId();
        config.setDeviceId(id);

        String text = mSampleRateSpinner.getSelectedItem().toString();
        int sampleRate = Integer.parseInt(text);
        config.setSampleRate(sampleRate);

        text = mInputPresetSpinner.getSelectedItem().toString();
        int inputPreset = StreamConfiguration.convertTextToInputPreset(text);
        config.setInputPreset(inputPreset);

        text = mUsageSpinner.getSelectedItem().toString();
        int usage = StreamConfiguration.convertTextToUsage(text);
        config.setUsage(usage);

        text = mContentTypeSpinner.getSelectedItem().toString();
        int contentType = StreamConfiguration.convertTextToContentType(text);
        config.setContentType(contentType);

        text = mSpatializationBehaviorSpinner.getSelectedItem().toString();
        int spatializationBehavior = StreamConfiguration.convertTextToSpatializationBehavior(text);
        config.setSpatializationBehavior(spatializationBehavior);

        text = mPackageNameSpinner.getSelectedItem().toString();
        config.setPackageName(text);

        text = mAttributionTagSpinner.getSelectedItem().toString();
        config.setAttributionTag(text);

        // The corresponding channel count of the selected channel mask may be different from
        // the selected channel count, the last selected will be respected.
        if (mIsChannelMaskLastSelected) {
            text = mChannelMaskSpinner.getSelectedItem().toString();
            int channelMask = StreamConfiguration.convertTextToChannelMask(text);
            config.setChannelMask(channelMask);
            config.setChannelCount(0);
            Log.d(TAG, String.format(Locale.getDefault(), "Set channel mask as %s(%#x)", text, channelMask));
        } else {
            config.setChannelCount(mChannelCountSpinner.getSelectedItemPosition());
            config.setChannelMask(StreamConfiguration.UNSPECIFIED);
            Log.d(TAG, "Set channel count as " + mChannelCountSpinner.getSelectedItemPosition());
        }

        text = mCapacitySpinner.getSelectedItem().toString();
        int bufferCapacity = Integer.parseInt(text);
        config.setBufferCapacityInFrames(bufferCapacity);

        config.setMMap(mRequestedMMapView.isChecked());
        config.setChannelConversionAllowed(mChannelConversionBox.isChecked());
        config.setFormatConversionAllowed(mFormatConversionBox.isChecked());
        config.setSharingMode(mRequestedExclusiveView.isChecked()
                ? StreamConfiguration.SHARING_MODE_EXCLUSIVE
                : StreamConfiguration.SHARING_MODE_SHARED);
        config.setSessionId(mRequestSessionId.isChecked()
                ? StreamConfiguration.SESSION_ID_ALLOCATE
                : StreamConfiguration.SESSION_ID_NONE);

        config.setPerformanceMode(mPerformanceSpinner.getSelectedItemPosition()
                + StreamConfiguration.PERFORMANCE_MODE_NONE);
    }

    public void setChildrenEnabled(boolean enabled) {
        mNativeApiSpinner.setEnabled(enabled);
        mPerformanceSpinner.setEnabled(enabled);
        mRequestedMMapView.setEnabled(enabled && NativeEngine.isMMapSupported());
        mRequestedExclusiveView.setEnabled(enabled && NativeEngine.isMMapExclusiveSupported());
        mChannelConversionBox.setEnabled(enabled);
        mFormatConversionBox.setEnabled(enabled);
        mChannelCountSpinner.setEnabled(enabled);
        mChannelMaskSpinner.setEnabled(enabled);
        mCapacitySpinner.setEnabled(enabled);
        mInputPresetSpinner.setEnabled(enabled);
        mUsageSpinner.setEnabled(enabled);
        mContentTypeSpinner.setEnabled(enabled);
        mFormatSpinner.setEnabled(enabled);
        mSpatializationBehaviorSpinner.setEnabled(enabled);
        mPackageNameSpinner.setEnabled(enabled);
        mAttributionTagSpinner.setEnabled(enabled);
        mSampleRateSpinner.setEnabled(enabled);
        mRateConversionQualitySpinner.setEnabled(enabled);
        mDeviceSpinner.setEnabled(enabled);
        mRequestSessionId.setEnabled(enabled);
        mRequestAudioEffect.setEnabled(enabled);
    }

    // This must be called on the UI thread.
    void updateDisplay(StreamConfiguration actualConfiguration) {
        int value;

        value = actualConfiguration.getNativeApi();
        mActualNativeApiView.setText(StreamConfiguration.convertNativeApiToText(value));

        String deviceIdsText = StreamConfiguration.convertDeviceIdsToText(
                actualConfiguration.getDeviceIds());
        mActualDeviceIdView.setText(deviceIdsText);

        mActualMMapView.setText(yesOrNo(actualConfiguration.isMMap()));
        int sharingMode = actualConfiguration.getSharingMode();
        boolean isExclusive = (sharingMode == StreamConfiguration.SHARING_MODE_EXCLUSIVE);
        mActualExclusiveView.setText(yesOrNo(isExclusive));

        value = actualConfiguration.getPerformanceMode();
        mActualPerformanceView.setText(StreamConfiguration.convertPerformanceModeToText(value));
        mActualPerformanceView.requestLayout();

        value = actualConfiguration.getFormat();
        mActualFormatView.setText(StreamConfiguration.convertFormatToText(value));
        mActualFormatView.requestLayout();

        value = actualConfiguration.getInputPreset();
        mActualInputPresetView.setText(StreamConfiguration.convertInputPresetToText(value));
        mActualInputPresetView.requestLayout();

        value = actualConfiguration.getUsage();
        mActualUsageView.setText(StreamConfiguration.convertUsageToText(value));
        mActualUsageView.requestLayout();

        value = actualConfiguration.getContentType();
        mActualContentTypeView.setText(StreamConfiguration.convertContentTypeToText(value));
        mActualContentTypeView.requestLayout();

        value = actualConfiguration.getSpatializationBehavior();
        mActualSpatializationBehaviorView.setText(StreamConfiguration.convertSpatializationBehaviorToText(value));
        mActualSpatializationBehaviorView.requestLayout();

        String stringValue = actualConfiguration.getPackageName();
        mActualPackageNameView.setText(stringValue);
        mActualPackageNameView.requestLayout();

        stringValue = actualConfiguration.getAttributionTag();
        mActualAttributionTagView.setText(stringValue);
        mActualAttributionTagView.requestLayout();

        mActualChannelCountView.setText(actualConfiguration.getChannelCount() + "");
        mActualSampleRateView.setText(actualConfiguration.getSampleRate() + "");
        mActualSessionIdView.setText("S#: " + actualConfiguration.getSessionId());
        value = actualConfiguration.getChannelMask();
        mActualChannelMaskView.setText(StreamConfiguration.convertChannelMaskToText(value));
        mActualCapacityView.setText(actualConfiguration.getBufferCapacityInFrames() + "");

        boolean isMMap = actualConfiguration.isMMap();

        String msg = "";
        msg += "burst = " + actualConfiguration.getFramesPerBurst();
        msg += ", devIDs = " + deviceIdsText;
        msg += ", " + (actualConfiguration.isMMap() ? "MMAP" : "Legacy");
        msg += (isMMap ? ", " + StreamConfiguration.convertSharingModeToText(sharingMode) : "");

        int hardwareChannelCount = actualConfiguration.getHardwareChannelCount();
        int hardwareSampleRate = actualConfiguration.getHardwareSampleRate();
        int hardwareFormat = actualConfiguration.getHardwareFormat();
        msg += "\nHW: #ch=" + (hardwareChannelCount ==
                StreamConfiguration.UNSPECIFIED ? "?" : hardwareChannelCount);
        msg += ", SR=" + (hardwareSampleRate ==
                StreamConfiguration.UNSPECIFIED ? "?" : hardwareSampleRate);
        msg += ", format=" + (hardwareFormat == StreamConfiguration.UNSPECIFIED ?
               "?" : StreamConfiguration.convertFormatToText(hardwareFormat));

        mStreamInfoView.setText(msg);

        mHideableView.requestLayout();
    }

    // This must be called on the UI thread.
    public void setStatusText(String msg) {
        mStreamStatusView.setText(msg);
    }

    public void setExclusiveMode(boolean b) {
        mRequestedExclusiveView.setChecked(b);
    }

    public void setFormat(int format) {
        mFormatSpinner.setSelection(format); // position matches format
    }

    public void setFormatConversionAllowed(boolean allowed) {
        mFormatConversionBox.setChecked(allowed);
    }

    private void onChannelCountSpinnerSelected() {
        if (mChannelCountSpinner.getSelectedItemPosition() != 0) {
            mChannelMaskSpinner.setSelection(0); // Override the previous channel mask selection
            mIsChannelMaskLastSelected = false;
        }
    }

    private void onChannelMaskSpinnerSelected() {
        if (mChannelMaskSpinner.getSelectedItemPosition() != 0) {
            mChannelCountSpinner.setSelection(0); // Override the previous channel count selection
            mIsChannelMaskLastSelected = true;
        }
    }

    private void onRequestAudioEffectClicked(boolean isChecked) {
        if (isChecked) {
            mRequestSessionId.setEnabled(false);
            mRequestSessionId.setChecked(true);
            if (misOutput) {
                mOutputEffectsLayout.setVisibility(VISIBLE);
            } else {
                mInputEffectsLayout.setVisibility(VISIBLE);
            }
        } else {
            mRequestSessionId.setEnabled(true);
            if (misOutput) {
                mOutputEffectsLayout.setVisibility(GONE);
            } else {
                mInputEffectsLayout.setVisibility(GONE);
            }
        }
    }

    public void setupEffects(int sessionId) {
        if (!mRequestAudioEffect.isChecked()) {
            return;
        }
        if (misOutput) {
            mBassBoost = new BassBoost(0, sessionId);
            mBassBoost.setStrength((short) mBassBoostSeekBar.getProgress());
            mBassBoost.setEnabled(mBassBoostCheckBox.isChecked());
            mBassBoostSeekBar.setEnabled(mBassBoostCheckBox.isChecked());
            mLoudnessEnhancer = new LoudnessEnhancer(sessionId);
            mLoudnessEnhancer.setTargetGain((short) mLoudnessEnhancerSeekBar.getProgress());
            mLoudnessEnhancer.setEnabled(mLoudnessEnhancerCheckBox.isChecked());
            mLoudnessEnhancerSeekBar.setEnabled(mLoudnessEnhancerCheckBox.isChecked());
        } else {
            // If AEC is not available, the checkbox will be disabled in initializeViews().
            if (mAcousticEchoCancelerCheckBox.isEnabled()) {
                mAcousticEchoCanceler = AcousticEchoCanceler.create(sessionId);
                if (mAcousticEchoCanceler != null) {
                    boolean wasOn = mAcousticEchoCanceler.getEnabled();
                    String text = mAcousticEchoCancelerText + "(" + (wasOn ? "Y" : "N") + ")";
                    mAcousticEchoCancelerCheckBox.setText(text);
                    mAcousticEchoCanceler.setEnabled(mAcousticEchoCancelerCheckBox.isChecked());
                } else {
                    Log.e(TAG, String.format(Locale.getDefault(), "Could not create AcousticEchoCanceler"));
                }
            }

            // If AGC is not available, the checkbox will be disabled in initializeViews().
            if (mAutomaticGainControlCheckBox.isEnabled()) {
                mAutomaticGainControl = AutomaticGainControl.create(sessionId);
                if (mAutomaticGainControl != null) {
                    boolean wasOn = mAutomaticGainControl.getEnabled();
                    String text = mAutomaticGainControlText + "(" + (wasOn ? "Y" : "N") + ")";
                    mAutomaticGainControlCheckBox.setText(text);
                    mAutomaticGainControl.setEnabled(mAutomaticGainControlCheckBox.isChecked());
                } else {
                    Log.e(TAG, String.format(Locale.getDefault(), "Could not create AutomaticGainControl"));
                }
            }

            // If Noise Suppressor is not available, the checkbox will be disabled in initializeViews().
            if (mNoiseSuppressorCheckBox.isEnabled()) {
                mNoiseSuppressor = NoiseSuppressor.create(sessionId);
                if (mNoiseSuppressor != null) {
                    boolean wasOn = mNoiseSuppressor.getEnabled();
                    String text = mNoiseSuppressorText + "(" + (wasOn ? "Y" : "N") + ")";
                    mNoiseSuppressorCheckBox.setText(text);
                    mNoiseSuppressor.setEnabled(mNoiseSuppressorCheckBox.isChecked());
                } else {
                    Log.e(TAG, String.format(Locale.getDefault(), "Could not create NoiseSuppressor"));
                }
            }
        }
    }

    private void onLoudnessEnhancerSeekBarChanged(int progress) {
        if (mLoudnessEnhancer != null) {
            mLoudnessEnhancer.setTargetGain(progress);
        }
        mLoudnessEnhancerCheckBox.setText("Loudness Enhancer: " + progress);
    }

    private void onBassBoostSeekBarChanged(int progress) {
        if (mBassBoost != null) {
            mBassBoost.setStrength((short) progress);
        }
        mBassBoostCheckBox.setText("Bass Boost: " + progress);
    }

    private void onAutomaticGainControlCheckBoxChanged(boolean isChecked) {
        if (mAutomaticGainControlCheckBox.isEnabled() && mAutomaticGainControl != null) {
            mAutomaticGainControl.setEnabled(isChecked);
        }
    }

    private void onAcousticEchoCancelerCheckBoxChanged(boolean isChecked) {
        if (mAcousticEchoCancelerCheckBox.isEnabled() && mAcousticEchoCanceler != null) {
            mAcousticEchoCanceler.setEnabled(isChecked);
        }
    }

    private void onNoiseSuppressorCheckBoxChanged(boolean isChecked) {
        if (mNoiseSuppressorCheckBox.isEnabled() && mNoiseSuppressor != null) {
            mNoiseSuppressor.setEnabled(isChecked);
        }
    }

    private void onBassBoostCheckBoxChanged(boolean isChecked) {
        if (mBassBoost != null) {
            mBassBoost.setEnabled(isChecked);
        }
        mBassBoostSeekBar.setEnabled(isChecked);
    }

    private void onLoudnessEnhancerCheckBoxChanged(boolean isChecked) {
        if (mLoudnessEnhancer != null) {
            mLoudnessEnhancer.setEnabled(isChecked);
        }
        mLoudnessEnhancerSeekBar.setEnabled(isChecked);
    }
}

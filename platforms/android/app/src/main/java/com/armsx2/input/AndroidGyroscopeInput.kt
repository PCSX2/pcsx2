package com.armsx2.input

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.view.Surface
import android.view.WindowManager
import kotlin.math.PI

class AndroidGyroscopeInput(
    context: Context,
    private val onAnalog: (mode: Int, x: Float, y: Float) -> Unit
) : SensorEventListener {
    companion object {
        fun isModeAvailable(context: Context, mode: Int): Boolean {
            val manager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
            return when (mode) {
                1 -> manager.getDefaultSensor(Sensor.TYPE_GYROSCOPE) != null
                2 -> manager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR) != null
                else -> true
            }
        }
    }
    private val appContext = context.applicationContext
    private val sensorManager = appContext.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val windowManager = appContext.getSystemService(Context.WINDOW_SERVICE) as WindowManager
    private var activeSensor: Sensor? = null
    private var mode = 0
    private var sensitivity = 1f
    private var smoothing = 0.45f
    private var invertX = false
    private var invertY = false
    private var filteredX = 0f
    private var filteredY = 0f
    private var lastSentX = 0f
    private var lastSentY = 0f
    private var wasActive = false
    private var steeringCenter: Float? = null

    fun start(mode: Int, sensitivityPercent: Int, smoothingPercent: Int, invertX: Boolean, invertY: Boolean): Boolean {
        stop()
        this.mode = mode
        this.sensitivity = sensitivityPercent.coerceIn(25, 300) / 100f
        this.smoothing = smoothingPercent.coerceIn(0, 90) / 100f
        this.invertX = invertX
        this.invertY = invertY
        filteredX = 0f
        filteredY = 0f
        lastSentX = 0f
        lastSentY = 0f
        wasActive = false
        steeringCenter = null
        activeSensor = when (mode) {
            1 -> sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
            2 -> sensorManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR)
            else -> null
        }
        val sensor = activeSensor ?: return false
        return sensorManager.registerListener(this, sensor, SensorManager.SENSOR_DELAY_GAME)
    }

    fun recenter() {
        steeringCenter = null
        filteredX = 0f
        filteredY = 0f
        wasActive = false
        onAnalog(mode, 0f, 0f)
    }

    fun stop() {
        activeSensor?.let { sensorManager.unregisterListener(this, it) }
        activeSensor = null
        steeringCenter = null
        filteredX = 0f
        filteredY = 0f
        wasActive = false
        onAnalog(mode, 0f, 0f)
    }

    override fun onSensorChanged(event: SensorEvent) {
        val raw = when (mode) {
            1 -> aimValues(event)
            2 -> steeringValues(event)
            else -> return
        }
        var x = applyDeadzone(raw.first.coerceIn(-1f, 1f), 0.035f)
        var y = applyDeadzone(raw.second.coerceIn(-1f, 1f), 0.035f)
        if (invertX) x = -x
        if (invertY) y = -y
        val alpha = (1f - smoothing * 0.86f).coerceIn(0.16f, 1f)
        filteredX += (x - filteredX) * alpha
        filteredY += (y - filteredY) * alpha
        val outputX = filteredX.coerceIn(-1f, 1f)
        val outputY = filteredY.coerceIn(-1f, 1f)
        val active = kotlin.math.abs(outputX) > 0.004f || kotlin.math.abs(outputY) > 0.004f
        if (!active && !wasActive) return
        if (active && kotlin.math.abs(outputX - lastSentX) < 0.004f && kotlin.math.abs(outputY - lastSentY) < 0.004f) return
        val sentX = if (active) outputX else 0f
        val sentY = if (active) outputY else 0f
        lastSentX = sentX
        lastSentY = sentY
        wasActive = active
        onAnalog(mode, sentX, sentY)
    }

    private fun aimValues(event: SensorEvent): Pair<Float, Float> {
        if (event.sensor.type != Sensor.TYPE_GYROSCOPE) return 0f to 0f
        val rotation = @Suppress("DEPRECATION") windowManager.defaultDisplay.rotation
        val gx = event.values[0]
        val gy = event.values[1]
        val axes = when (rotation) {
            Surface.ROTATION_90 -> gx to gy
            Surface.ROTATION_270 -> -gx to -gy
            Surface.ROTATION_180 -> gy to -gx
            else -> -gy to -gx
        }
        return (axes.first * 0.72f * sensitivity) to (axes.second * 0.72f * sensitivity)
    }

    private fun steeringValues(event: SensorEvent): Pair<Float, Float> {
        if (event.sensor.type != Sensor.TYPE_GAME_ROTATION_VECTOR) return 0f to 0f
        val matrix = FloatArray(9)
        val remapped = FloatArray(9)
        SensorManager.getRotationMatrixFromVector(matrix, event.values)
        val rotation = @Suppress("DEPRECATION") windowManager.defaultDisplay.rotation
        val (axisX, axisY) = when (rotation) {
            Surface.ROTATION_90 -> SensorManager.AXIS_Y to SensorManager.AXIS_MINUS_X
            Surface.ROTATION_180 -> SensorManager.AXIS_MINUS_X to SensorManager.AXIS_MINUS_Y
            Surface.ROTATION_270 -> SensorManager.AXIS_MINUS_Y to SensorManager.AXIS_X
            else -> SensorManager.AXIS_X to SensorManager.AXIS_Y
        }
        SensorManager.remapCoordinateSystem(matrix, axisX, axisY, remapped)
        val roll = SensorManager.getOrientation(remapped, FloatArray(3))[2]
        val center = steeringCenter ?: roll.also { steeringCenter = it }
        var delta = roll - center
        while (delta > PI) delta -= (2 * PI).toFloat()
        while (delta < -PI) delta += (2 * PI).toFloat()
        val steeringRange = Math.toRadians(32.0).toFloat()
        return (delta / steeringRange * sensitivity).coerceIn(-1f, 1f) to 0f
    }

    private fun applyDeadzone(value: Float, deadzone: Float): Float {
        val magnitude = kotlin.math.abs(value)
        if (magnitude <= deadzone) return 0f
        return kotlin.math.sign(value) * ((magnitude - deadzone) / (1f - deadzone))
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) = Unit
}

package com.armsx2

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.armsx2.config.Settings
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ExampleInstrumentedTest {
    @Test
    fun useAppContext() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        assertEquals(BuildConfig.APPLICATION_ID, appContext.packageName)
    }

    @Test
    fun everySerializedSettingSupportsRoundTripDiffAndMerge() {
        val defaults = Settings()
        val serialized = defaults.toJson()
        assertJsonEquals(serialized, Settings.fromJson(serialized).toJson())

        serialized.keys().forEach { key ->
            val changed = changedValue(serialized.get(key))
            val override = JSONObject().put(key, changed)
            val merged = Settings.merge(defaults, override)
            val mergedJson = merged.toJson()
            assertTrue("merge omitted $key", mergedJson.has(key))
            assertJsonValueEquals("merge failed for $key", changed, mergedJson.get(key))

            val diff = Settings.diff(defaults, merged)
            assertTrue("diff omitted $key", diff.has(key))
            assertJsonValueEquals("diff failed for $key", changed, diff.get(key))
        }
    }

    private fun changedValue(value: Any): Any = when (value) {
        is Boolean -> !value
        is Int -> value + 1
        is Long -> value + 1L
        is Double -> value + 0.5
        is String -> "${value}x"
        is JSONArray -> JSONArray().put(
            JSONObject().put("url", "example.invalid").put("ip", "127.0.0.1").put("enabled", false),
        )
        else -> error("Unsupported JSON value ${value::class.java.name}")
    }

    private fun assertJsonEquals(expected: JSONObject, actual: JSONObject) {
        assertEquals(expected.keys().asSequence().toSet(), actual.keys().asSequence().toSet())
        expected.keys().forEach { key ->
            assertJsonValueEquals("round-trip failed for $key", expected.get(key), actual.get(key))
        }
    }

    private fun assertJsonValueEquals(message: String, expected: Any, actual: Any) {
        assertEquals(message, expected.toString(), actual.toString())
    }
}

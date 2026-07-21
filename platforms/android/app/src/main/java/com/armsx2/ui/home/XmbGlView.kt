package com.armsx2.ui.home

import android.content.Context
import android.graphics.SurfaceTexture
import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.EGLContext
import android.opengl.EGLDisplay
import android.opengl.EGLSurface
import android.opengl.GLES30
import android.util.Log
import android.view.TextureView
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin

/**
 * The animated PlayStation-3 XMB library background, ported from linkev's WebGL implementation
 * (github.com/linkev/PlayStation-3-XMB) — the same algorithm iOS renders in Metal.
 *
 * The wave is NOT a fragment shader (my first mistake): it's a displaced grid MESH. A flat
 * 100x100 grid is pushed in the vertex shader by a base spline curve plus flow/tension/FFD
 * terms, then shaded with a fresnel-edged translucent white — that's what gives the XMB its
 * signature flowing 3D ribbon rather than flat bands. The shaders below are linkev's GLSL ES
 * 3.00 verbatim; the base spline curve is filled on the CPU each frame (linkev's
 * writeDisplacementTexture) and uploaded as an R32F texture.
 *
 * A TextureView, not a GLSurfaceView: it composites inside the Compose view tree, so the library
 * cards / text / readability scrim layer over it correctly with no surface z-order juggling.
 * Rendered on its own EGL thread. Only the DEFAULT backdrop — a user-picked image overrides it
 * in HomeScreen and clearing that returns here.
 */
class XmbGlView(context: Context) : TextureView(context), TextureView.SurfaceTextureListener {
    private var thread: RenderThread? = null

    init {
        surfaceTextureListener = this
        // Non-opaque so that if EGL/GLES3 init ever fails and nothing renders, the still image
        // layered behind this view in HomeScreen shows through instead of a black rectangle. When
        // the GL path works it draws an opaque gradient every frame, fully covering that still.
        isOpaque = false
    }

    override fun onSurfaceTextureAvailable(st: SurfaceTexture, w: Int, h: Int) {
        thread = RenderThread(st, w, h).also { it.start() }
    }

    override fun onSurfaceTextureSizeChanged(st: SurfaceTexture, w: Int, h: Int) {
        thread?.resize(w, h)
    }

    override fun onSurfaceTextureDestroyed(st: SurfaceTexture): Boolean {
        thread?.finish()
        thread = null
        return true
    }

    override fun onSurfaceTextureUpdated(st: SurfaceTexture) {}

    private class RenderThread(
        private val surfaceTexture: SurfaceTexture,
        private var width: Int,
        private var height: Int,
    ) : Thread("xmb-gl") {
        @Volatile private var running = true
        @Volatile private var sizeDirty = true

        // ---- EGL ----
        private var eglDisplay: EGLDisplay = EGL14.EGL_NO_DISPLAY
        private var eglContext: EGLContext = EGL14.EGL_NO_CONTEXT
        private var eglSurface: EGLSurface = EGL14.EGL_NO_SURFACE

        // ---- GL objects ----
        private var bgProg = 0
        private var waveProg = 0
        private var bgVao = 0
        private var gridVao = 0
        private var gridIndexCount = 0
        private var splineTex = 0
        private val splineData = FloatArray(STEX_W * STEX_H)
        private val splineBuf: FloatBuffer =
            ByteBuffer.allocateDirect(STEX_W * STEX_H * 4).order(ByteOrder.nativeOrder()).asFloatBuffer()
        private val cp = FloatArray(28)
        private var startNanos = 0L

        fun resize(w: Int, h: Int) { width = w; height = h; sizeDirty = true }
        fun finish() { running = false; runCatching { join(500) } }

        override fun run() {
            if (!initEgl()) return
            runCatching { initGl() }.onFailure { Log.w(TAG, "GL init failed", it); teardown(); return }
            startNanos = System.nanoTime()
            while (running) {
                val frameStart = System.nanoTime()
                if (sizeDirty) { GLES30.glViewport(0, 0, width, height); sizeDirty = false }
                val t = (frameStart - startNanos) / 1_000_000_000f
                runCatching { drawFrame(t) }
                if (!EGL14.eglSwapBuffers(eglDisplay, eglSurface)) running = false
                // Cap to ~30 fps. The wave is slow, so 30 looks identical to 60 but roughly
                // halves GPU/CPU load — without this the loop ran flat-out at vsync (60) and
                // spun the RP6's fans up. Still more than a GIF (real mesh render vs a blit),
                // but well within a calm envelope.
                val leftMs = FRAME_TARGET_MS - (System.nanoTime() - frameStart) / 1_000_000L
                if (leftMs > 1) runCatching { sleep(leftMs) }
            }
            teardown()
        }

        // ---- rendering ----
        private fun drawFrame(timeSec: Float) {
            fillSpline(timeSec)
            splineBuf.clear(); splineBuf.put(splineData); splineBuf.position(0)
            GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, splineTex)
            GLES30.glTexSubImage2D(GLES30.GL_TEXTURE_2D, 0, 0, 0, STEX_W, STEX_H,
                GLES30.GL_RED, GLES30.GL_FLOAT, splineBuf)

            GLES30.glDisable(GLES30.GL_DEPTH_TEST)
            GLES30.glDisable(GLES30.GL_BLEND)

            // background gradient (full-screen quad)
            GLES30.glUseProgram(bgProg)
            GLES30.glBindVertexArray(bgVao)
            GLES30.glUniform3f(GLES30.glGetUniformLocation(bgProg, "uColorStart"), BG_TOP[0], BG_TOP[1], BG_TOP[2])
            GLES30.glUniform3f(GLES30.glGetUniformLocation(bgProg, "uColorEnd"), BG_BOT[0], BG_BOT[1], BG_BOT[2])
            GLES30.glUniform2f(GLES30.glGetUniformLocation(bgProg, "uDir"), 0f, 1f)
            GLES30.glUniform1f(GLES30.glGetUniformLocation(bgProg, "uTMin"), 0f)
            GLES30.glUniform1f(GLES30.glGetUniformLocation(bgProg, "uTSpan"), 1f)
            GLES30.glDrawArrays(GLES30.GL_TRIANGLE_STRIP, 0, 4)

            // wave mesh, alpha-blended over the gradient
            GLES30.glEnable(GLES30.GL_BLEND)
            GLES30.glBlendFunc(GLES30.GL_SRC_ALPHA, GLES30.GL_ONE_MINUS_SRC_ALPHA)
            GLES30.glUseProgram(waveProg)
            GLES30.glActiveTexture(GLES30.GL_TEXTURE0)
            GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, splineTex)
            fun u(n: String) = GLES30.glGetUniformLocation(waveProg, n)
            GLES30.glUniform1i(u("uSplineTex"), 0)
            GLES30.glUniform1f(u("uTime"), timeSec)
            GLES30.glUniform1f(u("flowSpeed"), FLOW_SPEED)
            GLES30.glUniform1f(u("tension"), TENSION)
            GLES30.glUniform1f(u("damping"), DAMPING)
            GLES30.glUniform1f(u("length"), LENGTH)
            GLES30.glUniform1f(u("spacing"), SPACING)
            GLES30.glUniform1f(u("perturbation"), PERTURBATION)
            GLES30.glUniform1f(u("perturbationScale"), PERTURBATION_SCALE)
            GLES30.glUniform1f(u("timeStep"), TIME_STEP)
            GLES30.glUniform1f(u("waveCosAmp"), WAVE_COS_AMP)
            GLES30.glUniform1f(u("waveBias"), WAVE_BIAS)
            GLES30.glUniform1f(u("waveHeightScale"), WAVE_HEIGHT_SCALE)
            GLES30.glUniform1f(u("waveSoftClip"), WAVE_SOFT_CLIP)
            GLES30.glUniform3f(u("ffdScale1"), FFD1[0], FFD1[1], FFD1[2])
            GLES30.glUniform3f(u("ffdScale2"), FFD2[0], FFD2[1], FFD2[2])
            GLES30.glUniform3f(u("ffdOffset"), FFD_OFF[0], FFD_OFF[1], FFD_OFF[2])
            GLES30.glUniform1f(u("ffdYAmp"), FFD_Y_AMP)
            GLES30.glUniform1f(u("ffdZAmp"), FFD_Z_AMP)
            GLES30.glUniform1f(u("zDetailScale"), Z_DETAIL_SCALE)
            GLES30.glUniform1f(u("opacity"), OPACITY)
            GLES30.glUniform1f(u("brightness"), BRIGHTNESS)
            GLES30.glUniform1f(u("fresnelPower"), FRESNEL_POWER)
            GLES30.glUniform1f(u("fresnelScale"), FRESNEL_SCALE)
            GLES30.glBindVertexArray(gridVao)
            GLES30.glDrawElements(GLES30.GL_TRIANGLE_STRIP, gridIndexCount, GLES30.GL_UNSIGNED_SHORT, 0)
            GLES30.glBindVertexArray(0)
        }

        /** Port of linkev writeDisplacementTexture: 28 control points per row from band + legacy
         *  sines (the reKernelGain=0.04 descriptor term is dropped as negligible), smoothed to the
         *  row by a cubic B-spline. */
        private fun fillSpline(timeSec: Float) {
            val flow = timeSec * FLOW_SPEED * TIME_STEP
            for (row in 0 until STEX_H) {
                val z = (row.toFloat() / (STEX_H - 1)) * 2f - 1f
                val rowBase = row * STEX_W
                val rowPhase = flow * 0.25f + z * 1.7f
                for (i in 0 until 28) {
                    val x = i.toFloat() / 27f
                    val reCore = sin(rowPhase + x * 6.2f) * BAND_AMP +
                        cos(z * BAND_SEC_FREQ + x * 4.8f + flow * 0.09f) * BAND_SEC_AMP
                    val legacy = sin((x * PIf * 1.3f + z * 0.8f) - flow * TRAVEL_SPEED1) * TRAVEL_AMP1 * TENSION +
                        sin((x * PIf * 2.8f - z * 1.2f) + flow * TRAVEL_SPEED2) * TRAVEL_AMP2 +
                        PERTURBATION * PERTURBATION_SCALE *
                        sin((x * (4f + LENGTH * 2f) + z * 4f - flow * 0.6f) * (SPACING * 0.01f))
                    cp[i] = reCore * RE_BLEND + legacy * (1f - RE_BLEND)
                }
                for (xi in 0 until STEX_W) splineData[rowBase + xi] = evalSpline(cp, xi.toFloat() / (STEX_W - 1))
            }
        }

        private fun evalSpline(c: FloatArray, u: Float): Float {
            val n = c.size - 3
            if (n < 1) return 0f
            val s = (u * n).coerceIn(0f, n - 1e-6f)
            val seg = floor(s).toInt()
            val t = s - seg
            val t2 = t * t; val t3 = t2 * t
            val b0 = (1 - 3 * t + 3 * t2 - t3) / 6f
            val b1 = (4 - 6 * t2 + 3 * t3) / 6f
            val b2 = (1 + 3 * t + 3 * t2 - 3 * t3) / 6f
            val b3 = t3 / 6f
            return b0 * c[seg] + b1 * c[seg + 1] + b2 * c[seg + 2] + b3 * c[seg + 3]
        }

        // ---- GL setup ----
        private fun initGl() {
            bgProg = link(BG_VS, BG_FS)
            waveProg = link(WAVE_VS, WAVE_FS)
            bgVao = makeFullscreenQuad()
            makeGrid(100)
            makeSplineTexture()
        }

        private fun compile(type: Int, src: String): Int {
            val s = GLES30.glCreateShader(type)
            GLES30.glShaderSource(s, src)
            GLES30.glCompileShader(s)
            val ok = IntArray(1)
            GLES30.glGetShaderiv(s, GLES30.GL_COMPILE_STATUS, ok, 0)
            if (ok[0] == 0) throw RuntimeException("shader compile: " + GLES30.glGetShaderInfoLog(s))
            return s
        }

        private fun link(vs: String, fs: String): Int {
            val p = GLES30.glCreateProgram()
            val v = compile(GLES30.GL_VERTEX_SHADER, vs)
            val f = compile(GLES30.GL_FRAGMENT_SHADER, fs)
            GLES30.glAttachShader(p, v); GLES30.glAttachShader(p, f)
            GLES30.glLinkProgram(p)
            GLES30.glDeleteShader(v); GLES30.glDeleteShader(f)
            val ok = IntArray(1)
            GLES30.glGetProgramiv(p, GLES30.GL_LINK_STATUS, ok, 0)
            if (ok[0] == 0) throw RuntimeException("program link: " + GLES30.glGetProgramInfoLog(p))
            return p
        }

        private fun makeFullscreenQuad(): Int {
            val verts = floatArrayOf(-1f, -1f, 1f, -1f, -1f, 1f, 1f, 1f)
            val vao = IntArray(1); GLES30.glGenVertexArrays(1, vao, 0); GLES30.glBindVertexArray(vao[0])
            val buf = IntArray(1); GLES30.glGenBuffers(1, buf, 0)
            GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, buf[0])
            val fb = ByteBuffer.allocateDirect(verts.size * 4).order(ByteOrder.nativeOrder()).asFloatBuffer()
            fb.put(verts).position(0)
            GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, verts.size * 4, fb, GLES30.GL_STATIC_DRAW)
            val loc = GLES30.glGetAttribLocation(bgProg, "aPos")
            GLES30.glEnableVertexAttribArray(loc)
            GLES30.glVertexAttribPointer(loc, 2, GLES30.GL_FLOAT, false, 0, 0)
            GLES30.glBindVertexArray(0)
            return vao[0]
        }

        private fun makeGrid(res: Int) {
            val strips = res - 1
            val vertsPerStrip = res * 2
            val verts = FloatArray(strips * vertsPerStrip * 2)
            var vi = 0
            for (y in 0 until strips) for (x in 0 until res) {
                val fx = (x.toFloat() / (res - 1)) * 2f - 1f
                verts[vi++] = fx; verts[vi++] = ((y + 1).toFloat() / (res - 1)) * 2f - 1f
                verts[vi++] = fx; verts[vi++] = (y.toFloat() / (res - 1)) * 2f - 1f
            }
            val idx = ShortArray(strips * (vertsPerStrip + 2) - 2)
            var ii = 0; var base = 0
            for (s in 0 until strips) {
                if (s > 0) { idx[ii++] = (base - 1).toShort(); idx[ii++] = base.toShort() }
                for (i in 0 until vertsPerStrip) idx[ii++] = (base + i).toShort()
                base += vertsPerStrip
            }
            gridIndexCount = ii
            val vao = IntArray(1); GLES30.glGenVertexArrays(1, vao, 0); GLES30.glBindVertexArray(vao[0])
            val vb = IntArray(1); GLES30.glGenBuffers(1, vb, 0)
            GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, vb[0])
            val vbuf = ByteBuffer.allocateDirect(verts.size * 4).order(ByteOrder.nativeOrder()).asFloatBuffer()
            vbuf.put(verts).position(0)
            GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, verts.size * 4, vbuf, GLES30.GL_STATIC_DRAW)
            val loc = GLES30.glGetAttribLocation(waveProg, "aPos")
            GLES30.glEnableVertexAttribArray(loc)
            GLES30.glVertexAttribPointer(loc, 2, GLES30.GL_FLOAT, false, 8, 0)
            val ib = IntArray(1); GLES30.glGenBuffers(1, ib, 0)
            GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, ib[0])
            val ibuf = ByteBuffer.allocateDirect(idx.size * 2).order(ByteOrder.nativeOrder()).asShortBuffer()
            ibuf.put(idx).position(0)
            GLES30.glBufferData(GLES30.GL_ELEMENT_ARRAY_BUFFER, idx.size * 2, ibuf, GLES30.GL_STATIC_DRAW)
            GLES30.glBindVertexArray(0)
            gridVao = vao[0]
        }

        private fun makeSplineTexture() {
            val tex = IntArray(1); GLES30.glGenTextures(1, tex, 0)
            GLES30.glBindTexture(GLES30.GL_TEXTURE_2D, tex[0])
            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_S, GLES30.GL_CLAMP_TO_EDGE)
            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_WRAP_T, GLES30.GL_CLAMP_TO_EDGE)
            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MIN_FILTER, GLES30.GL_LINEAR)
            GLES30.glTexParameteri(GLES30.GL_TEXTURE_2D, GLES30.GL_TEXTURE_MAG_FILTER, GLES30.GL_LINEAR)
            splineBuf.clear(); splineBuf.put(splineData); splineBuf.position(0)
            GLES30.glTexImage2D(GLES30.GL_TEXTURE_2D, 0, GLES30.GL_R32F, STEX_W, STEX_H, 0,
                GLES30.GL_RED, GLES30.GL_FLOAT, splineBuf)
            splineTex = tex[0]
        }

        // ---- EGL boilerplate ----
        private fun initEgl(): Boolean {
            eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
            if (eglDisplay == EGL14.EGL_NO_DISPLAY) return false
            val ver = IntArray(2)
            if (!EGL14.eglInitialize(eglDisplay, ver, 0, ver, 1)) return false
            val attribs = intArrayOf(
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT or 0x40, // ES2 | ES3
                EGL14.EGL_RED_SIZE, 8, EGL14.EGL_GREEN_SIZE, 8, EGL14.EGL_BLUE_SIZE, 8, EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_NONE,
            )
            val cfg = arrayOfNulls<EGLConfig>(1); val num = IntArray(1)
            if (!EGL14.eglChooseConfig(eglDisplay, attribs, 0, cfg, 0, 1, num, 0) || num[0] == 0) return false
            val ctxAttr = intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION, 3, EGL14.EGL_NONE)
            eglContext = EGL14.eglCreateContext(eglDisplay, cfg[0], EGL14.EGL_NO_CONTEXT, ctxAttr, 0)
            if (eglContext == EGL14.EGL_NO_CONTEXT) return false
            eglSurface = EGL14.eglCreateWindowSurface(eglDisplay, cfg[0], surfaceTexture, intArrayOf(EGL14.EGL_NONE), 0)
            if (eglSurface == EGL14.EGL_NO_SURFACE) return false
            return EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)
        }

        private fun teardown() {
            if (eglDisplay != EGL14.EGL_NO_DISPLAY) {
                EGL14.eglMakeCurrent(eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT)
                if (eglSurface != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(eglDisplay, eglSurface)
                if (eglContext != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(eglDisplay, eglContext)
                EGL14.eglTerminate(eglDisplay)
            }
            eglDisplay = EGL14.EGL_NO_DISPLAY; eglContext = EGL14.EGL_NO_CONTEXT; eglSurface = EGL14.EGL_NO_SURFACE
        }
    }

    companion object {
        private const val TAG = "XmbGlView"
        private const val PIf = PI.toFloat()
        private const val STEX_W = 256
        private const val STEX_H = 64

        private const val FRAME_TARGET_MS = 33L // ~30 fps cap (fan-friendly; wave is slow)

        // Gradient: a bold "eye-candy" PS3 blue. linkev's raw values (near-black -> #2559B3) read
        // as muted navy on-device, so the top is lifted to a deep blue and the bottom pushed to a
        // vivid bright royal blue. Tuned by eye against the reference on the RP6 panel.
        private val BG_TOP = floatArrayOf(0.02f, 0.07f, 0.20f)
        private val BG_BOT = floatArrayOf(0.18f, 0.46f, 0.96f)
        private const val FLOW_SPEED = 0.18f
        private const val TENSION = 0.12f
        private const val DAMPING = 0.0001f
        private const val LENGTH = 0.306001f
        private const val SPACING = 407.658f
        private const val TIME_STEP = 1.0f
        private const val BAND_AMP = 0.200f
        private const val BAND_SEC_FREQ = 7.0f
        private const val BAND_SEC_AMP = 0.025f
        private const val TRAVEL_SPEED1 = 0.25f
        private const val TRAVEL_AMP1 = 0.014f
        private const val TRAVEL_SPEED2 = 0.15f
        private const val TRAVEL_AMP2 = 0.008f
        private const val PERTURBATION = 0.0998587f
        private const val PERTURBATION_SCALE = 0.07f
        private const val WAVE_COS_AMP = 0.09f
        private const val WAVE_BIAS = -0.1f
        private const val WAVE_HEIGHT_SCALE = 0.5f
        private const val WAVE_SOFT_CLIP = 0.22f
        private const val RE_BLEND = 0.45f
        private const val FRESNEL_POWER = 4.0f
        private const val FRESNEL_SCALE = 0.5f
        private const val OPACITY = 0.7f
        private const val BRIGHTNESS = 0.98f
        private const val Z_DETAIL_SCALE = 0.08f
        private val FFD1 = floatArrayOf(5.67726f, 1.00077f, 1.0f)
        private val FFD2 = floatArrayOf(2.82755f, 1.27579f, 2.88782f)
        private val FFD_OFF = floatArrayOf(0.0f, -0.469999f, 0.0f)
        private const val FFD_Y_AMP = 0.05f
        private const val FFD_Z_AMP = 0.06f

        private const val BG_VS = """#version 300 es
precision highp float;
in vec2 aPos;
out vec2 vUvYDown;
void main() {
  vUvYDown = vec2(aPos.x * 0.5 + 0.5, 1.0 - (aPos.y * 0.5 + 0.5));
  gl_Position = vec4(aPos, 0.0, 1.0);
}"""

        private const val BG_FS = """#version 300 es
precision highp float;
in vec2 vUvYDown;
out vec4 oColor;
uniform vec3 uColorStart;
uniform vec3 uColorEnd;
uniform vec2 uDir;
uniform float uTMin;
uniform float uTSpan;
void main() {
  float t = dot(vUvYDown, uDir);
  float u = clamp((t - uTMin) / max(uTSpan, 1e-6), 0.0, 1.0);
  float g = u * u * (3.0 - 2.0 * u);
  // No gamma encode: pow(1/2.2) brightened but also washed the blue out to grey. The bottom
  // colour is boosted via linkev's own gradient multiplier instead (see BG_BOT), which keeps
  // the deep-blue saturation of the reference.
  oColor = vec4(mix(uColorStart, uColorEnd, g), 1.0);
}"""

        private const val WAVE_VS = """#version 300 es
precision highp float;
in vec2 aPos;
uniform sampler2D uSplineTex;
uniform float uTime;
uniform float flowSpeed;
uniform float tension;
uniform float damping;
uniform float length;
uniform float spacing;
uniform float perturbation;
uniform float perturbationScale;
uniform float timeStep;
uniform float waveCosAmp;
uniform float waveBias;
uniform float waveHeightScale;
uniform float waveSoftClip;
uniform float ffdYAmp;
uniform float ffdZAmp;
uniform float zDetailScale;
uniform vec3 ffdScale1;
uniform vec3 ffdScale2;
uniform vec3 ffdOffset;
out vec3 vPos;
void main() {
  vec3 p = vec3(aPos.x, 0.0, aPos.y);
  vec2 uv = (aPos + 1.0) * 0.5;
  p.y = texture(uSplineTex, uv).r;
  vec3 ffd1 = p * ffdScale1 + ffdOffset;
  vec3 ffd2 = p * ffdScale2 + ffdOffset;
  p.y += sin(ffd1.x + uTime * flowSpeed) * ffdYAmp;
  p.z += cos(ffd2.z + uTime * flowSpeed) * ffdZAmp;
  float baseWave = cos(p.x * 2.0 - uTime * 0.5 * timeStep) * waveCosAmp + waveBias;
  baseWave *= (1.0 - damping);
  baseWave += tension * sin(p.x * length + uTime * flowSpeed * timeStep * 0.25);
  float structured = perturbation * perturbationScale * (
    sin((p.x * length * 6.0 + p.z * 0.5) * spacing * 0.01 + uTime * flowSpeed * timeStep * 0.7) * 0.5 +
    sin((p.x * length * 10.0 - p.z * 0.8) * spacing * 0.005 - uTime * flowSpeed * timeStep * 0.35) * 0.25
  );
  float totalWave = (baseWave + structured) * waveHeightScale;
  totalWave = waveSoftClip * tanh(totalWave / max(waveSoftClip, 1e-4));
  p.y -= totalWave;
  vec2 uv2 = uv;
  uv2.x = fract(uv2.x - uTime * flowSpeed * 0.04 * timeStep);
  p.z -= texture(uSplineTex, uv2).r * zDetailScale;
  gl_Position = vec4(p, 1.0);
  vPos = p;
}"""

        private const val WAVE_FS = """#version 300 es
precision highp float;
in vec3 vPos;
out vec4 oColor;
uniform float opacity;
uniform float brightness;
uniform float fresnelPower;
uniform float fresnelScale;
void main() {
  vec3 dx = dFdx(vPos);
  vec3 dy = dFdy(vPos);
  vec3 N = normalize(cross(dx, dy));
  float F = fresnelScale * pow(1.0 + dot(vec3(0.0, 0.0, -1.0), N), fresnelPower);
  oColor = vec4(vec3(1.0), F * opacity * brightness);
}"""
    }
}

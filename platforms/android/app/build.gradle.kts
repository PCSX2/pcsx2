@file:Suppress("UnstableApiUsage", "DEPRECATION")
import org.gradle.api.GradleException
import java.util.Properties

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.compose.compiler)
}

val armsx2NativeLibName = providers.gradleProperty("armsx2.nativeLibName").orElse("emucore_4k")
val armsx2Pgo = providers.gradleProperty("armsx2.pgo").orElse("none") // none | generate | optimize
val armsx2PgoProfile = providers.gradleProperty("armsx2.pgoProfile").orElse("") // abs path to merged .profdata (optimize)
val armsx2HostPageSize = providers.gradleProperty("armsx2.hostPageSize").orElse("0x1000")
val armsx2ApplicationId = providers.gradleProperty("armsx2.applicationId").orElse("com.armsx2")
val armsx2SigningPropertiesFile = rootProject.file("armsx2_keystore.properties")
val armsx2SigningProperties = Properties().apply {
    if (armsx2SigningPropertiesFile.isFile) {
        armsx2SigningPropertiesFile.inputStream().use(::load)
    }
}
fun armsx2SigningProperty(name: String): String? = armsx2SigningProperties.getProperty(name)?.takeIf { it.isNotBlank() }
val armsx2PlaySigningReady = listOf("storeFile", "storePassword", "keyAlias", "keyPassword")
    .all { armsx2SigningProperty(it) != null }

if (armsx2SigningPropertiesFile.isFile && !armsx2PlaySigningReady) {
    throw GradleException("armsx2_keystore.properties is missing one or more required signing keys.")
}

// Runtime resources (GameDB, shaders, fonts, icons) have a single canonical copy
// at <repo>/bin/resources — the same tree the desktop/Linux build ships via
// pcsx2_copy_runtime_resources. Rather than commit a third drifting copy under
// src/main/assets, generate the APK's assets/resources from it at build time:
// sync bin/resources (minus the Windows-only dx11 shaders the mobile backends
// never compile) plus the canonical mobile GameDB overlay into a generated
// assets root. Genuine Android-only extras (patches.zip, the Noto color emoji
// font) stay committed under src/main/assets/resources and AGP merges the roots.
val generateSharedResources by tasks.registering(Sync::class) {
    val repoRoot = rootProject.layout.projectDirectory.dir("../..")
    from(repoRoot.dir("bin/resources")) {
        exclude("dx11/**")
    }
    from(repoRoot.file("bin/resources-overlay/armsx2_overrides.yaml"))
    into(layout.buildDirectory.dir("generated/sharedResources/resources"))
}

android {
    namespace = "com.armsx2"
    compileSdk = 37

    defaultConfig {
        applicationId = armsx2ApplicationId.get()
        minSdk = 26
        targetSdk = 37
        versionCode = providers.gradleProperty("armsx2.versionCode").orNull?.toInt() ?: 1088
        versionName = providers.gradleProperty("armsx2.versionName").orNull ?: "2.6.1"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk {
            abiFilters.add("arm64-v8a")
        }
    }

    signingConfigs {
        create("playRelease") {
            armsx2SigningProperty("storeFile")?.let { storeFile = rootProject.file(it) }
            storePassword = armsx2SigningProperty("storePassword")
            keyAlias = armsx2SigningProperty("keyAlias")
            keyPassword = armsx2SigningProperty("keyPassword")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            // Sign release with the debug keystore so it's installable on-device
            // without a separate signing config. NOT for distribution — the debug
            // keystore is well-known and not secure for Play Store uploads.
            // Replace with a real release signingConfig before publishing.
            signingConfig = if (armsx2PlaySigningReady) {
                signingConfigs.getByName("playRelease")
            } else {
                signingConfigs.getByName("debug")
            }
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            if (true) externalNativeBuild {
                cmake {
                    arguments += "-DANDROID=true"
                    arguments += "-DANDROID_STL=c++_static"
                    arguments += "-DCMAKE_BUILD_TYPE=Release"
                    // PGO (profile-guided optimization), opt-in via -Parmsx2.pgo:
                    //   generate -> instrumented build (writes .profraw on-device); LTO OFF
                    //              for a faster/cleaner instrument pass.
                    //   optimize -> consume the merged profile (-fprofile-use); LTO ON.
                    //   <unset>  -> normal release, LTO ON (unchanged).
                    val pgo = armsx2Pgo.get()
                    arguments += if (pgo == "generate") "-DLTO_PCSX2_CORE=OFF" else "-DLTO_PCSX2_CORE=ON"
                    arguments += "-DARMSX2_EMUCORE_LIBRARY_NAME=${armsx2NativeLibName.get()}"
                    arguments += "-DARMSX2_ANDROID_HOST_PAGE_SIZE=${armsx2HostPageSize.get()}"
                    arguments += "-DCMAKE_C_FLAGS=-O3 -g"
                    arguments += "-DCMAKE_CXX_FLAGS=-O3 -g"
                    if (pgo == "generate") arguments += "-DUSE_PGO_GENERATE=ON"
                    if (pgo == "optimize") {
                        arguments += "-DUSE_PGO_OPTIMIZE=ON"
                        val prof = armsx2PgoProfile.get()
                        if (prof.isNotBlank()) arguments += "-DARMSX2_PGO_PROFILE=$prof"
                    }
                }
            }
        }
        debug {
            // Keep PCSX2_DEBUG/VIXL_DEBUG defines (via CMAKE_BUILD_TYPE=Debug)
            // but compile at -O3 to match release's
            // codegen. -O0 was exposing a JIT-adjacent crash in MGS2 that
            // -O3 release didn't hit, which narrows the cause to stack/
            // uninitialised-local fragility rather than the debug defines.
            // -ffp-contract=off was previously kept for VU1 bit-exactness
            // but only affects C/C++ FP code, not JIT-emitted FMUL/FADD.
            // Removing it lets the compiler fuse a*b+c → FMADD in counters,
            // GS software renderer, SPU2 audio mixing, IPU, VIF unpack —
            // significant FP-heavy paths. JIT'd VU FMAC semantics are
            // unaffected because the recompiler emits explicit Fmul+Fadd.
            externalNativeBuild {
                cmake {
                    arguments += "-DANDROID=true"
                    arguments += "-DANDROID_STL=c++_static"
                    arguments += "-DCMAKE_BUILD_TYPE=Debug"
                    arguments += "-DARMSX2_EMUCORE_LIBRARY_NAME=${armsx2NativeLibName.get()}"
                    arguments += "-DARMSX2_ANDROID_HOST_PAGE_SIZE=${armsx2HostPageSize.get()}"
                    arguments += "-DCMAKE_C_FLAGS=-O3 -g"
                    arguments += "-DCMAKE_CXX_FLAGS=-O3 -g"
                }
            }
        }
    }
    // Distribution split: the Play AAB (play flavor) stays scoped-storage /
    // SAF only — src/main/AndroidManifest.xml has NO MANAGE_EXTERNAL_STORAGE,
    // so play is Play-policy clean by construction. The sideloaded GitHub APK
    // (github flavor) merges src/github/AndroidManifest.xml, which adds
    // MANAGE_EXTERNAL_STORAGE back, and STORAGE_ALL_FILES gates the runtime
    // all-files / custom-folder path in the setup wizard. applicationId is left
    // to defaultConfig (driven by -Parmsx2.applicationId) so both flavors honor
    // the release/AAB pipeline's CLI override.
    flavorDimensions += "store"
    productFlavors {
        create("github") {
            dimension = "store"
            buildConfigField("boolean", "STORAGE_ALL_FILES", "true")
        }
        create("play") {
            dimension = "store"
            buildConfigField("boolean", "STORAGE_ALL_FILES", "false")
        }
    }
    // Merge the generated bin/resources tree in as a second assets root. Passing
    // the task's output provider (not a bare path) makes AGP's asset-merge tasks
    // depend on generateSharedResources, so the tree is materialized before it is
    // packaged. destinationDir is .../resources; its parent is the assets root.
    sourceSets.named("main") {
        assets.srcDir(generateSharedResources.map { it.destinationDir.parentFile })
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    // NATIVE BUILD DISABLED for the recovered tree: the prebuilt native .so files
    // (extracted from vc1063 into src/main/jniLibs/arm64-v8a) are packaged directly,
    // so UI/Kotlin iteration doesn't require recompiling the C++ core. Re-enable this
    // block (and the per-buildType cmake blocks above) to rebuild native from source.
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    buildFeatures {
        // Generated BuildConfig.DEBUG used by Main.kt's debug-only auto-boot
        // path. AGP 8 made this opt-in.
        buildConfig = true
    }

    packaging {
        jniLibs {
            // libadrenotools requires `useLegacyPackaging = true` so the
            // hook .so files (hook_impl, main_hook, file_redirect_hook,
            // gsl_alloc_hook) get extracted to ApplicationInfo.nativeLibraryDir
            // at install time. Without this, AGP leaves them inside the apk
            // and adrenotools' linker-namespace bypass can't find them by
            // path — the custom Vulkan driver load silently falls back to
            // the system loader.
            useLegacyPackaging = true
        }
    }
}

composeCompiler {
    // Keep R8 enabled while avoiding AGP's incompatible built-in Kotlin
    // compose-group-mapping producer. Source/line mappings remain preserved.
    includeComposeMappingFile.set(false)
}

// Android Studio's "Build > Clean Project" runs the `clean` task, but AGP
// leaves `app/.cxx/` (the CMake/Ninja workspace) in place. Stale .cxx state
// can lead to ghost builds — old object files linking against newer headers,
// or vice versa. Wire `cleanCxx` into `clean` so the native build workspace
// gets wiped too.
tasks.register<Delete>("cleanCxx") {
    delete(layout.projectDirectory.dir(".cxx"))
}
tasks.named("clean") {
    dependsOn("cleanCxx")
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)

    //AndroidX Compose
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.material3)
    implementation(libs.composeIcons.fontAwesome)
    implementation(libs.composeIcons.lineAwesome)

    implementation(libs.kotlin.reflect)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.compose.foundation)
    implementation(libs.androidx.documentfile)
    implementation(libs.coil.compose)
    implementation(libs.coil.gif) // animated GIF / WebP / APNG (library background)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    debugImplementation(libs.androidx.compose.ui.tooling)
}

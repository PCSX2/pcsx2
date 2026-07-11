# Preserve useful crash traces and metadata used by Android/Compose tooling.
-keepattributes SourceFile,LineNumberTable,Signature,InnerClasses,EnclosingMethod
-keepattributes RuntimeVisibleAnnotations,RuntimeInvisibleAnnotations,AnnotationDefault

# JNI entry points are resolved from native code by their exact class and member
# names (FindClass / GetStaticMethodID on literal strings). R8 can't see those
# native references, so every class the native side looks up by name must be kept
# un-renamed — this whole package is the JNI bridge layer (NativeApp, HttpClient +
# its nested Response). Keeping only NativeApp let R8 rename HttpClient, which broke
# the native HTTP downloader (RetroAchievements login / badge fetch → hard crash).
-keep class kr.co.iefriends.pcsx2.** { *; }
-keep class com.armsx2.BiosInfo { *; }
-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}

# Application components instantiated by the Android framework.
-keep class com.armsx2.Pasx2Application { *; }
-keep class com.armsx2.BootSplashActivity { *; }
-keep class com.armsx2.Main { *; }
-keep class com.armsx2.RetroAchievementsHostOverrideReceiver { *; }

# SDL resolves its Java bridge classes and callbacks through JNI/reflection.
-keep class org.libsdl.app.** { *; }

# ReLinker is reached reflectively by SDL on devices that need its fallback loader.
-keep class com.getkeepsafe.relinker.** { *; }
-dontwarn com.getkeepsafe.relinker.**

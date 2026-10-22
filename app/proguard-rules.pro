# VeloSphere ProGuard / R8 Configuration Rules
# Keep GameActivity and JNI entry points intact during release minification

-keep class com.google.androidgamesdk.GameActivity { *; }
-keep class com.hsissa.velosphere.MainActivity {
    public static void playHitSound();
    public static void playSfx(int);
    public static void vibrate(int, int);
    public static void updateSettings(boolean, boolean, boolean);
}

-keepclasseswithmembernames class * {
    native <methods>;
}

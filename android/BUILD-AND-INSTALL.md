# Build and install OpenTail native Android v0.2.0-alpha1

The official debug APK is built by the GitHub Actions `android-debug-apk`
job on this PR. Locally, with Java 17 and Android SDK 35 installed, run:

```powershell
cd android; .\gradlew.bat --no-daemon :app:assembleDebug
```

Expected output: `android/app/build/outputs/apk/debug/app-debug.apk`.
The application ID remains `com.opentail.android`; versionCode is 31.

**Export from the previous WebView build before updating:** in Android
v0.1.30 open Settings → Project backup → Export JSON. Save the file outside
the app. Then install v0.2.0-alpha1 and Settings → Import JSON. The native
app cannot automatically read old WebView localStorage.

This Android release does not require an OpenTail CORE firmware flash.

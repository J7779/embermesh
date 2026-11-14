import java.io.File
import java.security.KeyStore

plugins {
    id("com.android.application")
}

// Repository root (apps/android/app -> ../../..). Overridable with -PemberRoot=/path.
val emberRoot: File = (findProperty("emberRoot") as String?)?.let { file(it) } ?: rootProject.projectDir.resolve("../..").canonicalFile

// Core modules to build: "all" (default), or a ;-list such as capi for a faster partial build.
val emberModules: String = (findProperty("emberModules") as String?) ?: "all"

// Embed the host library's hub (host/) for host.hub.* and the ws_server endpoint: -PemberHub=false to drop it.
val emberHub: Boolean = (findProperty("emberHub") as String?)?.toBoolean() ?: true

// Version from the top-level CMake project() so the app and the core always agree.
val emberVersion: String = run {
    val cm = emberRoot.resolve("CMakeLists.txt")
    val m = if (cm.exists()) Regex("""project\(\s*EmberMesh\s+VERSION\s+([0-9.]+)""").find(cm.readText()) else null
    m?.groupValues?.get(1) ?: "0.1.0"
}
val emberVersionCode: Int = emberVersion.split(".").map { it.toIntOrNull() ?: 0 }
    .let { (it.getOrElse(0) { 0 } * 10000) + (it.getOrElse(1) { 0 } * 100) + it.getOrElse(2) { 0 } }
    .coerceAtLeast(1)

// The shared web UI and the guide library are copied into the APK assets at build time.
val uiSourceDir: File = (findProperty("emberUiDir") as String?)?.let { file(it) } ?: emberRoot.resolve("ui")
val guidesPackFile: File? = ((findProperty("emberGuidesPack") as String?)?.let { listOf(file(it)) }
    ?: listOf(
        emberRoot.resolve("build/guides.pack"),
        emberRoot.resolve("dist/guides.pack"),
        emberRoot.resolve("guides/guides.pack"),
        emberRoot.resolve("build/guides/guides.pack"),
    )).firstOrNull { it.isFile }
// The web build (WASM core + UI) served to guests by the phone hub; absent -> the hub serves assets/ui.
val webDistDir: File = (findProperty("emberWebDir") as String?)?.let { file(it) } ?: emberRoot.resolve("dist/web")

// --- Release signing (optional) -------------------------------------------------------------------
// The release build is signed with the owner's key when ALL four settings are present, read from the
// environment first and from Gradle properties (-P / gradle.properties outside the repo) second:
//   EMBER_ANDROID_KEYSTORE           path to a PKCS12 or JKS keystore (kept outside the repository)
//   EMBER_ANDROID_KEYSTORE_PASSWORD  EMBER_ANDROID_KEY_ALIAS  EMBER_ANDROID_KEY_PASSWORD
// Otherwise it is signed with the builder's debug key, as before. Settings that are present but wrong
// (no such file, wrong password, unknown alias) fail the build; they never fall back to the debug key.
val releaseSigningNames = listOf(
    "EMBER_ANDROID_KEYSTORE", "EMBER_ANDROID_KEYSTORE_PASSWORD", "EMBER_ANDROID_KEY_ALIAS", "EMBER_ANDROID_KEY_PASSWORD",
)
val releaseSigning: Map<String, String> = releaseSigningNames.mapNotNull { n ->
    (System.getenv(n)?.takeIf { it.isNotEmpty() } ?: (findProperty(n) as String?)?.takeIf { it.isNotEmpty() })?.let { n to it }
}.toMap()
val releaseKeystore: File? = if (releaseSigning.size == releaseSigningNames.size) file(releaseSigning.getValue("EMBER_ANDROID_KEYSTORE")) else null
val releaseKeystoreType: String? = releaseKeystore?.let { ks ->
    if (!ks.isFile) throw GradleException("EMBER_ANDROID_KEYSTORE: keystore file ${ks.name} does not exist")
    val storePassword = releaseSigning.getValue("EMBER_ANDROID_KEYSTORE_PASSWORD").toCharArray()
    val alias = releaseSigning.getValue("EMBER_ANDROID_KEY_ALIAS")
    val keyPassword = releaseSigning.getValue("EMBER_ANDROID_KEY_PASSWORD").toCharArray()
    var problem = "unreadable keystore"
    for (type in listOf("PKCS12", "JKS")) {
        val store = KeyStore.getInstance(type)
        try {
            ks.inputStream().use { store.load(it, storePassword) }
        } catch (e: Exception) {
            problem = "cannot open it as a PKCS12 or JKS keystore (wrong EMBER_ANDROID_KEYSTORE_PASSWORD?)"; continue
        }
        if (!store.isKeyEntry(alias)) throw GradleException("EMBER_ANDROID_KEY_ALIAS: ${ks.name} has no key named '$alias'")
        try {
            store.getKey(alias, keyPassword)
        } catch (e: Exception) {
            throw GradleException("EMBER_ANDROID_KEY_PASSWORD: cannot unlock key '$alias' in ${ks.name} (wrong password?)")
        }
        return@let type
    }
    throw GradleException("EMBER_ANDROID_KEYSTORE: ${ks.name}: $problem")
}
println(
    if (releaseKeystore != null) "EmberMesh: release builds are signed with key '${releaseSigning.getValue("EMBER_ANDROID_KEY_ALIAS")}' from ${releaseKeystore.name} ($releaseKeystoreType)"
    else if (releaseSigning.isEmpty()) "EmberMesh: release builds are signed with this machine's debug key (EMBER_ANDROID_* not set)"
    else "EmberMesh: release builds are signed with this machine's debug key (incomplete release signing settings, missing: ${(releaseSigningNames - releaseSigning.keys).joinToString()})"
)

android {
    namespace = "org.embermesh.app"
    compileSdk = 37
    ndkVersion = "27.1.12297006"

    defaultConfig {
        applicationId = "org.embermesh.app"
        minSdk = 21
        targetSdk = 37
        versionCode = emberVersionCode
        versionName = emberVersion
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DEMBER_ROOT=${emberRoot.absolutePath}",
                    "-DEMBER_MODULES=$emberModules",
                    "-DEMBER_VERSION=$emberVersion",
                    "-DEMBER_ANDROID_HUB=${if (emberHub) "ON" else "OFF"}",
                    "-DANDROID_STL=c++_static",
                )
                // No absolute build-machine paths in the native libraries (file names in asserts and
                // debug info): the repository maps to ".", anything else under the builder's home
                // (Android SDK/NDK) to "~".
                val prefixMaps = listOf(
                    "-ffile-prefix-map=${emberRoot.absolutePath}=.",
                    "-ffile-prefix-map=${System.getProperty("user.home")}=~",
                )
                cppFlags += listOf("-std=c++17", "-ffunction-sections", "-fdata-sections") + prefixMaps
                cFlags += listOf("-ffunction-sections", "-fdata-sections") + prefixMaps
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    signingConfigs {
        if (releaseKeystore != null) {
            create("release") {
                storeFile = releaseKeystore
                storePassword = releaseSigning.getValue("EMBER_ANDROID_KEYSTORE_PASSWORD")
                keyAlias = releaseSigning.getValue("EMBER_ANDROID_KEY_ALIAS")
                keyPassword = releaseSigning.getValue("EMBER_ANDROID_KEY_PASSWORD")
                storeType = releaseKeystoreType!!.lowercase()
                enableV2Signing = true
                enableV3Signing = true
            }
        }
    }

    buildTypes {
        debug {
            isMinifyEnabled = false
        }
        release {
            vcsInfo { include = false } // no VCS state embedded in the APK
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            // The owner's release key when EMBER_ANDROID_* are set (see above and README "Signing");
            // otherwise the BUILDER'S OWN debug key (~/.android/debug.keystore on the building machine;
            // it identifies that machine). No keystore is part of this repository.
            signingConfig = signingConfigs.getByName(if (releaseKeystore != null) "release" else "debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    androidResources {
        // guides.pack is memory-mapped straight out of the APK (ember_guides_load copy=0).
        noCompress += listOf("pack")
    }

    packaging {
        jniLibs {
            // Compressed .so (extracted at install): ~4 MB smaller APK, which matters because the APK
            // is passed phone to phone over Bluetooth. guides.pack stays uncompressed and memory-mapped.
            useLegacyPackaging = true
        }
    }

    testOptions {
        unitTests.isReturnDefaultValues = true
    }

    lint {
        abortOnError = false
        checkReleaseBuilds = false
    }
}

// --- bundled assets: ui/ and guides.pack ----------------------------------------------------------
abstract class EmberAssetsTask : DefaultTask() {
    @get:InputFiles @get:Optional @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val uiDir: DirectoryProperty

    @get:InputFile @get:Optional @get:PathSensitive(PathSensitivity.NONE)
    abstract val guidesPack: RegularFileProperty

    @get:InputFiles @get:Optional @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val webDir: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun run() {
        val out = outputDir.get().asFile
        out.deleteRecursively()
        out.mkdirs()
        val ui = uiDir.orNull?.asFile
        if (ui != null && ui.isDirectory) {
            ui.copyRecursively(out.resolve("ui"), overwrite = true)
            // Drop editor/OS junk and test-only files that should not ship.
            out.resolve("ui/tests").deleteRecursively()
            out.resolve("ui").walkBottomUp().filter { it.name == ".DS_Store" }.forEach { it.delete() }
        }
        val pack = guidesPack.orNull?.asFile
        if (pack != null && pack.isFile) pack.copyTo(out.resolve("guides.pack"), overwrite = true)
        // dist/web for the hub, without what the APK already has or the hub does not serve: its copy
        // of guides.pack (the hub serves assets/guides.pack as data/guides.pack) and the 5 MB
        // single-file build.
        val web = webDir.orNull?.asFile
        if (web != null && web.resolve("index.html").isFile && web.resolve("wasm").isDirectory) {
            web.copyRecursively(out.resolve("web"), overwrite = true)
            out.resolve("web/data/guides.pack").delete()
            out.resolve("web/embermesh.html").delete()
            // The web build contains the same UI files as assets/ui: keep only what differs (WASM, its
            // index.html, PWA files). The hub rebuilds the full web root as ui/ + web/ on extraction.
            val webOut = out.resolve("web")
            webOut.walkTopDown().filter { it.isFile }.forEach { f ->
                val twin = out.resolve("ui").resolve(f.relativeTo(webOut).path)
                if (twin.isFile && twin.length() == f.length() && twin.readBytes().contentEquals(f.readBytes())) f.delete()
            }
            out.resolve("web").walkBottomUp().filter { it.name == ".DS_Store" || (it.isDirectory && it.list()?.isEmpty() == true) }.forEach { it.delete() }
        }
    }
}

val emberAssets = tasks.register<EmberAssetsTask>("emberAssets") {
    uiDir.set(uiSourceDir)
    guidesPack.set(guidesPackFile)
    if (webDistDir.isDirectory) webDir.set(webDistDir)
    outputDir.set(layout.buildDirectory.dir("generated/emberAssets"))
}

androidComponents {
    onVariants { variant ->
        variant.sources.assets?.addGeneratedSourceDirectory(emberAssets, EmberAssetsTask::outputDir)
    }
}

dependencies {
    // ZXing core (Apache-2.0), vendored so the app can be rebuilt offline. See libs/ZXING-LICENSE.txt.
    implementation(files("libs/zxing-core-3.5.3.jar"))

    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20180813")

    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test:runner:1.6.2")
    androidTestImplementation("androidx.test:core:1.6.1")
}

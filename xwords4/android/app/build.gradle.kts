/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2015 - 2024 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

import java.io.ByteArrayOutputStream

// These two change with every release.
val VERSION_CODE_BASE = 201
val VERSION_NAME = "4.4.205"

val INITIAL_CLIENT_VERS = 10
val BUILD_INFO_NAME = "build-info.txt"
val LAST_COMMIT_FILE = "last-commit.txt"


// Not all variants use the same BT_UUID. Those with the same talk to
// each other
val XW_UUID = "\"7be0d084-ff89-4d6d-9c78-594773a6f963\"" // from comms.h
val XWD_UUID = "\"b079b640-35fe-11e5-a432-0002a5d5c51b\"" // from comms.h

// AID must start with F (first 4 bits) and be from 5 to 16 bytes long
val NFC_AID_XW4 = "FC8FF510B360"
val NFC_AID_XW4d = "FDDA0A3EB5E5"

fun String.runString(): String {
	var bytesOut = ByteArrayOutputStream()
	val errCode = project.exec {
		commandLine = this@runString.split(" ")
		// println("commandLine: " + commandLine)
		standardOutput = bytesOut
		isIgnoreExitValue = true
	}
	var result: String = ""
	if ( errCode.exitValue == 0 ) {
		result = String(bytesOut.toByteArray()).trim()
	}
	return result;
}

// Get the git revision we're using. Since fdroid modifies files as
// part of its build process -dirty will always be added, confusing
// users. So add that for the non-fdroid case.
if (! project.hasProperty("GITREV")) {
	extra.apply{
        set("GITREV", "git describe --tags --dirty".runString())
	}
}
var GITREV = extra["GITREV"]

val GITREV_SHORT = "git rev-parse --short HEAD".runString()

// Make CURTAG non-empty IFF we're at a tag (release build)
val CURTAG = "git describe --exact-match".runString()
// print "CURTAG: " + CURTAG + "\n"

plugins {
	id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

repositories {
    google()
    mavenCentral()
}

android {
	// It'll compile without specifying these. So let's try going with
	// the defaults for now
     compileOptions {
         sourceCompatibility = JavaVersion.VERSION_1_8
         targetCompatibility = JavaVersion.VERSION_1_8
	 }

     packagingOptions {
         resources {
             excludes += listOf("META-INF/INDEX.LIST", "META-INF/io.netty.versions.properties")

             excludes += listOf("/META-INF/{AL2.0,LGPL2.1}")
             excludes += listOf("META-INF/LICENSE.md")
             excludes += listOf("META-INF/LICENSE-notice.md")
             excludes += listOf("META-INF/INDEX.LIST")
             excludes += listOf("META-INF/*.properties")
         }
     }

    // Specify buildToolsVersion so gradle will inform when the
    // default changes and .travis.yml can be kept in sync
    ndkVersion  = "21.4.7075529" // upgrade this and may need to up minSdk below
    defaultConfig {
        minSdk = 21			// this will abandon some users; consider
        versionCode = VERSION_CODE_BASE
        versionName = VERSION_NAME
    }

    // Rename all output artifacts to include version information
    applicationVariants.all {

        // We need both of these because xwprefs.xml can't reference
        // the BuildConfig constant
        this.buildConfigField( "String", "GIT_REV", "\"${extra["GITREV"]}\"")
        this.buildConfigField( "String", "GITREV_SHORT", "\"${GITREV_SHORT}\"")

        val stamp = System.currentTimeMillis() / 1000;
        this.buildConfigField( "long", "BUILD_STAMP", "${stamp}" )
    }

    /* Variants:
     *
     * GPlay: for the Google Play store, include FCM but not SMS-data.
     *
     * FDroid: for the f-droid store, includes SMS-data but not FCM.
     *
     * Foss: released via SourceForge and Github, and auto-upgraded
     * within the app. Includes both SMS-data and FCM
     */
    
    flavorDimensions += "variant"//, "abi"
    productFlavors {
        all {
            android {
                targetSdk = 34
                compileSdk = 34
            }
            buildConfigField( "String", "JNI_LIB_NAME", "\"xwjni\"" )
            buildConfigField(  "String", "DB_NAME", "\"xwdb\"" )
            buildConfigField(  "String", "BUILD_INFO_NAME", "\"${BUILD_INFO_NAME}\"" )
            buildConfigField(  "boolean", "IS_TAGGED_BUILD", if ( "${CURTAG}" == "" ) { "false" } else { "true" } )
            resValue( "string", "invite_prefix", "/and/" )
            resValue( "string", "conf_prefix", "/cnf/" )
			resValue( "string", "newgame_scheme", "newxwgame" )
            buildConfigField( "boolean", "UDP_ENABLED", "true" )
            buildConfigField( "boolean", "REPORT_LOCKS", "false" )
            buildConfigField( "boolean", "LOG_LIFECYLE", "false" )
            buildConfigField( "boolean", "ATTACH_SUPPORTED", "false" )
            buildConfigField( "boolean", "NON_RELEASE", "DEBUG || !IS_TAGGED_BUILD" )
            buildConfigField( "boolean", "HAVE_KNOWN_PLAYERS", "true" )
            buildConfigField( "boolean", "FOR_FDROID", "false" )
            buildConfigField( "boolean", "HAVE_PASSWORD", "false" )
            buildConfigField( "boolean", "NO_NEW_RELAY", "true" )
            buildConfigField( "boolean", "LOCUTILS_ENABLED", "false" )
            buildConfigField( "String", "LAST_COMMIT_FILE", "\"$LAST_COMMIT_FILE\"" )
            buildConfigField( "int", "BAD_COUNT", "2" )
            resValue( "string", "nbs_port", "0" )
            resValue( "string", "dflt_log_prune_hours", "24" )
        }

        create("xw4GPlay") {
            dimension = "variant"
            applicationId = "org.eehouse.android.xw4GPTest"
			manifestPlaceholders["APP_ID"] = "org.eehouse.android.xw4"
            resValue( "string", "app_name", "DoNotRelease" )
            buildConfigField( "boolean", "NON_RELEASE", "true" )
            buildConfigField( "boolean", "WIDIR_ENABLED", "false" )
            buildConfigField( "String", "VARIANT_NAME", "\"Google Play Store\"" )
            buildConfigField( "int", "VARIANT_CODE", "1" )
            buildConfigField( "String", "NFC_AID", "\"${NFC_AID_XW4}\"" )
            resValue( "string", "nfc_aid", "$NFC_AID_XW4" )
            externalNativeBuild.ndkBuild.cFlags += arrayOf("-DVARIANT_xw4GPlay")
            externalNativeBuild.ndkBuild.arguments += arrayOf("XW_BT_UUID=" + XW_UUID)
        }

        create("xw4fdroid") {
            dimension = "variant"
            applicationId = "org.eehouse.android.xw4"
			manifestPlaceholders["APP_ID"] = applicationId.toString()
            resValue( "string", "app_name", "CrossWords" )
            resValue( "string", "nbs_port", "3344" )
            buildConfigField( "boolean", "WIDIR_ENABLED", "false" )
            buildConfigField( "String", "VARIANT_NAME", "\"F-Droid\"" )
            buildConfigField( "int", "VARIANT_CODE", "2" )
            buildConfigField( "boolean", "FOR_FDROID", "true" )
            buildConfigField( "String", "NFC_AID", "\"${NFC_AID_XW4}\"" )
            resValue( "string", "nfc_aid", "$NFC_AID_XW4" )
            externalNativeBuild.ndkBuild.cFlags += arrayOf("-DVARIANT_xw4fdroid")
            externalNativeBuild.ndkBuild.arguments += arrayOf("XW_BT_UUID=" + XW_UUID)
        }

		create("xw4d") {
            dimension = "variant"

            buildConfigField( "String", "DB_NAME", "\"xwddb\"" )
            applicationId = "org.eehouse.android.xw4dbg"
            resValue( "string", "app_name", "CrossDeb" )
            resValue( "string", "nbs_port", "3345" )
            resValue( "string", "invite_prefix", "/andd/" )
			resValue( "string", "conf_prefix", "/cnfd/" )
			resValue( "string", "newgame_scheme", "newxwgamed" )
            buildConfigField( "boolean", "WIDIR_ENABLED", "true" )
            buildConfigField( "String", "VARIANT_NAME", "\"Dev/Debug\"" )
            buildConfigField( "int", "VARIANT_CODE", "3" )
            buildConfigField( "boolean", "REPORT_LOCKS", "true" )
            buildConfigField( "String", "NFC_AID", "\"${NFC_AID_XW4d}\"" )
            resValue( "string", "nfc_aid", "$NFC_AID_XW4d" )
            externalNativeBuild.ndkBuild.cFlags += arrayOf("-DVARIANT_xw4d")
            externalNativeBuild.ndkBuild.arguments += arrayOf("XW_BT_UUID=" + XWD_UUID)
        }

        create("xw4dGPlay") {
            dimension = "variant"
            applicationId = "org.eehouse.android.xw4dbg"
            buildConfigField( "String", "DB_NAME", "\"xwddb\"" )
            resValue( "string", "app_name", "CrossDeb" )
            resValue( "string", "invite_prefix", "/andd/" )
            resValue( "string", "conf_prefix", "/cnfd/" )
			resValue( "string", "newgame_scheme", "newxwgamed" )
            buildConfigField( "boolean", "WIDIR_ENABLED", "true" )
            buildConfigField( "String", "VARIANT_NAME", "\"Dev/Debug GPlay\"" )
            buildConfigField( "int", "VARIANT_CODE", "4" )
            buildConfigField( "boolean", "REPORT_LOCKS", "true" )
            buildConfigField( "String", "NFC_AID", "\"${NFC_AID_XW4d}\"" )
            resValue(  "string", "nfc_aid", "$NFC_AID_XW4d" )
            externalNativeBuild.ndkBuild.cFlags += arrayOf("-DVARIANT_xw4dGPlay")
            externalNativeBuild.ndkBuild.arguments += arrayOf("XW_BT_UUID=" + XWD_UUID)
        }

        create("xw4Foss") {
            dimension = "variant"
            applicationId = "org.eehouse.android.xw4"
			manifestPlaceholders["APP_ID"] = applicationId.toString()
            resValue( "string", "app_name", "CrossWords" )
            resValue( "string", "nbs_port", "3344" )
            buildConfigField( "boolean", "WIDIR_ENABLED", "false" )
            buildConfigField( "String", "VARIANT_NAME", "\"FOSS\"" )
            buildConfigField( "int", "VARIANT_CODE", "5" )
            buildConfigField( "String", "NFC_AID", "\"${NFC_AID_XW4}\"" )
            resValue( "string", "nfc_aid", "$NFC_AID_XW4" )
            externalNativeBuild.ndkBuild.cFlags += arrayOf("-DVARIANT_xw4Foss")
            externalNativeBuild.ndkBuild.arguments += arrayOf("XW_BT_UUID=" + XW_UUID)
        }

        // WARNING: "all" breaks things. Seems to be a keyword. Need
        // to figure out how to express include-all-abis
        // all {
        //     dimension "abi"
        //     versionCode 0 + VERSION_CODE_BASE
        // }
        // armeabi {
        //     dimension "abi"
        //     versionCode 1 + VERSION_CODE_BASE
        // }
        // x86 {
        //     dimension "abi"
        //     versionCode 2 + VERSION_CODE_BASE
        // }
        // armeabiv7a {
        //     dimension "abi"
        //     versionCode 3 + VERSION_CODE_BASE
        // }
        all {
            buildConfigField( "String", "compileSdkVersion", "\"$compileSdkVersion\"" )
        }
    }

    signingConfigs {
        getByName("debug") {
            var path = System.getenv("DEBUG_KEYSTORE_PATH")
            if (null == path || path.equals("")) {
                path = "./debug.keystore"
            }
            storeFile = file(path)
            keyAlias = "androiddebugkey"
            storePassword = "android"
            keyPassword = "android"
        }
    }

    buildTypes {
        all {
            externalNativeBuild {
                ndkBuild {
                    arguments += arrayOf("INITIAL_CLIENT_VERS=" + INITIAL_CLIENT_VERS)
                    arguments += arrayOf("GITREV=" + GITREV)
                }
            }
        }

        release {
            isDebuggable = false
            isMinifyEnabled = false // PENDING
            // proguard crashes when I do this (the optimize part)
            // proguardFiles getDefaultProguardFile('proguard-android-optimize.txt'), 'proguard-rules.pro'
            resValue( "bool", "DEBUG", "false" )
            externalNativeBuild {
                ndkBuild {
                    arguments += arrayOf("BUILD_TARGET=release")
                }
            }
            ndk {
				abiFilters.addAll(arrayOf("armeabi-v7a", "arm64-v8a"))
            }
        }
        debug {
            isDebuggable = true
            resValue( "bool", "DEBUG", "true" )
            // Drop this. Takes too long to build
            // minifyEnabled true // for testing
            proguardFiles( getDefaultProguardFile("proguard-android.txt"), "proguard-rules.pro")
            // This doesn't work on marshmallow: duplicate permission error
            // applicationIdSuffix ".debug"

            externalNativeBuild {
                ndkBuild {
                    cFlags += arrayOf("-DDEBUG")
                    arguments += arrayOf("BUILD_TARGET=debug")
                }
            }

            ndk {
				abiFilters.addAll(arrayOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64" ))
            }
        }
    }

    externalNativeBuild {
        ndkBuild {
            path("../jni/Android.mk")
        }
    }

	// April 2024: I can't port this to Kotlin, and am not quite sure
	// what it was doing. Since my current build process doesn't seem
	// to need it I'll punt for now.
    // sourceSets {
    //     // Use symlinks instead of setting non-conventional
    //     // directories here. AS doesn't respect what's set here: it'll
    //     // compile, but post-install app launch and source-level
    //     // debugging don't work.
    //     getByName("xw4GPlay") {
    //         release {
    //             jniLibs.srcDir = "../libs-xw4GPlayRelease"
    //         }
    //         debug {
    //             jniLibs.srcDir = "../libs-xw4GPlayDebug"
    //         }
    //     }
    //     xw4d {
    //         release {
    //             jniLibs.srcDir = "../libs-xw4dRelease"
    //         }
    //         debug {
    //             jniLibs.srcDir = "../libs-xw4dDebug"
    //         }
    //     }
    //     xw4dGPlay {
    //         release {
    //             jniLibs.srcDir = "../libs-xw4dGPlayRelease"
    //         }
    //         debug {
    //             jniLibs.srcDir = "../libs-xw4dGPlayDebug"
    //         }
    //     }
    //     xw4Foss {
    //         release {
    //             jniLibs.srcDir = "../libs-xw4FossRelease"
    //         }
    //         debug {
    //             jniLibs.srcDir = "../libs-xw4FossDebug"
    //         }
    //     }
    //     xw4fdroid {
    //         release {
    //             jniLibs.srcDir = "../libs-xw4fdroidRelease"
    //         }
    //         debug {
    //             jniLibs.srcDir = "../libs-xw4fdroidDebug"
    //         }
    //     }
    // }

    namespace = "org.eehouse.android.xw4"

	this.buildOutputs.all {
        val variantOutputImpl = this as com.android.build.gradle.internal.api.BaseVariantOutputImpl
        var variantName: String = variantOutputImpl.outputFileName
			.replace(".apk", "-${GITREV}.apk")
			.replace("app-", "")
        variantOutputImpl.outputFileName = variantName
    }

    buildFeatures {
        buildConfig = true
    }
    lint {
        abortOnError = false
    }
}

dependencies {
    implementation( "androidx.legacy:legacy-support-v4:1.0.+" )
	implementation( "androidx.preference:preference:1.2.+" )

    implementation( "androidx.lifecycle:lifecycle-extensions:2.0.+" )
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.0.0")
    annotationProcessor( "androidx.lifecycle:lifecycle-compiler:2.0.+" )

    implementation("com.hivemq:hivemq-mqtt-client:1.3.7")

    implementation( "com.google.zxing:core:3.3.+" )
    implementation( "com.jakewharton:process-phoenix:2.1.2" )

    implementation( "com.android.support:appcompat-v7:28.0.0" )
    implementation( "com.android.support:recyclerview-v7:28.0.0" )
}

tasks.create<Exec>("mkImages") {
    workingDir("../")
    commandLine("./scripts/mkimages.sh")
}

tasks.create<Exec>("copyLocStrings") {
    workingDir("../")
    commandLine( "make", "-f", "./scripts/copy-strings.mk" )
}

tasks.create<Exec>("mkPrefsWrapper") {
    workingDir( "../" )
    commandLine( "make", "-f", "./scripts/prefsWrapper.mk" )
}

tasks.create<Exec>( "cleanPrefsWrapper") {
    workingDir( "../")
	commandLine( "make", "-f", "./scripts/prefsWrapper.mk", "clean")
}

tasks.create<Exec>("cleanLocStrings") {
    workingDir( "../" )
    commandLine( "make", "-f", "./scripts/copy-strings.mk", "clean" )
}

tasks.named("clean") {
    dependsOn("cleanLocStrings", "cleanPrefsWrapper")
}

tasks.create("myPreBuild") {
	dependsOn( "makeBuildAssets",
			   "mkImages",
			   "copyLocStrings",
			   "mkPrefsWrapper",
			   "copyStringsXw4D",
			   "copyStringsXw4DGPlay"
	)
}

tasks.named("preBuild") {
	dependsOn( "myPreBuild" )
}

tasks.create<Exec>("copyStringsXw4D") {
    workingDir( "./" )
    environment.put("APPNAME", "CrossDeb")
    commandLine( "make", "-f", "../scripts/Variant.mk", "src/xw4d/res/values/strings.xml", "src/xw4d/res/values/tmpstrings.xml" )
}

tasks.create<Exec>("copyStringsXw4DGPlay" ) {
    workingDir( "./" )
    environment.put("APPNAME", "CrossDeb")
    commandLine( "make", "-f", "../scripts/Variant.mk", "src/xw4dGPlay/res/values/strings.xml", "src/xw4dGPlay/res/values/tmpstrings.xml" )
}

tasks.create( "makeBuildAssets" ) {
    val srcDirs = android.sourceSets["main"].assets.srcDirs
    val assetsDir: File = srcDirs.iterator().next()

	var out = "git_describe: ${GITREV}"

    out += "\nHEAD: " + "git rev-parse HEAD".runString()

	out += "\ndate: " + "date".runString()

    // I want the variant, but that's harder. Here's a quick hack from SO.
	val taskNames = gradle.startParameter.taskNames
	if ( 0 < taskNames.size ) {
		out += "\ntarget: " + taskNames[taskNames.size-1]
	}

    val diff = "git diff".runString()
    if (diff != "") {
        out += "\n" + diff
    }

	File(assetsDir, BUILD_INFO_NAME).writeText(out + "\n")

    // Now the latest commit
	File(assetsDir, LAST_COMMIT_FILE).writeText("git log -n 1".runString())
}

tasks.create("testClasses") {}

// must match JavaVersion.VERSION_1_8 above
tasks.withType<org.jetbrains.kotlin.gradle.tasks.KotlinCompile> {
    kotlinOptions.jvmTarget = "1.8"
}

// To turn on javac options
// tasks.withType<JavaCompile> {
//     options.compilerArgs.add("-Xdiags:verbose")
// }

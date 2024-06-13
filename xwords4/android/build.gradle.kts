// Top-level build file where you can add configuration options common to all sub-projects/modules.

buildscript {
    val kotlin_version by extra("2.0.0-Beta5")
    repositories {
        google()
        mavenCentral()
    }
    dependencies {
        classpath ("com.android.tools.build:gradle:8.5.0")
        classpath ( "com.google.gms:google-services:4.3.10" )
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:$kotlin_version")
        // classpath ( "com.google.firebase:firebase-crashlytics-gradle:2.5.2" ) // rm-for-fdroid
        // NOTE: Do not place your application dependencies here; they belong
        // in the individual module build.gradle files
    }
}

allprojects {
    repositories {
        google()
        mavenCentral()
        maven { url = uri("https://jitpack.io") }
    }
}

tasks.create<Delete>("clean") {
    delete(rootProject.buildDir)
}

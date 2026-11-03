import org.jetbrains.compose.desktop.application.dsl.TargetFormat

plugins {
    kotlin("jvm") version "2.1.20"
    kotlin("plugin.serialization") version "2.1.20"
    id("org.jetbrains.compose") version "1.8.0"
    id("org.jetbrains.kotlin.plugin.compose") version "2.1.20"
}

group = "com.nesstation"
version = "1.0.0"

repositories {
    mavenCentral()
    google()
    maven("https://maven.pkg.jetbrains.space/public/p/compose/dev")
}

dependencies {
    // Compose Desktop（含 Swing/AWT 互操作与 Skia 渲染）
    implementation(compose.desktop.currentOs)
    implementation(compose.material3)
    implementation(compose.materialIconsExtended)

    // libretro 核心动态加载（fceumm/snes9x/mGBA/... PC 核心的统一宿主）
    implementation("net.java.dev.jna:jna:5.16.0")

    // JSON 持久化（设置 / 游戏库 / 键位映射 / 核心选项）
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.8.1")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-swing:1.10.1")
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_21)
    }
}

java {
    sourceCompatibility = JavaVersion.VERSION_21
    targetCompatibility = JavaVersion.VERSION_21
}

compose.desktop {
    application {
        mainClass = "com.nesstation.app.MainKt"

        nativeDistributions {
            targetFormats(
                TargetFormat.Msi, TargetFormat.Exe,           // Windows
                TargetFormat.Deb, TargetFormat.Rpm,           // Linux
                TargetFormat.Dmg                              // macOS
            )
            packageName = "NesStation"
            description = "NesStation for PC —— 高质感多平台复古游戏模拟器（Neon 3D / FSD 双主题，libretro 进程内核心 + PC 独立核心）"
            copyright = "© 2026 NesStation contributors (MIT)"
            vendor = "NesStation"
            // 各平台安装包体积优化：排除无用的 JNA 平台库
            modules("java.naming", "java.desktop", "java.instrument", "java.management", "jdk.unsupported", "jdk.crypto.ec")

            windows {
                menuGroup = "NesStation"
                upgradeUuid = "7f8e6d92-4b1a-4c0e-9a3f-nestation-pc01"
                iconFile.set(project.file("packaging/icon.ico"))
            }
            linux {
                menuGroup = "Games"
                iconFile.set(project.file("packaging/icon.png"))
            }
            macOS {
                iconFile.set(project.file("packaging/icon.icns"))
                bundleID = "com.nesstation.app"
            }
        }
    }
}

tasks.withType<org.jetbrains.kotlin.gradle.tasks.KotlinCompile>().configureEach {
    compilerOptions {
        freeCompilerArgs.add("-opt-in=kotlin.RequiresOptIn")
    }
}

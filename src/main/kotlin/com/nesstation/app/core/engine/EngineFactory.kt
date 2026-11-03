package com.nesstation.app.core.engine

import com.nesstation.app.core.coremgmt.CoreRegistry
import com.nesstation.app.core.coremgmt.ExternalCoreDef
import com.nesstation.app.core.coremgmt.LibretroCoreDef
import com.nesstation.app.core.model.GamePlatform

/**
 * 平台 → 引擎工厂（对应 Android 版 EmulatorEngine.forPlatform）。
 *
 * PC 核心组合规则：
 *  - 单核心平台（NES/SFC/GB/GBA/MD/PCE/DOS/ARCADE/NDS/PSX/3DS/GC-Wii/WiiU/DC/Switch/J2ME）
 *    直接返回唯一核心。
 *  - 双核心平台（PS2/PSP）：进程内 libretro（实验性）+ 外置独立核心（推荐），
 *    启动时由 CorePickerDialog 选择，对应 Android 版 NDS「melonDS/激烈」双核心
 *    与 3DS「Azahar/MMJ」双核心的交互（PC 版因激烈/MMJ 无电脑版而改为上述组合）。
 */
object EngineFactory {

    /** 平台可用的核心选择（libretro 在前 / 外置在后）。 */
    fun coreChoices(platform: GamePlatform): List<CoreChoice> {
        val list = ArrayList<CoreChoice>()
        CoreRegistry.libretroFor(platform).forEach {
            list.add(CoreChoice.Libretro(it))
        }
        CoreRegistry.externalFor(platform).forEach {
            list.add(CoreChoice.External(it))
        }
        return list
    }

    fun engineFor(choice: CoreChoice): EmulatorEngine = when (choice) {
        is CoreChoice.Libretro -> LibretroEngine(choice.def)
        is CoreChoice.External -> ExternalEngine(choice.def)
    }

    fun choiceById(platform: GamePlatform, id: String): CoreChoice? =
        coreChoices(platform).firstOrNull { it.id() == id }

    /** 平台是否有多个核心可选（弹选择器的条件）。 */
    fun multiCore(platform: GamePlatform): Boolean = coreChoices(platform).size > 1

    sealed interface CoreChoice {
        fun id(): String
        fun title(): String
        data class Libretro(val def: LibretroCoreDef) : CoreChoice {
            override fun id() = def.id
            override fun title() = "${def.name}（进程内核心）"
        }
        data class External(val def: ExternalCoreDef) : CoreChoice {
            override fun id() = def.id + "-ext"
            override fun title() = "${def.name}（独立核心）"
        }
    }
}

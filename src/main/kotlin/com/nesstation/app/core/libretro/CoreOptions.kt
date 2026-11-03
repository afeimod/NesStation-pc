package com.nesstation.app.core.libretro

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import java.io.File

/**
 * 核心选项（对应 Android 版 PadLayoutStore 里的各核心选项字段；
 * PC 版为「核心 → 键值」JSON，v1/v2 两种核心选项协议统一解析到这里）。
 */
@Serializable
data class CoreOptionChoice(val value: String, val label: String = "")

@Serializable
data class CoreOptionDef(
    val key: String,
    val desc: String = "",
    val info: String = "",
    val category: String = "",
    val choices: List<CoreOptionChoice> = emptyList(),
    val default: String = ""
)

object CoreOptions {

    private val json = Json { ignoreUnknownKeys = true; prettyPrint = true }

    /** coreId → optionKey → 定义（由 SET_CORE_OPTIONS_V2 / SET_VARIABLES 填充）。 */
    @Volatile private var defs: Map<String, List<CoreOptionDef>> = emptyMap()

    /** 用户已选值：coreId → optionKey → value。 */
    @Volatile private var values: Map<String, Map<String, String>> = loadValues()

    private fun file(): File =
        File(com.nesstation.app.core.storage.Paths.dataDir, "core-options.json")

    private fun loadValues(): Map<String, Map<String, String>> = runCatching {
        @Suppress("UNCHECKED_CAST")
        json.parseToJsonElement(file().readText()).let { el ->
            (el as? kotlinx.serialization.json.JsonObject)?.mapValues { (_, core) ->
                (core as? kotlinx.serialization.json.JsonObject)?.mapValues { (_, v) ->
                    (v as? kotlinx.serialization.json.JsonPrimitive)?.content ?: ""
                } ?: emptyMap()
            } ?: emptyMap()
        }
    }.getOrDefault(emptyMap())

    fun definitions(coreId: String): List<CoreOptionDef> = defs[coreId] ?: emptyList()

    fun setDefinitions(coreId: String, list: List<CoreOptionDef>) {
        defs = defs + (coreId to list)
    }

    /** GET_VARIABLE 返回值：用户值 → 核心默认 → 第一个选项 → 空串。 */
    fun effectiveValue(coreId: String, key: String): String {
        val user = values[coreId]?.get(key)
        if (!user.isNullOrEmpty()) return user
        val def = defs[coreId]?.firstOrNull { it.key == key } ?: return ""
        return def.default.ifEmpty { def.choices.firstOrNull()?.value ?: "" }
    }

    fun userValue(coreId: String, key: String): String? = values[coreId]?.get(key)

    fun setValue(coreId: String, key: String, value: String) {
        val core = values[coreId] ?: emptyMap()
        values = values + (coreId to (core + (key to value)))
        runCatching { file().writeText(json.encodeToString(kotlinx.serialization.json.JsonObject.serializer(), buildJsonObject(values))) }
    }

    private fun buildJsonObject(map: Map<String, Map<String, String>>): kotlinx.serialization.json.JsonObject =
        kotlinx.serialization.json.JsonObject(
            map.mapValues { (_, core) ->
                kotlinx.serialization.json.JsonObject(core.mapValues { (_, v) -> kotlinx.serialization.json.JsonPrimitive(v) })
            }
        )
}

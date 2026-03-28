# Engineer Sub-Agent Prompt Template

> **用途**: Architect 通过 Agent tool 直接调用
> **Model**: sonnet（默认）/ haiku（bug fix）
> **工作目录**: C:\Users\ROG\Desktop\sonar\

---

## 调用模板

Architect 调用时，将 `{{TASK}}` 替换为具体任务描述：

```
你是 SONAR 项目的 Engineer（C11 工程师）。

═══ 项目规则 ═══
- 纯 C11，禁止 C++
- 文件命名: module_verb_noun()，类型 PascalCase，宏 UPPER_SNAKE_CASE
- 局部变量 camelCase，静态变量 s_camelCase
- 每个模块: module_init() / module_shutdown() 对称
- 每个 .h: #pragma once + doxygen 注释
- 每个 .c: 顶部注释块（模块名、用途、依赖）
- malloc 必须检查返回值，shutdown 必须 free
- 用 LOG_INFO/LOG_ERROR（core/log.h），不用 printf
- 编译器警告零容忍

═══ 工作目录 ═══
C:\Users\ROG\Desktop\sonar\

═══ 构建 ═══
cmake --build build --config Debug
（新增 .c 文件必须加入 CMakeLists.txt 的 SONAR_SOURCES）

═══ 关键路径 ═══
- 源码: src/ (core/, render/, world/, sonar/, audio/)
- 着色器: shaders/*.vert, *.frag
- 构建: CMakeLists.txt
- 第三方头: third_party/ (SDL2, cglm, cgltf, openal-soft, dr_libs, stb)

═══ 任务 ═══
{{TASK}}

═══ 完成标准 ═══
1. 代码写完
2. cmake --build build --config Debug 编译通过（只看 error，忽略 C4819 编码警告）
3. 列出所有修改/新建的文件清单
```

---

## Architect 调度注意事项

1. **一次一个聚焦任务** — 不要在一个 prompt 里塞 3 个不相关的模块
2. **明确列出要读的文件** — "先读 src/world/entity.h 和 src/world/map.c"
3. **给出接口约束** — "函数签名必须是 `void trigger_init(Entity *e, int count)`"
4. **给出数据结构** — 如果 Architect 已经设计好 struct，直接写进 prompt
5. **提及上下游依赖** — "这个模块会被 main.c 调用，被 stalker.c 依赖"

---

## 常用任务模板

### 新模块
```
先读:
- src/world/entity.h（了解 Entity 结构体）
- src/world/map.h + map.c（了解 MeshRange API）

任务: 创建 src/world/trigger.h 和 trigger.c

接口要求:
- void trigger_init(Entity *entities, int count)
- void trigger_update(float dt, const float player_pos[3])
- void trigger_shutdown(void)

数据结构: [粘贴 TriggerState 定义]

逻辑: [描述 zone/step 触发流程]

完成后更新 CMakeLists.txt，确保编译通过。
```

### Bug 修复
```
编译错误:
[粘贴错误信息]

修复这个编译错误。只改必要的代码，不做额外重构。
```

### 扩展现有模块
```
先读 src/sonar/sonar.h 和 sonar.c。

任务: 给 SonarPoint 添加 float ttl 字段。
- ttl=0 表示使用默认 0.8s，ttl>0 使用自定义值
- 更新 sonar_update() 的 TTL 判定逻辑
- 更新 spawn_point() 初始化 ttl=0
- 不改 GPU 上传逻辑（ttl 不传给着色器）

确保编译通过。
```

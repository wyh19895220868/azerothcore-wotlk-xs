# AGENTS.md

## 本地项目约定（适用于本仓库全部目录）

### 目录与迁移目标

- 新服务端主修改目录：`E:\wowdecomp\xs-wow\azerothcore-wotlk-xs`。后续服务端功能移植在此实现。
- 旧服务端参考目录：`E:\AA\azerothcore-wotlk-master`；原始项目参考目录：`E:\AA\NAz-xszx1`。两者默认只读，按功能核对并移植，不整目录覆盖新项目。
- 新机器人唯一指定源码：`E:\wowdecomp\xs-wow\mod-playerbots-xs`。禁止移植、启用或混入旧服务端及原始项目中的旧机器人实现。
- 服务端加壳工具目录：`E:\AA\UPX`。加壳属于构建后的独立步骤，按任务需要执行，并保留未加壳原件及符号以便排错。
- 配套客户端规则入口：`E:\wowdecomp\wowdecomp\AGENTS.md`；涉及客户端与服务端通讯时核对两端实现。

### 构建工具（后续编译优先使用以下指定安装）

- CMake：`E:\CMake3.25.3`。
- Boost：`E:\boost_1_78_0`。
- MySQL：`E:\mysql-8.0.12-winx64`。
- OpenSSL：`E:\OpenSSL-Win64`。
- Visual Studio 2022 / MSVC：`E:\Vs2022-IDE`。
- 后续服务端编译默认使用 `RelWithDebInfo` 配置，以保留优化和调试符号；除非用户明确指定，不用 `Release` 或 `Debug` 替代。命令行构建使用 `cmake --build ... --config RelWithDebInfo`。
- 配置前核实实际可执行文件、头文件和库路径，以及版本、架构和编译器兼容性；使用显式工具路径及项目支持的依赖参数，避免 PATH 或缓存误用其他安装。若指定版本与新核心不兼容，报告具体错误和处理建议，不擅自更换依赖。
- 使用独立构建目录，不复用旧服务端缓存。只有用户要求构建、配置或验证编译时才执行；纯文档修改无需编译。

### 功能移植与验证

- 先定位旧功能入口、调用链、配置、SQL 和客户端协议，再对照新核心接口逐项移植；优先沿用新项目及模块扩展方式。
- 每次只处理当前功能及直接必要改动，保护用户已有修改，不夹带无关重构。保留来源和必要的上游作者信息。
- 机器人接入必须使用上述新机器人源码，并核对其核心补丁、构建要求、配置和数据库迁移；不假定目录存在即已接入或运行成功。外部机器人仓库的变更须单独提交，服务端提交中记录对应版本及接入方式。
- 数据库变更遵循下方 SQL 规则，不用旧库整体覆盖新库，不将真实账号、密码或生产数据提交到 Git。
- 中文文档、配置及 SQL 使用 UTF-8 无 BOM，代码格式遵循 `.editorconfig`。
- 根据实际改动执行静态检查、相关构建和必要回归；分别报告编译、部署、启动、联机、功能及性能验收的实际状态。未执行的检查明确标注，不以编译成功替代游戏内验收。

### Git 工作流（每次修改必须执行）

- 服务端远程仓库：`https://github.com/wyh19895220868/azerothcore-wotlk-xs`。每次修改必须留下 Git 提交记录，并推送到此仓库的功能子分支。
- 开始前确认仓库路径、远程地址、当前分支、工作区状态和已有差异；禁止覆盖或提交他人的无关修改。
- 修改前创建或切换到合适的功能子分支，例如 `feat/<功能>`、`fix/<功能>`、`docs/<主题>`；禁止直接在 `main` / `master` 上修改并提交。
- 按功能块提交，仅暂存本次文件或补丁；提交前检查 staged diff 并运行 `git diff --cached --check`，不盲目 `git add .`。
- 提交完成后执行 `git push -u origin <功能分支>`；每次任务包含向上述服务端远程推送功能分支的授权，无需重复询问。不得擅自合并主分支、强制推送或重写他人提交。
- 不提交构建产物、日志、凭据、数据库备份和游戏资源。跨独立仓库的文件在各自仓库记录提交，不能将客户端仓库误推到服务端远程。
- 若提交或推送失败，保留本地工作并报告实际原因；完成时报告修改文件、分支、提交哈希、推送结果、验证结果和遗留事项。

以下保留上游开发规范；与上述本地项目约定冲突时，以用户要求和上述约定为准。

AzerothCore is a C++ MMORPG server emulator for World of Warcraft 3.3.5a (WotLK), built with CMake, backed by MySQL.

## Agent rules

- **Do not configure or build unless explicitly asked.** Builds are slow and rarely needed for code changes.
- **Never edit SQL files outside `data/sql/updates/pending_db_*/` unless explicitly requested.** `data/sql/base/`, `data/sql/archive/`, and `data/sql/updates/db_*/` are immutable.
- Formatting follows `.editorconfig`: UTF-8, LF, max 120 cols, trailing newline, no trailing whitespace; 4-space indent for C++ (tabs forbidden), 2-space for JSON/YAML/sh/ts/js.
- **Do not write or add live-stack e2e tests unless the user explicitly asks.** Do not create `e2e/local/` scratch tests to validate a fix. Existing `e2e/` tests that break after a change may be updated. Do not mention missing coverage unless the user asked about it.
- Planning docs go in `.agents/plans/<task-slug>/` (gitignored), named `<task-slug>.<TYPE>.md` (`PLAN`, `REQUIREMENTS`, `ANALYSIS`, …).
- **Credit upstream authors.** Code, a mechanism, or data mirrored from another core (TrinityCore, cMaNGOS, …) is committed with `--author` naming the original commit's author (extra sources as `Co-authored-by`), even when rewritten against AC or confirmed by own sniffs; find them in the upstream file's commit history.

## Mandatory reading per task

Read the matching doc(s) BEFORE starting the task:

- Compiling, configuring, or running tests → `.agents/docs/build.md`
- Writing or modifying C++ → `.agents/docs/cpp-guidelines.md`
  - Script work (under `src/server/scripts/`) → also `.agents/docs/cpp-scripts.md`
- Creating or modifying SQL → `.agents/docs/sql-guidelines.md`
  - SmartAI work (`smart_scripts` data) → also `.agents/docs/cpp-scripts.md`
- Reviewing a changeset or PR → `.agents/docs/code-review.md`
- Self-reviewing, or opening or updating a PR → also `.agents/docs/self-review-rules.md`
- Touching a subsystem that has a doc in `.agents/docs/systems/` → read that doc too
- Changing existing live-stack e2e (`e2e/`), and only when the user asked or an existing test broke → `e2e/README.md` and `.agents/docs/e2e-policy.md`
- Capturing a lesson or adding/updating agent docs → `.agents/docs/README.md`

## Repository layout

- `src/common/` — networking (Asio), crypto, config, logging, shared utilities.
- `src/server/game/` — core gameplay; compiled into worldserver.
- `src/server/scripts/` — content scripts grouped by region (`EasternKingdoms/`, `Northrend/`, …), class (`Spells/spell_mage.cpp`, …), and domain (`Commands/`, `Pet/`, `OutdoorPvP/`, `World/`).
- `src/server/database/` — DB abstraction and schema updater.
- `src/server/shared/` — code shared by auth and world servers.
- `src/server/apps/{authserver,worldserver}/` — entry points (ports 3724 and 8085).
- `src/test/` — unit tests + mocks.
- `e2e/` — live-stack Go e2e (AzerothGhost harness); see `e2e/README.md`.
- `data/sql/` — `base/` (historical schema), `updates/db_*/` (merged), `updates/pending_db_*/` (in-flight), `custom/` (gitignored).
- `modules/` — external modules (see below).
- `apps/` — helper scripts; `apps/codestyle/` holds the lint scripts.
- `conf/dist/` — distributed config templates; `conf/*.conf` is gitignored.
- `deps/` — vendored third-party dependencies.

## Modules

External modules live in `modules/`, each a subdir with its own `CMakeLists.txt`. Disable with `-DDISABLED_AC_MODULES="mod1;mod2"`. See `modules/how_to_make_a_module.md`.

## Persisting lessons

When a user correction reveals a lesson that generalizes, offer to persist it into these docs (placement per `.agents/docs/README.md`): use the `/self-improve` skill if installed, otherwise suggest the user to install it and read this page: https://www.azerothcore.org/wiki/agentic-engineering

# 通信项目文档研读进度

## 2026-06-07
- 启动任务，确认需要阅读 `.docx`、重点关注第 5 章和第 15 章、保存项目级记忆并创建工作树。
- 发现 `rg.exe` 被拒绝执行、`git` 不在 PATH；后续使用 PowerShell 和 Codex worktree 工具替代。
- 已创建 `.planning/communication_doc_review/` 计划文件。
- 第一次 `.docx` 抽取尝试因 PowerShell 不支持 Bash here-doc 失败；改用 PowerShell here-string。
- 第二次抽取因中文文件名编码问题失败；下一步改为脚本自动枚举 `.docx`。
- 第三次抽取因脚本内中文正则被转码失败；下一步改为全 ASCII 脚本。
- `.docx` 抽取文件已生成：`doc_full_text.txt`、`doc_headings.json`、`doc_meta.json`。控制台打印中文标题时出现 GBK 编码错误，不影响抽取产物。
- 已生成 `chapter_5.txt`、`chapter_5_headings.txt`、`chapter_15.txt`、`chapter_15_headings.txt` 和章节范围索引。
- 修正章节抽取逻辑，生成干净版：`doc_full_text_clean.txt`、`chapter_5_clean.txt`、`chapter_15_clean.txt` 和 `clean_section_*` 文件。
- 已保存通信项目摘要：`.planning/communication_doc_review/communication_work_summary.md`。
- 尝试写入全局记忆目录时被审批服务 503 拒绝；未绕过。
- 已通过 Codex fork worktree 提交工作树创建请求，pendingWorktreeId 为 `local:c3a4a2a0-3c32-4a7f-accf-d11166741082`。
- 2026-06-07：根据用户要求创建 `week1.0/`，仅包含 OLSR/NS-3 仿真工作流、参数表、运行矩阵和结果占位目录；未编写仿真代码。
- 2026-06-07：已在 `AGENTS.md` 中补充记忆：OLSR 仿真也必须使用 Ubuntu VM + NS-3，不能用 Python/networkx 等替代 NS-3。

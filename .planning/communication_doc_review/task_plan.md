# 通信项目文档研读计划

## 目标
- 阅读项目 `.docx`，理解整体项目内容。
- 重点研读第 5 章和第 15 章，提炼通信部分需要完成的工作。
- 将最重要的项目级结论保存为记忆更新。
- 创建一个后续工作用的 Codex 工作树。

## 阶段
1. [complete] 检查项目文件与工具环境。
2. [complete] 抽取 `.docx` 文本、目录、表格和章节结构。
3. [complete] 研读全文并重点整理第 5、15 章通信任务。
4. [blocked] 保存项目级记忆更新：全局记忆目录写入因审批服务 503 被拒，项目内摘要已保存。
5. [complete] 创建工作树并汇报状态。

## 已知环境限制
- `rg.exe` 在当前托管环境中拒绝运行，需要使用 PowerShell 枚举替代。
- `git` 当前不在 PATH，普通 `git worktree` 命令不可用。

## 错误记录
| 时间 | 错误 | 处理 |
|---|---|---|
| 2026-06-07 | 在 PowerShell 中误用了 Bash here-doc (`<<'PY'`) 导致脚本解析失败 | 改用 PowerShell here-string 管道给 Python |
| 2026-06-07 | PowerShell 管道传递中文 `.docx` 文件名时被替换为问号，`python-docx` 找不到文件 | 改为 Python 在目录中自动枚举 `.docx` 文件 |
| 2026-06-07 | Python 脚本中的中文正则经 PowerShell 管道被转码，正则编译失败 | 改用 ASCII 脚本和 Unicode 转义范围 |
| 2026-06-07 | 抽取完成后，打印标题到 GBK 控制台时遇到不可编码字符 | 抽取文件已生成；改为读取 UTF-8 文件和输出 ASCII/JSON 摘要 |
| 2026-06-07 | 写入 `C:\Users\yyk\.codex\memories\extensions\ad_hoc\notes` 时审批层 503，操作被拒 | 未绕过限制；完整项目级摘要保存在 `.planning/communication_doc_review/communication_work_summary.md` |

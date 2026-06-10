const fs = require("fs");
const path = require("path");
let sharp = null;
try {
  sharp = require("sharp");
} catch {
  sharp = null;
}

const root = __dirname;
const csvPath = path.join(root, "results", "olsr_baseline_from_terminal.csv");
const figuresDir = path.join(root, "figures");
const svgPath = path.join(figuresDir, "olsr_baseline_summary_zh.svg");
const pngPath = path.join(figuresDir, "olsr_baseline_summary_zh.png");

function parseCsv(text) {
  const lines = text.trim().split(/\r?\n/);
  const headers = lines.shift().split(",");
  return lines.map((line) => {
    const values = line.split(",");
    return Object.fromEntries(headers.map((h, i) => [h, values[i]]));
  });
}

function esc(value) {
  return String(value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function text(x, y, content, opts = {}) {
  const {
    size = 24,
    weight = 400,
    fill = "#243142",
    anchor = "start",
    family = "Noto Sans SC, Microsoft YaHei, SimHei, Arial, sans-serif",
  } = opts;
  return `<text x="${x}" y="${y}" font-family="${family}" font-size="${size}" font-weight="${weight}" fill="${fill}" text-anchor="${anchor}">${esc(content)}</text>`;
}

function rect(x, y, w, h, fill, opts = {}) {
  const {
    rx = 0,
    stroke = "none",
    sw = 0,
    opacity = 1,
  } = opts;
  return `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${rx}" fill="${fill}" stroke="${stroke}" stroke-width="${sw}" opacity="${opacity}"/>`;
}

function line(x1, y1, x2, y2, opts = {}) {
  const { stroke = "#cfd8e3", sw = 1, dash = "" } = opts;
  return `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${stroke}" stroke-width="${sw}"${dash ? ` stroke-dasharray="${dash}"` : ""}/>`;
}

function makeSvg(rows) {
  const width = 1600;
  const height = 960;
  const cardStroke = "#cfd8e3";
  const gridStroke = "#d9e2ec";
  const pdrColor = "#2f855a";
  const delayColor = "#2f78a8";
  const jitterColor = "#c7782d";

  const pdr = rows.map((row) => Number(row.pdr));
  const delay = rows.map((row) => Number(row.mean_delay_ms));
  const jitter = rows.map((row) => Number(row.jitter_ms));

  const left = { x: 55, y: 136, w: 690, h: 405 };
  const right = { x: 855, y: 136, w: 690, h: 405 };
  const note = { x: 55, y: 592, w: 1490, h: 330 };

  const parts = [];
  parts.push(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`);
  parts.push(rect(0, 0, width, height, "#f4f7fb"));
  parts.push(text(55, 76, "OLSR 静态场景基线仿真结果", { size: 40, weight: 700, fill: "#1f2a37" }));
  parts.push(text(57, 110, "NS-3.43 / Ubuntu 虚拟机 / 30 架无人机 / 仿真时长 300 秒 / 随机种子 1", { size: 22, fill: "#536276" }));

  parts.push(rect(left.x, left.y, left.w, left.h, "#ffffff", { rx: 14, stroke: cardStroke, sw: 1.2 }));
  parts.push(rect(right.x, right.y, right.w, right.h, "#ffffff", { rx: 14, stroke: cardStroke, sw: 1.2 }));
  parts.push(rect(note.x, note.y, note.w, note.h, "#ffffff", { rx: 14, stroke: cardStroke, sw: 1.2 }));

  parts.push(text(left.x + 24, left.y + 43, "各业务流投递率", { size: 25, weight: 700 }));
  parts.push(text(right.x + 24, right.y + 43, "端到端时延与抖动", { size: 25, weight: 700 }));

  const chart1 = { x: left.x + 70, y: left.y + 74, w: left.w - 115, h: 226 };
  const chart2 = { x: right.x + 70, y: right.y + 74, w: right.w - 115, h: 226 };

  [0, 25, 50, 75, 100].forEach((tick) => {
    const y = chart1.y + chart1.h - (tick / 100) * chart1.h;
    parts.push(line(chart1.x, y, chart1.x + chart1.w, y, { stroke: gridStroke, sw: 1 }));
    parts.push(text(chart1.x - 20, y + 6, tick, { size: 15, fill: "#64748b", anchor: "end" }));
  });
  parts.push(line(chart1.x, chart1.y, chart1.x, chart1.y + chart1.h, { stroke: "#94a3b8", sw: 1.4 }));
  parts.push(line(chart1.x, chart1.y + chart1.h, chart1.x + chart1.w, chart1.y + chart1.h, { stroke: "#94a3b8", sw: 1.4 }));
  parts.push(text(chart1.x - 44, chart1.y + 120, "投递率（%）", { size: 16, fill: "#475569", anchor: "middle", family: "Noto Sans SC, Microsoft YaHei, SimHei, Arial, sans-serif", weight: 400 }).replace("<text ", `<text transform="rotate(-90 ${chart1.x - 44} ${chart1.y + 120})" `));

  const groupW1 = chart1.w / rows.length;
  rows.forEach((row, i) => {
    const barW = 72;
    const barH = (pdr[i] / 100) * chart1.h;
    const x = chart1.x + groupW1 * i + groupW1 / 2 - barW / 2;
    const y = chart1.y + chart1.h - barH;
    parts.push(rect(x, y, barW, barH, pdrColor, { rx: 9 }));
    parts.push(text(x + barW / 2, y - 11, `${pdr[i].toFixed(0)}%`, { size: 17, fill: "#1f2a37", anchor: "middle" }));
    parts.push(text(chart1.x + groupW1 * i + groupW1 / 2, chart1.y + chart1.h + 31, `业务流${row.flow_id}`, { size: 16, fill: "#1f2a37", anchor: "middle" }));
    parts.push(text(chart1.x + groupW1 * i + groupW1 / 2, chart1.y + chart1.h + 56, row.src, { size: 14, fill: "#667085", anchor: "middle" }));
  });
  parts.push(text(left.x + 24, left.y + left.h - 20, "5 条业务流均收到 2/2 个应用包，投递率均为 100%。", { size: 17, fill: "#344054" }));

  [0, 15, 30, 45].forEach((tick) => {
    const y = chart2.y + chart2.h - (tick / 45) * chart2.h;
    parts.push(line(chart2.x, y, chart2.x + chart2.w, y, { stroke: gridStroke, sw: 1 }));
    parts.push(text(chart2.x - 20, y + 6, tick, { size: 15, fill: "#64748b", anchor: "end" }));
  });
  parts.push(line(chart2.x, chart2.y, chart2.x, chart2.y + chart2.h, { stroke: "#94a3b8", sw: 1.4 }));
  parts.push(line(chart2.x, chart2.y + chart2.h, chart2.x + chart2.w, chart2.y + chart2.h, { stroke: "#94a3b8", sw: 1.4 }));
  parts.push(text(chart2.x - 44, chart2.y + 120, "时间（毫秒）", { size: 16, fill: "#475569", anchor: "middle" }).replace("<text ", `<text transform="rotate(-90 ${chart2.x - 44} ${chart2.y + 120})" `));

  parts.push(rect(right.x + right.w - 294, right.y + 24, 22, 22, delayColor));
  parts.push(text(right.x + right.w - 262, right.y + 42, "平均时延", { size: 16, fill: "#344054" }));
  parts.push(rect(right.x + right.w - 160, right.y + 24, 22, 22, jitterColor));
  parts.push(text(right.x + right.w - 128, right.y + 42, "抖动", { size: 16, fill: "#344054" }));

  const groupW2 = chart2.w / rows.length;
  rows.forEach((row, i) => {
    const center = chart2.x + groupW2 * i + groupW2 / 2;
    const barW = 34;
    const delayH = (delay[i] / 45) * chart2.h;
    const jitterH = (jitter[i] / 45) * chart2.h;
    const dx = center - barW - 4;
    const jx = center + 4;
    parts.push(rect(dx, chart2.y + chart2.h - delayH, barW, delayH, delayColor, { rx: 5 }));
    parts.push(rect(jx, chart2.y + chart2.h - jitterH, barW, jitterH, jitterColor, { rx: 5 }));
    parts.push(text(dx + barW / 2, chart2.y + chart2.h - delayH - 9, delay[i].toFixed(0), { size: 14, fill: "#1f2a37", anchor: "middle" }));
    parts.push(text(jx + barW / 2, chart2.y + chart2.h - jitterH - 9, jitter[i].toFixed(0), { size: 14, fill: "#1f2a37", anchor: "middle" }));
    parts.push(text(center, chart2.y + chart2.h + 31, `业务流${row.flow_id}`, { size: 16, fill: "#1f2a37", anchor: "middle" }));
  });
  parts.push(text(right.x + 24, right.y + right.h - 20, "当前统计来自少量应用包，后续正式评估需要增加业务量和多随机种子。", { size: 17, fill: "#344054" }));

  parts.push(text(note.x + 24, note.y + 43, "结果摘要", { size: 25, weight: 700 }));
  [
    "场景：静态 OLSR 基线；节点规模：地面站 1 个 + 无人机 30 架；无线通信范围：2000 米。",
    "总体结果：发送应用包 10 个，接收应用包 10 个，总体投递率 100.0%，平均时延 10.5 毫秒。",
    "信道活动：物理层总发送 1,092,364 字节，包含 OLSR 路由控制开销和应用数据。",
    "路由现象：路由日志中可看到多跳路径，最大跳数约 5 跳，说明 OLSR 在该静态链式场景下已形成转发路径。",
    "注意：这是第一版跑通结果，不是最终性能结论；目前总应用包数较少，平均/最大时延字段还需要用原始 FlowMonitor 再核对。",
  ].forEach((content, i) => {
    parts.push(text(note.x + 30, note.y + 80 + i * 34, content, { size: 20, fill: "#344054" }));
  });

  parts.push(rect(note.x + 28, note.y + note.h - 64, note.w - 56, 42, "#fff5d6", { rx: 8, stroke: "#e9a23b", sw: 1 }));
  parts.push(text(note.x + 44, note.y + note.h - 37, "汇报口径：已完成 OLSR/NS-3 基线场景跑通和结果导出；未完成多场景、多随机种子、移动性、弱链路和协议对比。", { size: 17, fill: "#5f3b00" }));

  parts.push("</svg>");
  return parts.join("\n");
}

async function main() {
  const rows = parseCsv(fs.readFileSync(csvPath, "utf8"));
  fs.mkdirSync(figuresDir, { recursive: true });
  const svg = makeSvg(rows);
  fs.writeFileSync(svgPath, svg, "utf8");
  if (sharp) {
    await sharp(Buffer.from(svg)).png().toFile(pngPath);
    console.log(pngPath);
  } else {
    console.log("未检测到可用的 sharp，已跳过 PNG 转换。");
  }
  console.log(svgPath);
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});

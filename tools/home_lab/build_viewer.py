#!/usr/bin/env python3
"""Assemble lab.html: a static viewer for the home-page design lab output.

Reads the PNGs and lint.json that render_lab.sh / render_png.py wrote into the
output directory and embeds a small viewer (week scrubber, orientation switch,
hit-area overlay, side-by-side concept comparison, lint per frame).
Usage: build_viewer.py <output_dir>
"""
import json
import re
import sys
from pathlib import Path

CONCEPTS = {
    "e_medallion": ("E 勋章表盘",
                    "超大衬线数字做主角；宝宝物件被 40 颗周点围成一枚勋章："
                    "实心=已过的周，圆环=进行中的一周，小点=还没到。像一件摆在桌边的钟表/纪念章。"),
    "f_rings": ("F 年轮",
                "七圈不规则轮廓像树的年轮包住孕周数字，实线=已长成的圈、虚线=将要长的圈；"
                "轮廓相位随孕周变化，每周形状都略有不同。延续 assets/concepts 里的 Rings of Time。"),
    "g_specimen": ("G 标本图版",
                   "博物馆图版式：细线页眉、超大数字、物件作为“标本”配英文名，"
                   "底部 40 格周刻度尺（高=已过，最高=进行中，矮=未到）。含身长占位数据，未经临床审核。"),
}

NAME = re.compile(r"^(?P<concept>.+)_w(?P<week>\d+)_(?P<orient>landscape|portrait)_zh$")


def main():
    out = Path(sys.argv[1])
    lint = json.loads((out / "lint.json").read_text(encoding="utf-8"))
    frames = {}
    for stem, result in lint.items():
        m = NAME.match(stem)
        if not m:
            continue
        key = f"{m['concept']}|{m['week']}|{m['orient']}"
        fonts = result["fonts"]
        frames[key] = {
            "stem": stem,
            # caption-tier (16-17 px) is a review hint, not a failure
            "problems": result["warnings"] + [f for f in fonts if not f.startswith("caption")],
            "captions": sum(1 for f in fonts if f.startswith("caption")),
        }
    concepts = sorted({k.split("|")[0] for k in frames})
    weeks = sorted({int(k.split("|")[1]) for k in frames})
    meta = {c: CONCEPTS.get(c, (c, "")) for c in concepts}
    page = TEMPLATE.replace("__FRAMES__", json.dumps(frames, ensure_ascii=False)) \
                   .replace("__CONCEPTS__", json.dumps(meta, ensure_ascii=False)) \
                   .replace("__WEEKS__", json.dumps(weeks))
    (out / "lab.html").write_text(page, encoding="utf-8")
    bad = sum(1 for f in frames.values() if f["problems"])
    print(f"viewer: {len(frames)} frames, {bad} with problems -> {out}/lab.html")


TEMPLATE = """<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Home Lab</title>
<style>
:root{--bg:#f5f4f0;--fg:#1b1b1b;--muted:#666;--line:#cfccc4;--card:#fff;--bad:#b3261e;--ok:#1b6e3c}
@media (prefers-color-scheme:dark){:root{--bg:#161616;--fg:#eee;--muted:#9a9a9a;--line:#3a3a3a;--card:#202020;--bad:#ff8a80;--ok:#7fd69a}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.5 -apple-system,"PingFang SC","Helvetica Neue",sans-serif}
header{padding:16px 24px;border-bottom:1px solid var(--line)}
h1{margin:0;font-size:18px}
header p{margin:2px 0 0;color:var(--muted)}
.bar{display:flex;flex-wrap:wrap;gap:12px 24px;align-items:center;padding:12px 24px;border-bottom:1px solid var(--line);position:sticky;top:0;background:var(--bg);z-index:2}
.group{display:flex;gap:4px;align-items:center}
.group label{color:var(--muted);margin-right:4px}
button{font:inherit;color:inherit;background:var(--card);border:1px solid var(--line);border-radius:6px;padding:4px 10px;cursor:pointer}
button[aria-pressed=true]{background:var(--fg);color:var(--bg);border-color:var(--fg)}
input[type=range]{width:240px}
.wk{min-width:5.5em;text-align:center;font-variant-numeric:tabular-nums;font-weight:600}
main{padding:16px 24px 48px}
.note{max-width:70ch;color:var(--muted);margin:0 0 12px}
.grid{display:flex;flex-wrap:wrap;gap:16px;align-items:flex-start}
figure{margin:0;background:var(--card);border:1px solid var(--line);border-radius:8px;padding:10px}
figcaption{font-weight:600;margin-bottom:6px}
img{display:block;image-rendering:pixelated;border:1px solid #888;background:#fff;max-width:100%;height:auto}
ul.lint{margin:6px 0 0;padding-left:18px;color:var(--bad);font-size:12px}
.ok{color:var(--ok);font-size:12px;margin-top:6px}
.cap{color:var(--muted);font-size:12px;margin-top:2px}
footer{padding:0 24px 24px;color:var(--muted);font-size:12px}
</style></head><body>
<header><h1>首页设计实验室 · Home Lab</h1>
<p>由真实固件渲染代码生成的 1-bit 画面（与送屏数据一致）。仅宿主预览，未经真机验证；身长/体重为版面占位数据，未经临床审核。</p></header>
<div class="bar">
  <div class="group"><label>视图</label><button id="m-one" aria-pressed="true">单个方案</button><button id="m-all" aria-pressed="false">并排对比</button></div>
  <div class="group" id="g-concept"><label>方案</label></div>
  <div class="group"><label>方向</label><button id="o-landscape" aria-pressed="true">横屏</button><button id="o-portrait" aria-pressed="false">竖屏</button></div>
  <div class="group"><label>孕周</label><button id="w-prev">◀</button><input id="w" type="range"><button id="w-next">▶</button><span class="wk" id="wk"></span></div>
  <div class="group"><label>叠加</label><button id="hits" aria-pressed="false">命中区</button><button id="half" aria-pressed="false">50%</button></div>
</div>
<main><p class="note" id="note"></p><div class="grid" id="grid"></div></main>
<footer>键盘：← → 切换孕周；O 切换方向；H 命中区。图中红字=必须修复（灰阶、越界、重叠、缺字、命中区过小），16–17 px 说明类文字只计数，需人工确认不承载操作信息。</footer>
<script>
const FRAMES=__FRAMES__, CONCEPTS=__CONCEPTS__, WEEKS=__WEEKS__;
const ids=Object.keys(CONCEPTS);
const S={mode:'one',concept:ids[0],orient:'landscape',week:WEEKS.includes(21)?21:WEEKS[0],hits:false,half:false};
try{Object.assign(S,JSON.parse(localStorage.getItem('homelab')||'{}'))}catch(e){}
if(!ids.includes(S.concept))S.concept=ids[0];
if(!WEEKS.includes(S.week))S.week=WEEKS[0];
const $=s=>document.querySelector(s);
const gc=$('#g-concept');
ids.forEach(id=>{const b=document.createElement('button');b.textContent=CONCEPTS[id][0];b.dataset.id=id;b.onclick=()=>{S.concept=id;S.mode='one';draw()};gc.appendChild(b)});
const w=$('#w');w.min=0;w.max=WEEKS.length-1;w.oninput=()=>{S.week=WEEKS[+w.value];draw()};
$('#m-one').onclick=()=>{S.mode='one';draw()};$('#m-all').onclick=()=>{S.mode='all';draw()};
$('#o-landscape').onclick=()=>{S.orient='landscape';draw()};$('#o-portrait').onclick=()=>{S.orient='portrait';draw()};
$('#hits').onclick=()=>{S.hits=!S.hits;draw()};$('#half').onclick=()=>{S.half=!S.half;draw()};
const step=d=>{const i=WEEKS.indexOf(S.week)+d;if(i>=0&&i<WEEKS.length){S.week=WEEKS[i];draw()}};
$('#w-prev').onclick=()=>step(-1);$('#w-next').onclick=()=>step(1);
addEventListener('keydown',e=>{if(e.key==='ArrowLeft')step(-1);else if(e.key==='ArrowRight')step(1);else if(e.key==='o'){S.orient=S.orient==='landscape'?'portrait':'landscape';draw()}else if(e.key==='h'){S.hits=!S.hits;draw()}});
function fig(id){
  const f=FRAMES[id+'|'+S.week+'|'+S.orient];const el=document.createElement('figure');
  const cap=document.createElement('figcaption');cap.textContent=CONCEPTS[id][0]+' · 孕 '+S.week+' 周';el.appendChild(cap);
  if(!f){el.append('（该孕周未渲染）');return el}
  const img=document.createElement('img');img.src=f.stem+(S.hits?'.hits':'')+'.png';img.alt=cap.textContent;
  const base=S.orient==='landscape'?800:480;img.style.width=(S.half||S.mode==='all'?base/2:base)+'px';el.appendChild(img);
  if(f.problems.length){const ul=document.createElement('ul');ul.className='lint';f.problems.forEach(p=>{const li=document.createElement('li');li.textContent=p;ul.appendChild(li)});el.appendChild(ul)}
  else{const d=document.createElement('div');d.className='ok';d.textContent='✓ 无灰阶 / 越界 / 重叠 / 缺字 / 小命中区';el.appendChild(d)}
  const c=document.createElement('div');c.className='cap';c.textContent=f.captions+' 处 16–17 px 说明文字待人工确认';el.appendChild(c);
  return el}
function draw(){
  try{localStorage.setItem('homelab',JSON.stringify(S))}catch(e){}
  $('#m-one').setAttribute('aria-pressed',S.mode==='one');$('#m-all').setAttribute('aria-pressed',S.mode==='all');
  gc.querySelectorAll('button').forEach(b=>b.setAttribute('aria-pressed',S.mode==='one'&&b.dataset.id===S.concept));
  $('#o-landscape').setAttribute('aria-pressed',S.orient==='landscape');$('#o-portrait').setAttribute('aria-pressed',S.orient==='portrait');
  $('#hits').setAttribute('aria-pressed',S.hits);$('#half').setAttribute('aria-pressed',S.half);
  w.value=WEEKS.indexOf(S.week);$('#wk').textContent='孕 '+S.week+' 周';
  $('#note').textContent=S.mode==='one'?CONCEPTS[S.concept][1]:'同一孕周、同一方向下的全部方案；拖动孕周滑块观察物件、进度和文字长度的变化。';
  const g=$('#grid');g.replaceChildren();
  (S.mode==='one'?[S.concept]:ids).forEach(id=>g.appendChild(fig(id)));
}
draw();
</script></body></html>
"""

if __name__ == "__main__":
    main()

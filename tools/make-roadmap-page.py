"""Renders docs/TODO.md as the "4Stim Roadmap" page (one HTML file).

    python tools/make-roadmap-page.py <out.html> [commit] [date]

Sections are TODO.md's "##" headings, groups its "###" ones. Each top-level
"- " item is one entry; an item struck through at the start (~~...~~) counts
as done. An item whose body is a numbered list (the "Before Beta Release"
stages) shows its lead as a note and each numbered step as its own entry.
"""
import html, re, sys
import markdown

src = open("docs/TODO.md", encoding="utf-8").read().splitlines()
out = sys.argv[1]
commit = sys.argv[2] if len(sys.argv) > 2 else ""
date = sys.argv[3] if len(sys.argv) > 3 else ""

def md(text):
    text = re.sub(r"~~(.+?)~~", r"<del>\1</del>", text)
    return markdown.markdown(text)

def slug(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")

# Parse into sections -> blocks (("p", text) | ("h3", text) | ("li", text, done))
sections = []
cur = None
item = None
para = []

def flush_para():
    global para
    if para and cur is not None:
        cur["blocks"].append(("p", " ".join(para)))
    para = []

def flush_item():
    global item
    if item is not None and cur is not None:
        text = "\n".join(item)
        lines = item
        # numbered sub-steps?
        steps = [i for i, l in enumerate(lines) if re.match(r"\s+\d+\.\s", l)]
        if steps:
            lead = "\n".join(lines[: steps[0]])
            cur["blocks"].append(("note", lead[2:]))
            for n, s in enumerate(steps):
                end = steps[n + 1] if n + 1 < len(steps) else len(lines)
                body = " ".join(l.strip() for l in lines[s:end])
                body = re.sub(r"^\d+\.\s*", "", body)
                cur["blocks"].append(("li", body, body.startswith("~~")))
        else:
            body = text[2:]
            cur["blocks"].append(("li", body, body.lstrip().startswith("~~")))
    item = None

for line in src:
    if line.startswith("# "):
        continue
    if line.startswith("## "):
        flush_item(); flush_para()
        cur = {"title": line[3:].strip(), "blocks": []}
        sections.append(cur)
        continue
    if cur is None:
        continue
    if line.startswith("### "):
        flush_item(); flush_para()
        cur["blocks"].append(("h3", line[4:].strip()))
        continue
    if line.startswith("- "):
        flush_item(); flush_para()
        item = [line]
        continue
    if item is not None and (line.startswith(" ") or line.startswith("\t")) and line.strip():
        item.append(line)
        continue
    if not line.strip():
        flush_item(); flush_para()
        continue
    flush_item()
    para.append(line.strip())
flush_item(); flush_para()

total = sum(1 for s in sections for b in s["blocks"] if b[0] == "li")
done = sum(1 for s in sections for b in s["blocks"] if b[0] == "li" and b[2])

nav, body = [], []
for s in sections:
    items = [b for b in s["blocks"] if b[0] == "li"]
    d = sum(1 for b in items if b[2])
    sid = slug(s["title"])
    nav.append(f'<a href="#{sid}"><span>{html.escape(s["title"])}</span><span class="count">{d}/{len(items)}</span></a>')
    parts = [f'<section id="{sid}"><header><h2>{html.escape(s["title"])}</h2><span class="tally">{d} of {len(items)} done</span></header>']
    open_ul = False
    for b in s["blocks"]:
        if b[0] == "li":
            if not open_ul:
                parts.append("<ul>"); open_ul = True
            cls = ' class="done"' if b[2] else ""
            inner = md(b[1])
            inner = re.sub(r"^<p>(.*)</p>$", r"\1", inner.strip(), flags=re.S) if inner.count("<p>") == 1 else inner
            parts.append(f"<li{cls}>{inner}</li>")
            continue
        if open_ul:
            parts.append("</ul>"); open_ul = False
        if b[0] == "h3":
            parts.append(f"<h3>{html.escape(b[1])}</h3>")
        elif b[0] == "note":
            parts.append(f'<div class="note">{md(b[1])}</div>')
        else:
            parts.append(md(b[1]))
    if open_ul:
        parts.append("</ul>")
    parts.append("</section>")
    body.append("".join(parts))

pct = round(100 * done / total) if total else 0
stamp = " &middot; ".join(x for x in (commit, date) if x)
page = open(__file__.replace("make-roadmap-page.py", "roadmap-page.template.html"), encoding="utf-8").read()
page = (page.replace("{{NAV}}", "".join(nav)).replace("{{BODY}}", "".join(body))
        .replace("{{DONE}}", str(done)).replace("{{TOTAL}}", str(total)).replace("{{PCT}}", str(pct))
        .replace("{{STAMP}}", ("docs/TODO.md &middot; " + stamp) if stamp else "docs/TODO.md"))
open(out, "w", encoding="utf-8").write(page)
print(f"{done}/{total} done, {len(sections)} sections -> {out}")

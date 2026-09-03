const $ = (id) => document.getElementById(id);
const srcEl = $("src"), outEl = $("out"), astEl = $("ast"), statusEl = $("status"), runEl = $("run");

const DEFAULT_PROGRAM =
`LOAD UNIPROT "insulin AND organism_id:9606 AND reviewed:true" TOP 50

FIND proteins
WHERE length > 100
SORT BY length DESC
TOP 10
DISPLAY proteinid name organism length
`;

srcEl.value = DEFAULT_PROGRAM;

function setStatus(text, kind) {
  statusEl.textContent = text;
  statusEl.className = "status" + (kind ? " " + kind : "");
}

function renderResult(res, index) {
  const wrap = document.createElement("div");
  wrap.className = "result";

  const cap = document.createElement("div");
  cap.className = "cap";
  const suffix = res.returned === res.matched
    ? `${res.returned} row(s)`
    : `${res.returned} of ${res.matched} matched`;
  cap.textContent = `FIND #${index + 1} on ${res.entity} — ${suffix}`;
  wrap.appendChild(cap);

  if (res.kind === "count") {
    const n = document.createElement("div");
    n.className = "count";
    n.textContent = "COUNT = " + res.returned;
    wrap.appendChild(n);
    return wrap;
  }

  if (!res.rows.length) {
    const p = document.createElement("div");
    p.className = "hint";
    p.textContent = "No records matched.";
    wrap.appendChild(p);
    return wrap;
  }

  const scroll = document.createElement("div");
  scroll.className = "scroll";
  const table = document.createElement("table");

  const thead = document.createElement("thead");
  const hr = document.createElement("tr");
  for (const col of res.columns) {
    const th = document.createElement("th");
    th.textContent = col;
    hr.appendChild(th);
  }
  thead.appendChild(hr);
  table.appendChild(thead);

  const tbody = document.createElement("tbody");
  for (const row of res.rows) {
    const tr = document.createElement("tr");
    row.forEach((value, c) => {
      const td = document.createElement("td");
      const col = res.columns[c];
      if (col === "length") td.className = "num";
      else if (col === "sequence") td.className = "seq";
      // textContent, never innerHTML: these values come from UniProt.
      td.textContent = value;
      td.title = value;
      tr.appendChild(td);
    });
    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  scroll.appendChild(table);
  wrap.appendChild(scroll);
  return wrap;
}

function render(data) {
  outEl.replaceChildren();
  astEl.textContent = data.ast ? data.ast : "—";

  if (data.errors && data.errors.length) {
    const ul = document.createElement("ul");
    ul.className = "errors";
    for (const e of data.errors) {
      const li = document.createElement("li");
      li.textContent = e;
      ul.appendChild(li);
    }
    outEl.appendChild(ul);
  }

  if (data.sources && data.sources.length) {
    const div = document.createElement("div");
    div.className = "sources";
    div.textContent = "Loaded: " + data.sources.join("  |  ");
    outEl.appendChild(div);
  }

  (data.results || []).forEach((res, i) => outEl.appendChild(renderResult(res, i)));

  if (data.ok) {
    const rows = (data.results || []).reduce((n, r) => n + r.returned, 0);
    setStatus(`OK — ${data.statements} statement(s), ${rows} row(s).`, "ok");
  } else {
    setStatus(`Failed during ${data.stage}.`, "err");
  }
}

async function run() {
  runEl.disabled = true;
  setStatus("Running… (a first UniProt fetch can take a few seconds)", "busy");
  try {
    const resp = await fetch("/api/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ source: srcEl.value }),
    });
    render(await resp.json());
  } catch (err) {
    outEl.replaceChildren();
    setStatus("Could not reach the server: " + err.message, "err");
  } finally {
    runEl.disabled = false;
  }
}

runEl.addEventListener("click", run);
srcEl.addEventListener("keydown", (e) => {
  if ((e.ctrlKey || e.metaKey) && e.key === "Enter") { e.preventDefault(); run(); }
});

fetch("/api/examples")
  .then((r) => r.json())
  .then((data) => {
    const box = $("examples");
    for (const ex of data.examples || []) {
      const b = document.createElement("button");
      b.textContent = ex.name;
      b.addEventListener("click", () => { srcEl.value = ex.source; srcEl.focus(); });
      box.appendChild(b);
    }
  })
  .catch(() => {});

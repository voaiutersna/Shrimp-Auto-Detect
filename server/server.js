const express = require("express");
const Database = require("better-sqlite3");
const path = require("path");

// load .env if present (real environment variables take precedence)
if (require("fs").existsSync(path.join(__dirname, ".env"))) process.loadEnvFile(path.join(__dirname, ".env"));

const PORT = process.env.PORT || 3000;
// match with board
const API_KEY = process.env.API_KEY || "change-me-123"; 
const DB_FILE = path.resolve(__dirname, process.env.DB_FILE || "data.db");

// database
// https://tessl.io/regist-ry/tessl-labs/sqlite-node-best-practices/files/skills/sqlite-node-best-practices/SKILL.md
const db = new Database(DB_FILE);
db.pragma("journal_mode = WAL");db.exec(`
  CREATE TABLE IF NOT EXISTS readings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    time        INTEGER NOT NULL,          -- ms
    temperature REAL    NOT NULL,
    tds         REAL    NOT NULL
  );
  CREATE INDEX IF NOT EXISTS idx_readings_time ON readings(time);
`);

const insertReading = db.prepare(
  "INSERT INTO readings (time, temperature, tds) VALUES (?, ?, ?)"
);
const selectLatestN = db.prepare(
  "SELECT time, temperature, tds FROM readings ORDER BY id DESC LIMIT ?"
);
const selectRange = db.prepare(
  "SELECT time, temperature, tds FROM readings WHERE time BETWEEN ? AND ? ORDER BY time"
);
const selectStats = db.prepare(`
  SELECT COUNT(*) AS count,
         ROUND(AVG(tds), 1) AS avgTds, MIN(tds) AS minTds, MAX(tds) AS maxTds,
         ROUND(AVG(temperature), 2) AS avgTemp
  FROM readings WHERE time >= ?
`);

// web
const app = express();
app.use((req, res, next) => {
  res.set({
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Headers": "Content-Type, x-api-key",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  });
  if (req.method === "OPTIONS") return res.sendStatus(204);
  next();
});
app.use(express.json());
app.use(express.static(path.join(__dirname, "public")));

// api for update dataw for uno
app.post("/api/readings", (req, res) => {
  if (req.get("x-api-key") !== API_KEY) {
    return res.status(401).json({ error: "invalid api key" });
  }
  const temperature = Number(req.body.temperature);
  const tds = Number(req.body.tds);
  if (!Number.isFinite(temperature) || !Number.isFinite(tds)) {
    return res.status(400).json({ error: "temperature and tds must be numbers" });
  }
  insertReading.run(Date.now(), temperature, tds);
  res.json({ ok: true });
});

// for web to read
app.get("/api/readings", (req, res) => {
  if (req.query.from) {
    const from = Number(req.query.from);
    const to = Number(req.query.to) || Date.now();
    return res.json(selectRange.all(from, to));
  }
  const limit = Math.min(parseInt(req.query.limit) || 200, 20000);
  res.json(selectLatestN.all(limit).reverse());
});

app.get("/api/stats", (req, res) => {
  const hours = Number(req.query.hours) || 24;
  res.json(selectStats.get(Date.now() - hours * 3600 * 1000));
});

// CSV export
app.get("/api/export.csv", (req, res) => {
  res.setHeader("Content-Type", "text/csv; charset=utf-8");
  res.setHeader("Content-Disposition", "attachment; filename=readings.csv");
  res.write("time,temperature,tds\n");
  for (const r of db.prepare("SELECT time, temperature, tds FROM readings ORDER BY id").iterate()) {
    res.write(`${new Date(r.time).toISOString()},${r.temperature},${r.tds}\n`);
  }
  res.end();
});

app.listen(PORT, "0.0.0.0", () => {
  console.log(`Server running on http://localhost:${PORT}  (db: ${DB_FILE})`);
});

process.on("SIGINT", () => { db.close(); process.exit(0); });
process.on("SIGTERM", () => { db.close(); process.exit(0); });

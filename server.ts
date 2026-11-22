import express from 'express';
import fs from 'fs';
import path from 'path';
import { createServer as createViteServer } from 'vite';

const PORT = Number(process.env.PORT) || 3000;
const ROOT = process.cwd();

interface FileEntry {
  path: string;
  size: number;
}

function listAllowedFiles(): FileEntry[] {
  const entries: FileEntry[] = [];
  const rootFiles = ['Makefile', 'README.md'];
  for (const name of rootFiles) {
    const full = path.join(ROOT, name);
    if (fs.existsSync(full) && fs.statSync(full).isFile()) {
      entries.push({ path: name, size: fs.statSync(full).size });
    }
  }
  const cDir = path.join(ROOT, 'C');
  if (fs.existsSync(cDir) && fs.statSync(cDir).isDirectory()) {
    const files = fs.readdirSync(cDir).sort();
    for (const file of files) {
      const full = path.join(cDir, file);
      if (fs.statSync(full).isFile()) {
        entries.push({ path: `C/${file}`, size: fs.statSync(full).size });
      }
    }
  }
  return entries;
}

function resolveAllowedFile(relPath: string): string | null {
  const normalized = path.posix.normalize(relPath.replace(/\\/g, '/'));
  if (normalized.startsWith('..') || path.isAbsolute(normalized)) return null;
  if (normalized === 'Makefile' || normalized === 'README.md' || /^C\/[^/]+$/.test(normalized)) {
    const full = path.resolve(ROOT, normalized);
    if (full.startsWith(ROOT + path.sep) && fs.existsSync(full) && fs.statSync(full).isFile()) {
      return full;
    }
  }
  return null;
}

async function startServer() {
  const app = express();
  app.use(express.json());
  app.use('/docs', express.static(path.join(ROOT, 'docs')));

  app.get('/api/files', (_req, res) => {
    try {
      res.json({ files: listAllowedFiles() });
    } catch (err) {
      res.status(500).json({ error: err instanceof Error ? err.message : 'Unable to list files' });
    }
  });

  app.get('/api/file', (req, res) => {
    const rel = typeof req.query.path === 'string' ? req.query.path : '';
    const full = resolveAllowedFile(rel);
    if (!full) {
      res.status(404).json({ error: 'File not found' });
      return;
    }
    try {
      const content = fs.readFileSync(full, 'utf8');
      res.json({ path: rel, content });
    } catch (err) {
      res.status(500).json({ error: err instanceof Error ? err.message : 'Unable to read file' });
    }
  });

  const isProd = process.env.NODE_ENV === 'production';
  const distDir = path.join(ROOT, 'dist');

  if (isProd && fs.existsSync(distDir)) {
    app.use(express.static(distDir));
    app.get('*', (_req, res) => {
      res.sendFile(path.join(distDir, 'index.html'));
    });
  } else {
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: 'spa',
    });
    app.use(vite.middlewares);
  }

  app.listen(PORT, '0.0.0.0', () => {
    console.log(`Server listening on http://localhost:${PORT}`);
  });
}

startServer();

import { useEffect, useState } from 'react';
import { Camera, Code2, Cpu, FileCode2, FolderGit2, Layers, Monitor, Terminal, Volume2 } from 'lucide-react';

interface FileEntry {
  path: string;
  size: number;
}

const SCREENSHOTS = [
  {
    src: '/docs/screenshots/floating-bar-capture.svg',
    title: 'Floating Toolbar & Hollow Frame',
    caption: 'Hover-expanding bar with live region readout (W × H @ X, Y) and magnetic edge snapping.',
  },
  {
    src: '/docs/screenshots/window-targeting-magnifier.svg',
    title: 'UI Automation Targeter & 4× Magnifier',
    caption: 'Mouse wheel adjusts UI hierarchy depth; 4× loupe shows crosshair coordinates and RGB readout.',
  },
  {
    src: '/docs/screenshots/recording-and-clicks.svg',
    title: 'Active Video Recording & Click Halos',
    caption: 'Crimson recording state with live timer, Pause/Resume, and visual click rings.',
  },
  {
    src: '/docs/screenshots/settings-dark-mode.svg',
    title: 'Settings (Windows Dark / Light Theme)',
    caption: 'Syncs automatically with Windows AppsUseLightTheme including title bars and custom-painted controls.',
  },
];

export default function App() {
  const [files, setFiles] = useState<FileEntry[]>([]);
  const [activePath, setActivePath] = useState<string>('C/main.c');
  const [fileContent, setFileContent] = useState<string>('');
  const [loadingFile, setLoadingFile] = useState<boolean>(false);
  const [tab, setTab] = useState<'overview' | 'code'>('overview');

  useEffect(() => {
    fetch('/api/files')
      .then((r) => r.json())
      .then((data: { files?: FileEntry[] }) => {
        if (Array.isArray(data.files) && data.files.length > 0) {
          setFiles(data.files);
          if (!data.files.some((f) => f.path === activePath)) {
            setActivePath(data.files[0].path);
          }
        }
      })
      .catch(() => {});
  }, []);

  useEffect(() => {
    if (!activePath) return;
    setLoadingFile(true);
    fetch(`/api/file?path=${encodeURIComponent(activePath)}`)
      .then((r) => r.json())
      .then((data: { content?: string }) => {
        setFileContent(typeof data.content === 'string' ? data.content : '');
      })
      .catch(() => setFileContent(''))
      .finally(() => setLoadingFile(false));
  }, [activePath]);

  return (
    <div className="min-h-screen bg-zinc-950 text-zinc-100">
      <header className="border-b border-zinc-800 bg-zinc-950/90 sticky top-0 z-20 backdrop-blur">
        <div className="max-w-6xl mx-auto px-5 py-4 flex flex-wrap items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <div className="w-9 h-9 rounded-lg bg-blue-600/15 border border-blue-500/30 flex items-center justify-center text-blue-400">
              <Camera className="w-5 h-5" />
            </div>
            <div>
              <h1 className="text-lg font-semibold tracking-tight text-white">WinCapture</h1>
              <p className="text-xs text-zinc-400">Pure C Win32 Screen & Audio Recorder · MinGW / Direct3D 11 / Media Foundation</p>
            </div>
          </div>

          <nav className="flex items-center gap-2 bg-zinc-900 p-1 rounded-lg border border-zinc-800">
            <button
              type="button"
              onClick={() => setTab('overview')}
              className={`px-3 py-1.5 rounded-md text-xs font-medium transition-colors ${
                tab === 'overview' ? 'bg-zinc-800 text-white' : 'text-zinc-400 hover:text-zinc-200'
              }`}
            >
              Overview & Screenshots
            </button>
            <button
              type="button"
              onClick={() => setTab('code')}
              className={`px-3 py-1.5 rounded-md text-xs font-medium transition-colors flex items-center gap-1.5 ${
                tab === 'code' ? 'bg-zinc-800 text-white' : 'text-zinc-400 hover:text-zinc-200'
              }`}
            >
              <Code2 className="w-3.5 h-3.5" />
              C Source Browser
            </button>
          </nav>
        </div>
      </header>

      <main className="max-w-6xl mx-auto px-5 py-8">
        {tab === 'overview' ? (
          <div className="space-y-10">
            <section className="grid sm:grid-cols-2 lg:grid-cols-4 gap-4">
              <div className="p-4 rounded-xl bg-zinc-900/70 border border-zinc-800">
                <Monitor className="w-4 h-4 text-blue-400 mb-2" />
                <h2 className="text-sm font-medium text-white">DXGI + D3D11 GPU</h2>
                <p className="text-xs text-zinc-400 mt-1">Desktop Duplication with GPU BGRA→NV12 conversion and automatic GDI fallback.</p>
              </div>
              <div className="p-4 rounded-xl bg-zinc-900/70 border border-zinc-800">
                <Cpu className="w-4 h-4 text-emerald-400 mb-2" />
                <h2 className="text-sm font-medium text-white">H.264 / HEVC & QP</h2>
                <p className="text-xs text-zinc-400 mt-1">Media Foundation MP4 encoding with target bitrate or per-frame-type QP (I/P/B) and VFR.</p>
              </div>
              <div className="p-4 rounded-xl bg-zinc-900/70 border border-zinc-800">
                <Volume2 className="w-4 h-4 text-amber-400 mb-2" />
                <h2 className="text-sm font-medium text-white">WASAPI Audio Mix</h2>
                <p className="text-xs text-zinc-400 mt-1">System loopback + optional microphone resampled and mixed to AAC in real time.</p>
              </div>
              <div className="p-4 rounded-xl bg-zinc-900/70 border border-zinc-800">
                <Layers className="w-4 h-4 text-rose-400 mb-2" />
                <h2 className="text-sm font-medium text-white">UIA + 4× Magnifier</h2>
                <p className="text-xs text-zinc-400 mt-1">Target windows/controls by depth, draw custom regions, and adjust pixel-by-pixel.</p>
              </div>
            </section>

            <section className="space-y-4">
              <div className="flex items-center justify-between">
                <h2 className="text-base font-semibold text-white">Screenshots in Use</h2>
                <span className="text-xs text-zinc-400 font-mono">docs/screenshots/</span>
              </div>
              <div className="grid md:grid-cols-2 gap-5">
                {SCREENSHOTS.map((shot) => (
                  <figure key={shot.src} className="rounded-xl overflow-hidden bg-zinc-900 border border-zinc-800">
                    <img
                      src={shot.src}
                      alt={shot.title}
                      className="w-full aspect-video object-cover bg-zinc-950"
                      loading="lazy"
                    />
                    <figcaption className="p-4">
                      <div className="text-sm font-medium text-white">{shot.title}</div>
                      <p className="text-xs text-zinc-400 mt-1">{shot.caption}</p>
                    </figcaption>
                  </figure>
                ))}
              </div>
            </section>

            <section className="rounded-xl bg-zinc-900/70 border border-zinc-800 p-5 flex flex-wrap items-center justify-between gap-4">
              <div className="flex items-center gap-3">
                <Terminal className="w-5 h-5 text-zinc-300" />
                <div>
                  <h3 className="text-sm font-medium text-white">Build on Windows with MinGW-w64</h3>
                  <p className="text-xs text-zinc-400">Objects compile into <code className="text-zinc-200">obj/</code> and output <code className="text-zinc-200">WinCapture.exe</code> at the root.</p>
                </div>
              </div>
              <pre className="text-xs font-mono bg-zinc-950 border border-zinc-800 px-3.5 py-2 rounded-lg text-emerald-400">
                make fclean && make
              </pre>
            </section>
          </div>
        ) : (
          <div className="grid lg:grid-cols-[260px_1fr] gap-5 items-start">
            <aside className="rounded-xl bg-zinc-900 border border-zinc-800 overflow-hidden">
              <div className="px-3.5 py-2.5 border-b border-zinc-800 flex items-center gap-2 text-xs font-medium text-zinc-300">
                <FolderGit2 className="w-4 h-4 text-zinc-400" />
                Project Files
              </div>
              <div className="divide-y divide-zinc-800/60">
                {files.map((f) => (
                  <button
                    key={f.path}
                    type="button"
                    onClick={() => setActivePath(f.path)}
                    className={`w-full px-3.5 py-2 text-left text-xs flex items-center justify-between transition-colors ${
                      activePath === f.path
                        ? 'bg-blue-600/15 text-blue-300 font-medium'
                        : 'text-zinc-300 hover:bg-zinc-800/60'
                    }`}
                  >
                    <span className="flex items-center gap-2 truncate">
                      <FileCode2 className="w-3.5 h-3.5 shrink-0 text-zinc-500" />
                      <span className="truncate">{f.path}</span>
                    </span>
                    <span className="text-[11px] text-zinc-500 ml-2 shrink-0">
                      {(f.size / 1024).toFixed(1)} KB
                    </span>
                  </button>
                ))}
              </div>
            </aside>

            <section className="rounded-xl bg-zinc-900 border border-zinc-800 overflow-hidden">
              <div className="px-4 py-2.5 border-b border-zinc-800 flex items-center justify-between">
                <span className="text-xs font-mono text-zinc-200">{activePath}</span>
                <span className="text-[11px] text-zinc-500">
                  {loadingFile ? 'Loading…' : `${fileContent.split('\n').length} lines`}
                </span>
              </div>
              <pre className="p-4 text-xs font-mono text-zinc-200 overflow-x-auto max-h-[70vh] leading-relaxed">
                <code>{loadingFile ? 'Loading file…' : fileContent}</code>
              </pre>
            </section>
          </div>
        )}
      </main>
    </div>
  );
}

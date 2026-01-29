# csong plan

Objective
- Deliver a bottom-right, always-on-top, semi-transparent lyrics overlay
- Keep shared core logic across platforms (MPD, lyrics, caching, timing, BiDi)

Current state
- Terminal renderer is the active UI: `src/render/renderer.c`
- X11 and other windowing backends are stubs: `src/x11/*.c`
- Lyrics fetch/cache/parse pipeline is complete and used by `src/app/app.c`

Architecture direction
- Core (shared): MPD tracking, lyrics fetch/cache/parse, config, timing, BiDi
- UI backends (platform-specific): terminal (current), X11 overlay, Wayland layer-shell, Win32 overlay
- Keep rendering backend API minimal so `app_run` does not depend on platform details

Phase 0: Design + refactor
- Define a UI backend interface (init, draw, status, shutdown)
- Move terminal renderer behind that interface (no behavior change)
- Add config keys for UI backend selection and overlay options
  - Example: ui.backend, ui.opacity, ui.position, ui.margin, ui.font, ui.width, ui.height, ui.click_through
- Document platform constraints using `window_manager_interact` as reference

Phase 1: Linux X11 overlay (priority)
- Implement X11 window creation and positioning (bottom-right)
- Set always-on-top and opacity (`_NET_WM_WINDOW_OPACITY`)
- Support click-through using XFixes input shape
- Render text using Cairo + Pango or a lightweight text renderer
- Keep terminal backend as fallback

Phase 2: Linux Wayland overlay
- Implement layer-shell backend (wlr-layer-shell)
- Anchor to bottom-right with margins
- Handle compositor capability detection and fallback to terminal/XWayland
- Document compositor support caveats

Phase 3: Windows overlay
- Implement Win32 layered, topmost, click-through window
- Render text with DirectWrite (or GDI as a fallback)
- Add a platform-specific “now playing” source if MPD is not available

Risks and constraints
- Wayland overlay support varies by compositor
- Overlay behavior depends on compositor policies and security restrictions
- Rendering quality depends on font stack and text shaping libraries

Success criteria
- Stable overlay rendering without flicker
- Accurate lyric timing with LRC offsets and per-track offsets
- Graceful fallback when overlay backend is unavailable

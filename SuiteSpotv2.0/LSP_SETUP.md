# LSP Setup for SuiteSpot

This project is configured for **Language Server Protocol (LSP)** to provide intelligent code completion, navigation, and error detection for BakkesMod C++ development.

## Quick Start

### 1. Install clangd (Recommended)

**Option A: VS Code Extension (Easiest)**
1. Open VS Code
2. Install extension: `llvm-vs-code-extensions.vscode-clangd`
3. Reload window (`Ctrl+Shift+P` → "Reload Window")

**Option B: System-wide clangd**
```powershell
# Install via chocolatey
choco install llvm

# Or download from: https://github.com/clangd/clangd/releases
```

### 2. Reload Your Editor

After installing clangd:
- **VS Code**: `Ctrl+Shift+P` → "Reload Window"
- **Other editors**: Restart the editor

### 3. Verify It's Working

Open `SuiteSpot.cpp` and:
- Type `gameWrapper->` - you should see auto-completion for methods
- Hover over `ServerWrapper` - you should see type documentation
- `Ctrl+Click` on `GetCurrentGameState()` - should jump to BakkesMod SDK definition

---

## What's Configured

### Files Created

| File | Purpose |
|------|---------|
| `compile_commands.json` | Tells LSP how your code is compiled (include paths, flags) |
| `.clangd` | Clangd configuration (MSVC compatibility, diagnostics) |
| `.vscode/settings.json` | VS Code C++ settings |
| `.vscode/extensions.json` | Recommended extensions |
| `.vscode/tasks.json` | Build tasks (Ctrl+Shift+B) |
| `generate_compile_commands.ps1` | Script to regenerate compile database |

### Features Enabled

✅ **Auto-completion** for all BakkesMod wrappers
✅ **Go-to-definition** (Ctrl+Click) into SDK headers
✅ **Error detection** (missing null checks, incorrect types)
✅ **Hover documentation** for classes and methods
✅ **Inlay hints** (parameter names, deduced types)
✅ **Find all references** across project
✅ **Rename symbol** (F2)

---

## Troubleshooting

### "No IntelliSense suggestions"

1. Check clangd is running:
   - VS Code: Look for "clangd" in bottom status bar
   - Output panel: `View` → `Output` → Select "clangd"

2. Regenerate compile database:
   ```powershell
   .\generate_compile_commands.ps1
   ```

3. Restart clangd:
   - VS Code: `Ctrl+Shift+P` → "clangd: Restart language server"

### "Cannot find BakkesMod SDK headers"

The script auto-detects BakkesMod from registry. If it fails:

1. Check BakkesMod is installed
2. Verify SDK exists at: `%APPDATA%\bakkesmod\bakkesmod\bakkesmodsdk\include`
3. Manually edit `compile_commands.json` to fix paths if needed

### "Errors in pch.h"

Clangd may show false positives with precompiled headers. These don't affect actual compilation. To reduce noise:

- The `.clangd` config already suppresses PCH warnings
- Build with MSBuild to verify real errors: `Ctrl+Shift+B`

### Using Microsoft C++ Extension Instead

If you prefer Microsoft's IntelliSense over clangd:

1. Edit `.vscode/settings.json`:
   ```json
   // Comment out clangd settings
   // "C_Cpp.intelliSenseEngine": "disabled",

   // Uncomment Microsoft settings
   "C_Cpp.intelliSenseEngine": "default",
   "C_Cpp.default.compileCommands": "${workspaceFolder}/compile_commands.json",
   ```

2. Install: `ms-vscode.cpptools` extension
3. Reload window

---

## Maintenance

### When to Regenerate compile_commands.json

Run `.\generate_compile_commands.ps1` when you:
- Add new `.cpp` or `.h` files to the project
- Change include paths in `.vcxproj`
- Update BakkesMod SDK version
- Change compiler flags

You can also run via VS Code task:
- `Ctrl+Shift+P` → "Tasks: Run Task" → "Rebuild compile_commands.json"

---

## BakkesMod-Specific LSP Benefits

### Type-Safe Wrapper Checks

```cpp
// LSP detects missing null check ⚠️
ServerWrapper server = gameWrapper->GetCurrentGameState();
server.GetBall();  // ❌ LSP warning: server might be null

// LSP approves ✅
if (server) {
    server.GetBall();  // ✅ Safe
}
```

### Event Hook Signature Validation

```cpp
// LSP detects wrong caller type ⚠️
gameWrapper->HookEventWithCallerPost<CarWrapper>(  // ❌ Wrong type!
    "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
    bind(&SuiteSpot::OnMatchEnded, this, _1, _2, _3));

// LSP approves ✅
gameWrapper->HookEventWithCallerPost<ServerWrapper>(  // ✅ Correct
    "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
    bind(&SuiteSpot::OnMatchEnded, this, _1, _2, _3));
```

### Auto-Complete for BakkesMod API

Just type `gameWrapper->` or `server.` and LSP will show all available methods with documentation!

---

## Alternative Editors

### Visual Studio 2022

VS 2022 has built-in IntelliSense that reads `.vcxproj` directly. No setup needed!

### Neovim / Vim

Install `clangd` and configure with:
```lua
-- Using nvim-lspconfig
require('lspconfig').clangd.setup{
  cmd = {
    "clangd",
    "--compile-commands-dir=" .. vim.fn.getcwd(),
    "--background-index",
  }
}
```

### CLion

CLion can import MSBuild projects or use `compile_commands.json`:
1. `File` → `Open` → Select `compile_commands.json`
2. CLion will auto-configure

---

## More Information

- **clangd docs**: https://clangd.llvm.org/
- **BakkesMod SDK**: https://github.com/bakkesmodorg/BakkesModSDK
- **LSP Specification**: https://microsoft.github.io/language-server-protocol/

---

**Enjoy your enhanced BakkesMod development experience!** 🚀

# Quick Start Guide - SuiteSpot Development

## Initial Setup

### 1. Clone and Build Locally

```bash
git clone https://github.com/YOUR_ORG/SuiteSpotProject.git
cd SuiteSpotProject
```

**Build with Visual Studio 2022:**
- Open `SuiteSpotv2.0\SuiteSpot.sln`
- Select **Release | x64**
- Press **Ctrl+Shift+B** to build

**Output:** `SuiteSpotv2.0\plugins\SuiteSpot.dll`

### 2. Test Locally

Install in BakkesMod and test:
```
plugin load suitespot
plugin show suitespot
```

## Making Changes

### 1. Create a Feature Branch

```bash
git checkout -b feature/your-feature-name
```

### 2. Make Code Changes

Edit files in `SuiteSpotv2.0/`:
- `SuiteSpot.h/cpp` - Core plugin logic
- `Source.cpp` - ImGui settings UI
- `MapList.cpp` - Map data
- etc.

**Important:** Read `CLAUDE.md` for development patterns before coding.

### 3. Build Locally

```bash
# Visual Studio: Ctrl+Shift+B
# Or command line:
msbuild SuiteSpotv2.0\SuiteSpot.sln /p:Configuration=Release /p:Platform=x64
```

### 4. Commit Changes

```bash
git add .
git commit -m "feat: add new feature description"
```

### 5. Push and Create Pull Request

```bash
git push origin feature/your-feature-name
```

Then create a pull request on GitHub. The CI/CD pipeline will:
- ✅ Automatically build your changes
- ✅ Verify the DLL is generated
- ✅ Show build status in the PR

## Releasing

### 1. Update Version

Edit `SuiteSpotv2.0/version.h`:

```cpp
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 1       // ← Increment
#define VERSION_BUILD 314     // ← Increment
```

### 2. Commit and Tag

```bash
git add SuiteSpotv2.0/version.h
git commit -m "Bump version to 1.0.1"
git tag -a v1.0.1 -m "Release version 1.0.1"
git push origin master v1.0.1
```

### 3. GitHub Releases

The CI/CD pipeline automatically:
- Detects the `v1.0.1` tag
- Builds the plugin
- Creates a GitHub Release
- Uploads `SuiteSpot.dll` as downloadable asset

Users can download from: `https://github.com/YOUR_ORG/SuiteSpotProject/releases`

## Development Workflow

```
Local Branch → Commit → Push → PR → CI/CD Tests → Merge
                                      ↓
                               Build artifacts
```

On merge to `master`:
- Build runs automatically
- Artifacts available for 30 days

On version tag (`v*.*.*`):
- Build + Release creation
- DLL available for users

## Key Commands

```bash
# Check git status
git status

# View recent commits
git log --oneline -10

# View your branch
git branch

# Switch branches
git checkout feature-name

# Update main code from GitHub
git pull origin master

# Push your changes
git push origin feature-name
```

## Troubleshooting

### Build Fails Locally
- Ensure Visual Studio 2022 is installed
- Check `SuiteSpotv2.0/SuiteSpot.sln` exists
- Verify BakkesMod SDK in `BakkesModSDK/`

### Build Fails in CI/CD
- Check workflow logs: GitHub Actions tab
- Review build error in workflow output
- Common: Missing dependencies in `.vcxproj`

### DLL Not Loading in BakkesMod
- Ensure correct path: BakkesMod plugins folder
- Check console: `plugin show suitespot`
- Verify x64 architecture (not x86)

## Documentation

- **README.md** - Project overview
- **DEPLOYMENT.md** - Release procedures
- **CLAUDE.md** - Development guidelines
- **DECISIONS.md** - Architecture decisions
- **SuiteSpotDocuments/instructions/** - API reference

## Next Steps

1. ✅ Clone repository
2. ✅ Build locally
3. ✅ Read CLAUDE.md for development patterns
4. ✅ Create feature branch
5. ✅ Make changes
6. ✅ Push to GitHub
7. ✅ Create pull request
8. ✅ CI/CD validates build
9. ✅ Merge and create release tag when ready

Happy coding! 🚀

# Deployment Guide - SuiteSpot Plugin

This guide explains how to deploy SuiteSpot using automated releases.

## Automated Release Pipeline

The GitHub Actions CI/CD pipeline automatically:

1. **Builds on every push** to `master` or `main` branches
2. **Creates releases** when you push a version tag (e.g., `v1.0.1`)
3. **Uploads artifacts** for 30 days

## Creating a Release

### Step 1: Update Version

Edit `SuiteSpotv2.0/version.h`:

```cpp
#pragma once
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 1       // ← Update patch number
#define VERSION_BUILD 313     // ← Increment build number
```

### Step 2: Commit Changes

```bash
git add SuiteSpotv2.0/version.h
git commit -m "Bump version to 1.0.1"
```

### Step 3: Create a Git Tag

```bash
git tag -a v1.0.1 -m "Release version 1.0.1"
git push origin v1.0.1
```

The workflow will:
- Automatically detect the tag
- Build the plugin
- Create a GitHub Release with:
  - `SuiteSpot.dll` as downloadable asset
  - Build info and installation instructions
  - Link to recent changes (DECISIONS.md)

### Step 4: Publish Release

Go to: `https://github.com/YOUR_ORG/SuiteSpotProject/releases`

The release will be automatically created with your DLL attached.

## Version Numbering

Follow **Semantic Versioning** (MAJOR.MINOR.PATCH):

- **MAJOR**: Breaking changes (e.g., new plugin architecture)
- **MINOR**: New features (e.g., new map type support)
- **PATCH**: Bug fixes (e.g., thread safety fix)

Example progression:
```
1.0.0 → 1.0.1 (bugfix) → 1.1.0 (feature) → 2.0.0 (breaking)
```

## Downloading Releases

Users can download pre-built DLLs from:

**GitHub Releases Page:**
```
https://github.com/YOUR_ORG/SuiteSpotProject/releases
```

**Installation:**
1. Download `SuiteSpot.dll` 
2. Place in BakkesMod plugins folder
3. Run in Rocket League console: `plugin load suitespot`

## Build Artifacts

Each push to `master`/`main` creates artifacts available for 30 days:

**Access via:**
- GitHub Actions → Recent workflow → Artifacts section
- Useful for testing intermediate builds

## Rollback

If a release has issues:

1. **Local rollback:**
   ```bash
   git revert v1.0.1
   git push origin master
   ```

2. **Tag reassignment** (use carefully):
   ```bash
   git tag -d v1.0.1
   git push origin :v1.0.1
   git tag v1.0.1 COMMIT_SHA
   git push origin v1.0.1
   ```

## Troubleshooting

### Build Failed in CI/CD

Check the workflow run logs:
1. Go to GitHub → Actions tab
2. Click the failed workflow
3. Review build step output

Common issues:
- Missing NuGet packages: Check `SuiteSpot.vcxproj` dependencies
- Path issues: Ensure solution file exists at `SuiteSpotv2.0/SuiteSpot.sln`
- SDK missing: Verify BakkesMod SDK in `BakkesModSDK/`

### Release Not Created

- Verify git tag format: `git tag -l` shows tags
- Check workflow file: `.github/workflows/build.yml` is correct
- Confirm tag matches `v*.*.*` pattern (e.g., `v1.0.1`, NOT `release-1.0.1`)

## CI/CD Configuration

Workflow file: `.github/workflows/build.yml`

Key settings:
- **Triggers:** Push, PR, manual (`workflow_dispatch`)
- **Platform:** Windows x64
- **Retention:** 30 days for build artifacts
- **Release condition:** Tag starts with `v`

## Next Steps

1. Push changes: `git push origin master`
2. Verify build succeeds in Actions tab
3. Create release tag: `git tag -a v1.0.0 -m "Initial release"`
4. Push tag: `git push origin v1.0.0`
5. Check Releases page for published artifact

---

**See also:**
- `DECISIONS.md` - Recent changes and improvements
- `README.md` - Project overview
- `CLAUDE.md` - Development guidelines

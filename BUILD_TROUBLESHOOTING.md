# CI/CD and Build Troubleshooting

This guide helps resolve common build and deployment issues.

## Local Build Issues

### Issue: "Cannot find solution file"

**Error:** `SuiteSpot.sln not found`

**Solution:**
```bash
# Verify file exists
ls SuiteSpotv2.0/SuiteSpot.sln

# Build from correct directory
cd SuiteSpotv2.0
msbuild SuiteSpot.sln /p:Configuration=Release /p:Platform=x64
```

### Issue: "MSBuild not found"

**Error:** `'msbuild' is not recognized`

**Solution:**
1. Install Visual Studio 2022
2. Add to PATH: `C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin`

Or use full path:
```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe' `
  SuiteSpotv2.0\SuiteSpot.sln /p:Configuration=Release /p:Platform=x64
```

### Issue: "BakkesMod SDK not found"

**Error:** `Cannot find include file bakkesmod/plugin/bakkesmodplugin.h`

**Solution:**
1. Ensure `BakkesModSDK/` exists in project root
2. Check `SuiteSpotv2.0/BakkesMod.props` includes correct paths
3. Verify environment variable: `%BAKKESMOD_PLUGIN_API%`

### Issue: "Platform not available"

**Error:** `Platform "x64" not supported`

**Solution:**
- Use exact case: `/p:Platform=x64` (not `x86` or `X64`)
- Verify .vcxproj supports platform:
  ```xml
  <ProjectConfiguration Include="Release|x64">
  ```

## CI/CD Pipeline Issues

### Issue: Workflow not triggering

**Check:**
1. Is `.github/workflows/build.yml` in correct location?
   ```
   .github/
   └── workflows/
       └── build.yml
   ```

2. Is file syntax valid? (Check GitHub Actions UI)

3. Does commit match trigger conditions?
   - Pushed to `master` or `main` branch?
   - PR against `master` or `main`?
   - Tag format matches `v*.*.*`?

### Issue: "Build succeeded but no artifacts"

**Cause:** DLL not found at expected path

**Solution:**
1. Check actual build output location:
   ```bash
   find . -name "SuiteSpot.dll" -type f
   ```

2. Verify .vcxproj output directory:
   ```xml
   <OutDir>$(SolutionDir)plugins\</OutDir>
   ```

3. Compare local vs CI/CD paths

### Issue: "Upload artifact failed"

**Error:** `Upload failed with exit code 1`

**Solution:**
1. Verify build produced DLL
2. Check `build-artifacts/` directory exists
3. Ensure DLL is not locked by running process
4. Check disk space on runner

### Issue: NuGet restore fails in CI/CD

**Error:** `Package restore failed`

**Solution:**
1. Verify project has `packages.config` or `.csproj` with dependencies
2. Add to workflow before build:
   ```yaml
   - name: Restore NuGet packages
     run: nuget restore "${{ env.SOLUTION_PATH }}"
   ```
3. Check internet connectivity in runner

## Release Issues

### Issue: Release not created from tag

**Error:** No release appears after pushing tag

**Check:**
1. Tag format: Must be `v*.*.*` (e.g., `v1.0.1`)
   ```bash
   # Correct
   git tag v1.0.1
   
   # Incorrect
   git tag release-1.0.1
   git tag 1.0.1
   ```

2. Workflow runs for tag:
   ```yaml
   on:
     tags: [ 'v*.*.*' ]
   ```

3. GitHub token permissions:
   - Check repository settings
   - Ensure GITHUB_TOKEN has release creation rights

### Issue: Version not detected in release

**Error:** Release shows empty version info

**Check:**
1. `version.h` format is correct:
   ```cpp
   #define VERSION_MAJOR 1
   #define VERSION_MINOR 0
   #define VERSION_PATCH 1
   #define VERSION_BUILD 312
   ```

2. Regex in workflow matches:
   ```powershell
   [regex]::Match($content, 'VERSION_MAJOR\s+(\d+)')
   ```

3. File encoding is UTF-8

## Debugging

### Enable Verbose Build Output

Local build:
```bash
msbuild SuiteSpotv2.0/SuiteSpot.sln /v:detailed
```

CI/CD workflow - modify `.github/workflows/build.yml`:
```yaml
- name: Build solution
  run: |
    msbuild "${{ env.SOLUTION_PATH }}" `
      /p:Configuration=${{ env.BUILD_CONFIG }} `
      /p:Platform=${{ env.BUILD_PLATFORM }} `
      /v:detailed  # Changed from 'minimal'
```

### Inspect Workflow Logs

1. Go to repository → Actions tab
2. Click workflow run
3. Expand job steps
4. Review build output

### Check File Permissions

On Windows:
```powershell
# Check if file is locked
Get-Process | Where-Object { $_.Handles | Select-String "SuiteSpot.dll" }

# Release locked files
Stop-Process -Name "UE4Editor" -Force
```

### Validate Solution Structure

```bash
# Check files exist
Test-Path "SuiteSpotv2.0/SuiteSpot.sln"
Test-Path "SuiteSpotv2.0/SuiteSpot.vcxproj"
Test-Path ".github/workflows/build.yml"

# Check for syntax errors
Get-Content ".github/workflows/build.yml" | sls "^name:"
```

## Performance Issues

### Build Takes Too Long

**Optimize:**
1. Use incremental builds (don't clean every time)
2. Enable parallel builds:
   ```bash
   msbuild /m /p:Configuration=Release /p:Platform=x64
   ```
3. Check for large files in output directory

### CI/CD Pipeline Slow

**Optimize:**
1. Remove unnecessary jobs
2. Cache dependencies (if using vcpkg)
3. Use smaller runners if available
4. Remove retention or reduce from 30 days

## Getting Help

1. **Check logs:**
   - Local: Build output in Visual Studio Output window
   - CI/CD: GitHub Actions workflow logs

2. **Review documentation:**
   - `CLAUDE.md` - Development patterns
   - `DECISIONS.md` - Architecture decisions
   - `SuiteSpotDocuments/` - API reference

3. **Verify setup:**
   - Run `git status` to check repository state
   - Verify `SuiteSpotv2.0/version.h` is readable
   - Confirm BakkesMod SDK is present

4. **Clean rebuild:**
   ```bash
   # Remove build artifacts
   Remove-Item SuiteSpotv2.0/build -Recurse -Force
   Remove-Item SuiteSpotv2.0/plugins -Recurse -Force
   
   # Rebuild
   msbuild SuiteSpotv2.0/SuiteSpot.sln /p:Configuration=Release /p:Platform=x64
   ```

---

**Quick Links:**
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [MSBuild Reference](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild)
- [BakkesMod SDK](https://github.com/bakkesmodorg/BakkesModSDK)

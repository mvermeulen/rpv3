# GitHub Release Checklist

This document outlines the necessary steps to prepare for a GitHub release of the RPV3 Kernel Tracer project.

## Pre-Release Checklist

### 1. Documentation Review

#### Required Files
- [x] **README.md** - Comprehensive project documentation
  - [x] Clear project description and overview
  - [x] Installation instructions
  - [x] Usage examples with all features
  - [x] Troubleshooting section
  - [x] Project structure documented
- [x] **CHANGELOG.md** - Version history and changes
  - [x] All versions documented
  - [x] Changes categorized (Added, Changed, Fixed, etc.)
  - [x] Dates included for each release
- [x] **LICENSE** - Legal license file
  - [x] MIT License added
  - [x] LICENSE file in repository root
  - [ ] Update README with license badge (optional)
- [ ] **CONTRIBUTING.md** - Contribution guidelines
  - [ ] Code style guidelines
  - [ ] Pull request process
  - [ ] Issue reporting guidelines
  - [ ] Development setup instructions
- [x] **TODO.md** - Future roadmap and planned features
- [ ] **CODE_OF_CONDUCT.md** - Community guidelines (optional but recommended)

#### Documentation Quality
- [x] All examples tested and working
- [x] Version numbers consistent across all files
- [x] No broken internal links
- [ ] No TODO/FIXME comments in documentation
- [x] All features documented with examples

### 2. Code Quality

#### Build System
- [x] Clean build succeeds without warnings
- [x] Both Makefile and CMake builds work
- [x] All targets build successfully (C++, C, examples, utils)
- [x] Debug build works (`make debug`)
- [x] Clean target works properly (`make clean`)

#### Code Review
- [ ] No debug print statements left in code
- [ ] No commented-out code blocks
- [ ] Consistent code formatting
- [ ] All compiler warnings addressed
- [ ] Memory leaks checked (valgrind/sanitizers)
- [ ] Thread safety verified for concurrent usage

#### Version Consistency
- [x] Version in `rpv3_options.h` is correct (currently 1.5.0)
- [x] Version in `CHANGELOG.md` matches
- [x] Version in `README.md` examples matches
- [x] Version in `TODO.md` matches
- [x] Version in `CMakeLists.txt` matches (verified: 1.5.0)

### 3. Testing

#### Test Suite
- [x] All unit tests pass
- [x] All integration tests pass
- [x] All regression tests pass
- [x] Counter collection tests pass (or gracefully degrade)
- [x] CSV output tests pass
- [x] RocBLAS integration tests pass
- [x] README examples verified

#### Manual Testing
- [ ] Test on clean system (fresh clone)
- [ ] Test with different ROCm versions
- [ ] Test on different GPU architectures (if available)
- [ ] Test all command-line options
- [ ] Test error handling and edge cases
- [ ] Verify output formats (human-readable, CSV, timeline)

#### Performance
- [ ] No significant performance regressions
- [ ] Overhead is acceptable for production use
- [ ] Memory usage is reasonable

### 4. Repository Cleanup

#### Git Status
- [ ] All changes committed
- [ ] No uncommitted changes in working directory
- [ ] No untracked files that should be committed
- [ ] `.gitignore` is comprehensive and up-to-date
- [ ] No sensitive data in repository history

#### File Organization
- [x] Build artifacts not in repository (`.gitignore` configured)
- [x] Test artifacts properly ignored
- [ ] No temporary files committed
- [ ] All source files have proper headers/comments

### 5. GitHub Repository Setup

#### Repository Settings
- [ ] Repository description is clear and concise
- [ ] Topics/tags added for discoverability (rocm, profiling, hip, amd, gpu)
- [ ] Repository visibility set correctly (public/private)
- [ ] Default branch set (usually `main` or `master`)

#### GitHub Features
- [ ] Issues enabled
- [ ] Wiki enabled (optional)
- [ ] Discussions enabled (optional)
- [ ] Projects enabled (optional)
- [ ] Security policy added (SECURITY.md)

#### README Badges (Optional but Recommended)
- [ ] License badge
- [ ] Build status badge (if CI/CD setup)
- [ ] Version badge
- [ ] ROCm compatibility badge

### 6. Release Preparation

#### Version Tagging
- [ ] Decide on version number (follow semantic versioning)
- [ ] Update version in all files (see Version Consistency above)
- [ ] Update CHANGELOG.md with release date
- [ ] Commit version bump changes
- [ ] Create git tag: `git tag -a v1.5.0 -m "Release v1.5.0"`
- [ ] Push tag: `git push origin v1.5.0`

#### Release Notes
- [ ] Prepare release notes based on CHANGELOG.md
- [ ] Highlight major features and improvements
- [ ] List breaking changes (if any)
- [ ] Include upgrade instructions (if needed)
- [ ] Add known issues/limitations
- [ ] Link to documentation

#### Release Assets
- [ ] Consider providing pre-built binaries (optional)
- [ ] Include installation script (optional)
- [ ] Package source tarball (GitHub does this automatically)
- [ ] Include checksums for verification (optional)

### 7. Post-Release

#### Verification
- [ ] Download release tarball and verify it extracts correctly
- [ ] Test installation from release tarball
- [ ] Verify all links in release notes work
- [ ] Check that release appears correctly on GitHub

#### Communication
- [ ] Announce release (if applicable)
- [ ] Update any external documentation
- [ ] Notify users of breaking changes
- [ ] Update project website (if exists)

#### Next Steps
- [ ] Create milestone for next version
- [ ] Move incomplete TODO items to next milestone
- [ ] Update TODO.md with new priorities

---

## Current Status for v1.5.0 Release

### ✅ Completed - READY FOR RELEASE
- Comprehensive README with all features documented
- Complete CHANGELOG with version history
- Full test suite (unit, integration, regression)
- Both C and C++ implementations working
- All major features implemented and tested
- Version numbers consistent (1.5.0) across all files
- **LICENSE file added** - MIT License ✅
- **CMakeLists.txt version verified** - 1.5.0 ✅

### 📋 Optional Improvements (Can Do Post-Release)
- **CONTRIBUTING.md** - Community contribution guidelines
- **Manual testing on clean system** - Additional validation
- **GitHub repository setup** - Topics, description, badges
- **README badges** - License, version, build status

### � Ready to Release!

All **critical items** are complete. The project is ready for v1.5.0 release.

**Next Steps:**
1. Commit LICENSE file: `git add LICENSE && git commit -m "Add MIT License"`
2. Create git tag: `git tag -a v1.5.0 -m "Release v1.5.0"`
3. Push tag: `git push origin v1.5.0`
4. Create GitHub release with notes from CHANGELOG.md

---

## Quick Release Commands

```bash
# 1. Verify everything is committed
git status

# 2. Run full test suite
make clean
make test

# 3. Verify version consistency
grep -r "1.5.0" rpv3_options.h CHANGELOG.md README.md TODO.md CMakeLists.txt

# 4. Create and push tag
git tag -a v1.5.0 -m "Release v1.5.0 - CSV Summary Tool and RocBLAS Integration"
git push origin v1.5.0

# 5. Create GitHub release
# Go to: https://github.com/YOUR_USERNAME/rpv3/releases/new
# - Select tag: v1.5.0
# - Release title: "RPV3 v1.5.0 - CSV Summary Tool and RocBLAS Integration"
# - Description: Copy from CHANGELOG.md v1.5.0 section
# - Publish release
```

---

**Last Updated**: 2025-11-28  
**Target Release Version**: 1.5.0

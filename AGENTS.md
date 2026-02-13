# Agent Instructions

This project uses **bd** (beads) for issue tracking. Run `bd onboard` to get started.

## Quick Reference

```bash
bd ready              # Find available work
bd show <id>          # View issue details
bd update <id> --status in_progress  # Claim work
bd close <id>         # Complete work
bd sync               # Sync with git
```

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
    ```bash
    git pull --rebase
    bd sync
    git push
    git status  # MUST show "up to date with origin"
    ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds

## Installation Path Requirements

**⚠️ CRITICAL**: Always ensure programs install to `/usr` directory, NOT `/usr/local`!

### CMake Configuration Requirements

When adding or modifying CMake install targets, follow these rules:

1. **DO NOT use hardcoded paths like `/usr/local/lib` or `/usr/local/share`**
2. **Use CMAKE_INSTALL_PREFIX and CMAKE_INSTALL_LIBDIR variables**
3. **Standard fcitx5 installation locations:**
   - Plugin libraries: `${CMAKE_INSTALL_LIBDIR}/fcitx5/`
   - Shared data: `${CMAKE_INSTALL_DATAROOTDIR}/fcitx5/`
   - Config files: `${CMAKE_INSTALL_FULL_DATADIR}/fcitx5/`

4. **Preferred CMake install patterns:**
   ```cmake
   # Correct
   install(TARGETS target DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
   
   # INCORRECT
   install(TARGETS target DESTINATION "/usr/local/lib/fcitx5")
   ```

5. **When running CMake:**
   ```bash
   # Correct - uses system default (/usr)
   cmake -DCMAKE_BUILD_TYPE=Release ..
   
   # INCORRECT - forces /usr/local
   cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
   ```

### Important Note about `add_fcitx5_addon` Macro

**Known Issue**: The `add_fcitx5_addon` macro (defined in fcitx5 Core) internally hardcodes `CMAKE_INSTALL_PREFIX="/usr/local"`. This cannot be modified directly without modifying fcitx5 itself.

**Workaround**: Override the prefix by explicitly setting `CMAKE_INSTALL_PREFIX=/usr` when running CMake:
```bash
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ..
```

**Current Status**: The `im/voiceinput` module correctly uses `${CMAKE_INSTALL_LIBDIR}/fcitx5/` in its install command. However, without the `CMAKE_INSTALL_PREFIX=/usr` override, it will install to `/usr/local` due to the macro's hardcoded value.

### Verification

After installation, verify with:
```bash
# Check library location
pkg-config --variable=libdir fcitx5 | grep -v "/usr/local"

# Check plugin registration
fcitx5-remote -d
```


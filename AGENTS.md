# LS Factory development

Use the existing checkout; cloud tasks are isolated and do not need a Git worktree.

This is a native C++20/JUCE desktop application under `OpenDAWForge`, not a web app. Read its README and `docs/STATUS.md` for the implemented scope. In the cloud, source `/workspace/.tools/activate.sh`; build with the debug preset, run its nonempty Catch2 suite and run `tools/validate-cloud.sh` for UI and actual VST3 integration checks. Keep audio callbacks free of host-side allocation, file I/O and locks.

The user has explicitly authorized ongoing deployment and releases for large milestones in this repository. After validated changes, commit and push the intended code to `main`; the desktop workflow produces native app artifacts. For a large milestone, update the CMake/application versions and release notes, validate a Release build and push a new immutable semantic version tag. The tag workflow publishes a GitHub prerelease only after all target build jobs pass. Never move an already published release tag. Do not claim CI or release publication succeeded without evidence.

Include downloadable original PNG screenshots in release packages. Local workspace links and sandbox image links did not work in the user's viewer; provide GitHub file/download links or release asset links. `screenshots/` is a published set of actual UI captures.

Use existing platform Git authentication and injected credential bindings. Do not print credentials or request replacement tokens just because an operation fails. Publication of the cloud environment itself is a separate product action; saved configuration drafts do not prove publication.

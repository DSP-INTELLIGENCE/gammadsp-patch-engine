# Manual push commands

Copy the ZIP contents into the root of `DSP-INTELLIGENCE/gammadsp-patch-engine`, then run:

```bash
cd ~/Downloads/gammadsp-patch-engine

git status --short
git add README.md docs
git commit -m "Add GammaDSP PatchEngine extraction docs"
git push origin main
```

Inspect:

```bash
git log --oneline -3
git status --short
find docs -maxdepth 1 -type f | sort
```

Next branch after docs:

```bash
git checkout -b extract/minimal-patch-engine-runtime-01
```

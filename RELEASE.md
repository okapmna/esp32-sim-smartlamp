# Panduan Update & Release

## Alur Perubahan Firmware

```
FIX/feat di dev → commit → push ke main/master
                                    ↓
                   CI build otomatis (tanpa flag versi)
                                    ↓
                   Artifact: smartlamp-dev-<sha>.bin
```

## Alur Release

```
Buat tag versi → push tag → buat GitHub Release → publish
                                                    ↓
                         CI build dengan CURRENT_VERSION=<tag>
                                    ↓
                   Upload artifact ke Release
                                    ↓
                   Deploy ke OTA server (jika dikonfigurasi)
```

## Cara Release

```bash
# 1. Tentukan versi (ikuti semver, misal v1.2.0)
git tag v1.2.0

# 2. Push tag ke GitHub
git push origin v1.2.0

# 3. Buat Release (pilih salah satu)
# Via CLI:
gh release create v1.2.0 --generate-notes

# Via UI: GitHub → Releases → Draft a new release → pilih tag → Publish
```

## Catatan

- Tag harus diawali `v` (contoh: `v1.0.0`). Awalan `v` akan otomatis di-strip oleh CI.
- Saat release di-publish, CI akan:
  - Build dengan `-DCURRENT_VERSION="<versi>"`
  - Upload firmware ke GitHub Release
  - Deploy ke OTA server (via curl, memerlukan secrets `OTA_API_KEY` dan `OTA_UPLOAD_URL`)
- Commit langsung ke `main`/`master` akan memicu build dengan versi `dev-<sha>` (tanpa deploy OTA).

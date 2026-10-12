#!/usr/bin/env python3
"""Package a compiled iOS app for re-signing with a sideloading tool."""

import argparse
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--entitlements", type=Path)
    args = parser.parse_args()
    app = args.app.resolve()
    output = args.output.resolve()
    if not (app / "Info.plist").is_file():
        parser.error("The app must contain Info.plist.")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent, prefix="ipa-stage-") as temporary:
        stage = Path(temporary)
        bundle = stage / "Payload" / app.name
        shutil.copytree(app, bundle)
        catalog = bundle / "Assets.xcassets"
        assets_info = stage / "assets-info.plist"
        environment = dict(os.environ, TMPDIR=str(stage) + "/")
        if catalog.is_dir():
            subprocess.run([
                "xcrun", "actool", str(catalog), "--compile", str(bundle),
                "--platform", "iphoneos", "--minimum-deployment-target", "15.0",
                "--target-device", "iphone", "--target-device", "ipad",
                "--app-icon", "AppIcon", "--compress-pngs",
                "--output-partial-info-plist", str(assets_info),
            ], check=True, env=environment)
            shutil.rmtree(catalog)
        info_path = bundle / "Info.plist"
        info = plistlib.loads(info_path.read_bytes())
        if assets_info.exists():
            info.update(plistlib.loads(assets_info.read_bytes()))
        for key in ("CFBundleVersion", "CFBundleShortVersionString"):
            info[key] = info[key].removeprefix("v")
        info.update({
            "MinimumOSVersion": "15.0",
            "UIDeviceFamily": [1, 2],
            "CFBundleSupportedPlatforms": ["iPhoneOS"],
            "LSRequiresIPhoneOS": True,
        })
        if not (bundle / info["CFBundleExecutable"]).is_file():
            parser.error("The app executable is missing.")
        info_path.write_bytes(plistlib.dumps(info, fmt=plistlib.FMT_BINARY))
        subprocess.run(["xattr", "-cr", str(bundle)], check=True)
        signing = ["codesign", "--force", "--sign", "-", "--timestamp=none"]
        if args.entitlements:
            signing.extend(["--entitlements", str(args.entitlements.resolve())])
        subprocess.run([*signing, str(bundle)], check=True)
        subprocess.run(["codesign", "--verify", "--deep", "--strict", str(bundle)], check=True)
        archive_path = stage / "result.ipa"
        with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(bundle.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(stage))
        archive_path.replace(output)
    print(f"Created {output}. Re-sign it with your sideloading tool before installation.")


if __name__ == "__main__":
    main()

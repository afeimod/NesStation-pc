#!/usr/bin/env python3
"""列出 GitHub 仓库某 ref 的全部一级子模块：path sha url

用法: emu_submodules.py <owner/repo> <ref>
输出: 每行 "path sha url"（url 来自 .gitmodules；相对 url 转绝对）
"""
import base64
import json
import sys
import urllib.request

API = "https://api.github.com"


def get_json(url):
    req = urllib.request.Request(url, headers={"User-Agent": "nesstation-src-fetch"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)


def main():
    repo, ref = sys.argv[1], sys.argv[2]
    # .gitmodules（path → url）
    mods = {}
    try:
        gj = get_json(f"{API}/repos/{repo}/contents/.gitmodules?ref={ref}")
        txt = base64.b64decode(gj["content"]).decode()
        cur = None
        for line in txt.splitlines():
            line = line.strip()
            if line.startswith("[submodule"):
                cur = {}
            elif cur is not None and "=" in line:
                k, v = [x.strip() for x in line.split("=", 1)]
                cur[k] = v
                if k == "path" and v not in mods:
                    mods[v] = cur
    except Exception as e:
        print(f"# .gitmodules 读取失败: {e}", file=sys.stderr)
    # git trees（mode=160000 = 子模块固定位点）
    d = get_json(f"{API}/repos/{repo}/git/trees/{ref}?recursive=1")
    if d.get("truncated"):
        print("# 警告: 树被截断", file=sys.stderr)
    owner = repo.split("/")[0]
    for e in d.get("tree", []):
        if e.get("mode") == "160000":
            p, sha = e["path"], e["sha"]
            url = mods.get(p, {}).get("url", "")
            if url.startswith("../"):
                url = f"https://github.com/{owner}/{url[3:]}"
            print(p, sha, url)


if __name__ == "__main__":
    main()

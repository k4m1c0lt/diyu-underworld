#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
publish_api.py —— 通过 GitHub REST API 发布本仓库。

适用场景：github.com 的 Git 传输被网络策略拦截，但 api.github.com 可达。
不需要 git push，也不需要 gh CLI，只用 Python 标准库。

用法（Windows）：
    set GITHUB_TOKEN=github_pat_xxxxxxxx
    set GITHUB_USER=你的用户名        （可选，token 能读 /user 时会自动识别）
    python publish_api.py

安全提示：
  * 请用「只对目标仓库授权」的 Personal Access Token，用完立刻在 GitHub 设置里撤销。
  * token 只从环境变量读取，不会写进任何文件、日志或仓库。
  * 创建仓库通常需要 classic PAT 的 repo 权限；若用细粒度 PAT，
    请先到 https://github.com/new 建好名为 diyu-underworld 的空公开仓库。
"""
import base64
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request

API = "https://api.github.com"
REPO = os.environ.get("GITHUB_REPO", "diyu-underworld")
TOKEN = os.environ.get("GITHUB_TOKEN", "").strip()
USER = os.environ.get("GITHUB_USER", "k4m1c0lt").strip()
COMMIT_MSG = "Initial release: 大闹阴曹·归山 - a C roguelite set in the Chinese underworld"


def req(method, path, payload=None):
    data = None if payload is None else json.dumps(payload).encode("utf-8")
    r = urllib.request.Request(API + path, data=data, method=method)
    r.add_header("Authorization", "Bearer " + TOKEN)
    r.add_header("Accept", "application/vnd.github+json")
    r.add_header("X-GitHub-Api-Version", "2022-11-28")
    r.add_header("Content-Type", "application/json")
    r.add_header("User-Agent", "diyu-publisher")
    try:
        with urllib.request.urlopen(r, timeout=45) as resp:
            body = resp.read()
            return resp.status, (json.loads(body) if body else {})
    except urllib.error.HTTPError as e:
        body = e.read()
        try:
            return e.code, (json.loads(body) if body else {})
        except ValueError:
            return e.code, {"message": body.decode("utf-8", "replace")[:300]}
    except urllib.error.URLError as e:
        return 0, {"message": "网络错误: %s" % e.reason}


def die(step, code, body):
    print("[失败] %s -> HTTP %s: %s" % (step, code, body.get("message", body)))
    sys.exit(1)


def main():
    if not TOKEN:
        print("请先设置 GITHUB_TOKEN 环境变量（Personal Access Token）。")
        print("例：set GITHUB_TOKEN=github_pat_xxxxxxxx")
        return 1

    owner = USER
    code, me = req("GET", "/user")
    if code == 200:
        owner = me.get("login") or owner
        print("GitHub 账号: %s" % owner)
    elif not owner:
        die("校验 token", code, me)
    else:
        print("按 GITHUB_USER 使用账号: %s（token 无 /user 权限）" % owner)

    base = "/repos/%s/%s" % (owner, REPO)

    # 1) 创建公开仓库；已存在则复用
    code, body = req("POST", "/user/repos", {
        "name": REPO,
        "description": "大闹阴曹·归山 —— 用纯 C 写的中国地府轻度肉鸽游戏（黄泉路 / 奈何桥 / 枉死城 / 阎罗殿）",
        "homepage": "",
        "private": False,
        "has_issues": True,
        "has_wiki": False,
        "has_projects": False,
        "auto_init": False,
        "license_template": "mit",
    })
    if code == 201:
        print("已创建公开仓库: %s" % body["html_url"])
    elif code == 422:
        print("仓库已存在，将写入现有仓库")
    else:
        exists, _ = req("GET", base)
        if exists != 200:
            print("[提示] 创建仓库失败（HTTP %s）。" % code)
            print("        细粒度 PAT 通常无权建仓，请先在 https://github.com/new")
            print("        手动建一个名为 %s 的空公开仓库，再重跑本脚本。" % REPO)
            return 1
        print("复用现有仓库: https://github.com/%s/%s" % (owner, REPO))

    # 2) 取待上传文件（与 git 已跟踪文件完全一致，自动排除存档与编译产物）
    here = os.path.dirname(os.path.abspath(__file__))
    os.chdir(here)
    try:
        files = subprocess.check_output(["git", "ls-files"], text=True).splitlines()
    except Exception as e:
        print("[失败] 无法读取 git ls-files: %s" % e)
        return 1
    files = [f for f in files if os.path.isfile(f)]
    if not files:
        print("[失败] 没有找到待上传文件")
        return 1
    print("待上传 %d 个文件" % len(files))

    # 3) 用 Git Data API 打成单个 commit
    tree = []
    for path in files:
        with open(path, "rb") as f:
            raw = f.read()
        c, b = req("POST", base + "/git/blobs", {
            "content": base64.b64encode(raw).decode("ascii"),
            "encoding": "base64",
        })
        if c not in (200, 201):
            die("上传 " + path, c, b)
        tree.append({"path": path, "mode": "100644", "type": "blob", "sha": b["sha"]})
        print("  ok  %-20s %8d B" % (path, len(raw)))

    c, t = req("POST", base + "/git/trees", {"tree": tree})
    if c not in (200, 201):
        die("创建 tree", c, t)
    c, cm = req("POST", base + "/git/commits", {
        "message": COMMIT_MSG, "tree": t["sha"], "parents": [],
    })
    if c not in (200, 201):
        die("创建 commit", c, cm)

    c, ref = req("POST", base + "/git/refs", {
        "ref": "refs/heads/main", "sha": cm["sha"],
    })
    if c not in (200, 201):
        c2, ref2 = req("PATCH", base + "/git/refs/heads/main", {
            "sha": cm["sha"], "force": True,
        })
        if c2 != 200:
            die("写入 main 分支", c2, ref2)
    req("PATCH", base, {"default_branch": "main"})

    print("\n[完成] https://github.com/%s/%s" % (owner, REPO))
    print("       分支 main，单个初始提交，%d 个文件已上传。" % len(files))
    return 0


if __name__ == "__main__":
    sys.exit(main())

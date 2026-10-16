#!/bin/bash
echo '请输入你的GitHub用户名：'
read username
git remote add origin https://github.com/$username/4399-ESP-Hack.git
git branch -M main
git push -u origin main

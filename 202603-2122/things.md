# OpenClaw/PicoClaw in RISC-V

又来了，记录一下在x86上搭openclaw的过程

```
docker pull ubuntu:26.04
docker run -it \
--privileged \
--network=host \
--ipc=host \
--cap-add=SYS_ADMIN \
--security-opt seccomp=unconfined \
ubuntu:26.04 \
/bin/bash

apt update
apt upgrade
apt install sudo curl build-essential systemd vim -y

curl -fsSL https://openclaw.ai/install.sh | bash

# openclaw.json
  "models": {
    "mode": "replace",
    "providers": {
      "modelstudio": {
        "baseUrl": "url",
        "apiKey": "apikey",
        "api": "openai-completions",
        "models": [
          {
            "id": "DeepSeek-V3.2",
            "name": "DeepSeek-V3.2",
            "reasoning": false,
            "input": [
              "text"
            ],
            "contextWindow": 9999999999999999999,
            "maxTokens": 999999999999999999
          },
          {
            "id": "Qwen3.5-397B-A17B",
            "name": "Qwen3.5-397B-A17B",
            "reasoning": false,
            "input": [
              "text"
            ],
            "contextWindow": 9999999999999,
            "maxTokens": 999999999999
          },
          {
            "id": "GLM-5-Turbo",
            "name": "GLM-5-Turbo",
            "reasoning": false,
            "input": [
              "text"
            ],
            "contextWindow": 9999999999999999999,
            "maxTokens": 9999999999999999999
          }
        ]
      }
    }
  },

npx clawhub@latest install sonoscli --registry=https://cn.clawhub-mirror.com

https://clawhub.ai/pskoett/self-improving-agent
https://clawhub.ai/oswalpalash/ontology
https://clawhub.ai/matrixy/agent-browser-clawdbot
https://clawhub.ai/guohongbin-git/skill-finder-cn
https://clawhub.ai/ivangdavila/productivity
https://clawhub.ai/othmanadi/planning-with-files
https://clawhub.ai/wscats/code-analysis-skills
https://clawhub.ai/ivangdavila/skill-finder
https://clawhub.ai/russellfei/file-manager

这些是我找到的技能，你来安装并且配置一下吧
```

---

又来了，之前配的是picoclaw，感觉不好用，然后在[公众号](https://mp.weixin.qq.com/s/S_6GweaJ1KmNbaP84QJSZQ?scene=1)找了一些skills，发现需要配node.js，但是node.js官方还没支持riscv64的prebuilt deb包，所以只好用unofficial的，下面记录一下：

```
curl -LO https://github.com/gounthar/unofficial-builds/releases/download/v24.12.0/nodejs-unofficial_24.12.0-1_riscv64.deb
sudo dpkg -i nodejs-unofficial_24.12.0-1_riscv64.deb
/opt/nodejs-unofficial/bin/node --version
```

装openclaw也很方便：

```
curl -fsSL https://openclaw.ai/install.sh | bash
```

具体配置的话，上网查就ok了，有很多教程。

---

超级心累....

明白为啥都叫“养"龙虾了，因为最初用它，如果不配置memory/soul/skills，那和用一个web-LLM没区别。

所以关键在于“培养"它，让它学会自学、自驱，遇到不会的问题，自己会尝试着解决；每做完一个事情，自己会尝试总结...

冥冥之中，感觉，如果一个人把龙虾“养"的很好，那这个龙虾，可能会和ta本人的思维逻辑/方式很像，而且还有用比ta更牛/更快/更高效的工作能力。

---

上面的内容是我养了一上午之后想说的，下面说些有用的吧。

为了安全性，我在lp4a上开了一个ubuntu docker容器，而后在里面直接安装了picoclaw的deb包，按照文档配置了飞书app。

简单培养了一下soul/memory/skills等，不过目前还很低级。

lp4a不可能一直开着(因为是在宿舍，且工作日白天是在工位上)，而如果每次都开机lp4a后再进入docker再启动picoclaw，虽然步骤不多，但是人工来做没必要，所以可以配置一个开机自启的服务。

先配置开机要运行的脚本

```shell
cat > /usr/local/bin/start-picoclaw.sh << 'EOF'
#!/bin/bash

echo "Starting container 'claw'..."
docker start claw

sleep 3

echo "Running picoclaw gateway..."
docker exec claw picoclaw gateway
EOF

chmod +x /usr/local/bin/start-picoclaw.sh
```

然后配置systemd

```shell
cat > /etc/systemd/system/picoclaw.service << 'EOF'
[Unit]
Description=PicoClaw Gateway in Docker
After=network.target docker.service
Requires=docker.service

[Service]
Type=simple
ExecStart=/usr/local/bin/start-picoclaw.sh
Restart=on-failure
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

# 重新加载 systemd 配置
sudo systemctl daemon-reload

# 设置开机自启
sudo systemctl enable picoclaw.service

# 立即启动（测试）
sudo systemctl start picoclaw.service

# 查看运行状态
sudo systemctl status picoclaw.service
```

有了上面的配置，每次手机打开热点+插上lp4a电源+电脑打开飞书就ok了

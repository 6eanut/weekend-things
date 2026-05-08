opencode: [https://github.com/anomalyco/opencode](https://github.com/anomalyco/opencode)

deepseek-tui: [https://github.com/Hmbown/DeepSeek-TUI](https://github.com/Hmbown/DeepSeek-TUI)

aider: [https://github.com/Aider-AI/aider](https://github.com/Aider-AI/aider)

goose: [https://github.com/aaif-goose/goose](https://github.com/aaif-goose/goose)

openhands: [https://github.com/OpenHands/OpenHands](https://github.com/OpenHands/OpenHands)

codex: [https://github.com/openai/codex](https://github.com/openai/codex)

gemini-cli: [https://github.com/google-gemini/gemini-cli](https://github.com/google-gemini/gemini-cli)


我的想法是构建一个coding agent，它的特点是模块化。

目前已经有了不少成熟的开源coding agent，比如opencode, deepseek-tui, aider等。

第一步需要做的事情是通过agent teams对上面提到的每个coding agent做调研，看看其分别支持哪些功能。

第二步是看看这些功能中，有哪些是每个coding agent都具备的，哪些是个别coding agent独有的。

我的想法是，我们实现一个coding agent(名字还没想好，不过我觉得可以起名乐高或积木这种具有模块化特点的东西)。

整个coding agent以一个github orgnization组织起来，里面有多个仓库，其中最基本的是base仓库，也就是coding agent的主体，它实现了现有的成熟开源coding agent所具有的通用功能。然后其它每个仓库都是一个extension，每个仓库实现的是一个特有的功能。

base和extension之间的关系就是插座和插头，想要往base上加入extension非常方便，只要下载extension并且通过一条简单的命令插入即可。


这是我今天忽然心血来潮的一个项目，不知道是否有价值/卖点，不知道可行性如何。

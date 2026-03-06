#!/usr/bin/env python3
import sys
from pathlib import Path
from openai import OpenAI

def read_api_key(path="openai.key") -> str:
    key = Path(path).read_text().strip()
    if not key:
        raise RuntimeError("API key file is empty")
    return key

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <question_file>", file=sys.stderr)
        sys.exit(1)

    question_file = Path(sys.argv[1])
    if not question_file.exists():
        raise RuntimeError(f"File not found: {question_file}")

    question = question_file.read_text().strip()
    if not question:
        raise RuntimeError("Question file is empty")

    client = OpenAI(
        api_key=read_api_key(),
        base_url="https://chatbox.isrc.ac.cn/api/"
    )

    system_prompt = (
        "You are a Linux kernel and syzkaller analysis assistant.\n"
        "Respond concisely and clearly.\n"
        "Always produce a Markdown document.\n"
        "Do not include internal reasoning.\n"
    )

    response = client.chat.completions.create(
        model="gpt-5",
        messages=[
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": question},
        ],
        temperature=0.1,
    )

    msg = response.choices[0].message.content
    if not msg or not msg.strip():
        raise RuntimeError("Model returned empty output")

    # 直接输出 Markdown 到 stdout
    print(msg)

if __name__ == "__main__":
    main()

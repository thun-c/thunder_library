# ymatsuxさんのコードを参考にしています
# https://github.com/ymatsux/competitive-programming/tree/main/combiner
import re
import sys
from typing import Set
import os

def read_file_content(file_name: str) -> str:
    with open(file_name, "r") as f:
        return f.read()


def make_converted_file_content(
        file_name: str,
        stack: list[str],
        added_file_names: Set[str]) -> str:
    print(file_name, added_file_names)
    content = read_file_content(file_name)
    content_lines = content.split("\n")
    base_dir = os.path.dirname(file_name)  # 現在のファイルのディレクトリを取得
    
    for i in range(len(content_lines)):
        line = content_lines[i]
        match = re.match(r"^\s*#include\s+\"(.*)\"\s*$", line)
        if match:
            included_file = os.path.join(base_dir, match.group(1))  # 相対パスを結合
            if included_file not in added_file_names:
                if included_file in stack:
                    print("Circular dependency detected: "
                          f"{' > '.join(stack + [included_file])}")
                    exit(1)
                stack.append(included_file)
                content_lines[i] = make_converted_file_content(
                    included_file, stack, added_file_names)
                added_file_names.add(included_file)
                stack.pop()
            else:
                content_lines[i] = ""
    return "\n".join(content_lines)


def main(argv: list[str]):
    if len(argv) != 3:
        print("Usage: python3 combiner.py <source_file> <output_file>")
        sys.exit(1)
    converted_main_file_content = make_converted_file_content(
        argv[1], [argv[1]], set())
    with open(argv[2], "w") as f:
        f.write(converted_main_file_content)


if __name__ == "__main__":
    main(sys.argv)

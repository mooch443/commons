import re

def update_code_block(file_path, marker):
    with open(file_path, 'r') as f:
        new_content = f.read()

    with open('README.md', 'r') as f:
        readme_content = f.read()

    pattern = re.compile(f"## {marker}.*?```", re.DOTALL)
    replacement = f"## {marker}\n\n```cpp\n{new_content}\n```"
    readme_content = re.sub(pattern, replacement, readme_content)

    with open('README.md', 'w') as f:
        f.write(readme_content)

if __name__ == "__main__":
    update_code_block('examples/simple_gui.cpp', 'Example: Dynamic GUI')
    update_code_block('examples/test_gui.json', 'Example: Dynamic GUI JSON')

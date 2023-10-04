import re

def update_code_block(file_path, marker):
    with open(file_path, 'r') as f:
        new_content = f.read()

    with open('README.md', 'r') as f:
        readme_content = f.read()

    # Updated pattern to look for marker, then ```
    # and capture everything until another ```
    pattern = re.compile(f"({marker}.*?```\\n)(.*?)(```)", re.DOTALL)
    
    # Updated replacement to keep the existing marker and ticks,
    # but replace the content in between
    replacement = f"\\1{new_content}\n\\3"
    readme_content = re.sub(pattern, replacement, readme_content)

    with open('README.md', 'w') as f:
        f.write(readme_content)

if __name__ == "__main__":
    update_code_block('examples/simple_gui.cpp', 'Example: Dynamic GUI')
    update_code_block('examples/test_gui.json', 'test_gui.json')

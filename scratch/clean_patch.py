import re
import sys

def apply_patch(file_path, patch_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    with open(patch_path, 'r', encoding='utf-8') as f:
        patch = f.read()
    
    # Split patch into hunks
    hunks = re.split(r'^@@ -(\d+),(\d+) \+(\d+),(\d+) @@.*$', patch, flags=re.MULTILINE)[1:]
    
    # Sort hunks in reverse order to avoid shifting line numbers
    parsed_hunks = []
    for i in range(0, len(hunks), 5):
        orig_start = int(hunks[i])
        orig_count = int(hunks[i+1])
        new_start = int(hunks[i+2])
        new_count = int(hunks[i+3])
        hunk_content = hunks[i+4]
        parsed_hunks.append((orig_start, orig_count, new_start, new_count, hunk_content))
    
    parsed_hunks.sort(key=lambda x: x[0], reverse=True)
    
    for orig_start, orig_count, new_start, new_count, hunk_content in parsed_hunks:
        hunk_lines = hunk_content.strip('\n').split('\n')
        # Filter lines (remove first char which is + or - or space)
        # Note: hunk_content starts after the @@ line
        
        # We need to find where the hunk starts in the original file
        # The line numbers in diffs are often slightly off, so we match context
        
        # Actually, let's just use the line numbers if they are reliable
        # Or better: use git apply via shell if we can fix the whitespace
        pass

# Since git apply is more reliable, let's try to fix the diff file instead
with open('scratch/ropera_diff.txt', 'r', encoding='utf-8') as f:
    diff_lines = f.readlines()

# Clean diff: remove trailing whitespace and ensure correct format
cleaned_lines = []
for line in diff_lines:
    if line.startswith('+') or line.startswith('-') or line.startswith(' '):
        # Keep the first character but strip the rest and add back newline
        content = line[1:].rstrip()
        cleaned_lines.append(line[0] + content + '\n')
    else:
        cleaned_lines.append(line.rstrip() + '\n')

with open('scratch/ropera_diff_clean.patch', 'w', encoding='utf-8') as f:
    f.writelines(cleaned_lines)

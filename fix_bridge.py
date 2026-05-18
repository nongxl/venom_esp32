lines = open('src/Venom.cpp', 'r', encoding='utf-8').readlines()
# Remove lines 929-959 (1-indexed), which is index 928-958 (0-indexed)
# Line 929 is the blank line + bridge comment, line 959 is the closing '    }'
# But lines 958-959 ('        }' and '    }') are PART of the orphaned block too
# After main loop ends at line 927 ('    }'), the orphan runs from 928 (blank) to 959 ('    }')
# We keep everything up to and including line 927 (index 926), then skip to line 961 (index 960)
new_lines = lines[:927] + lines[960:]
open('src/Venom.cpp', 'w', encoding='utf-8').writelines(new_lines)
print(f"Done. File now has {len(new_lines)} lines (was {len(lines)}).")
print("Lines around removal:")
for i in range(924, 932):
    print(f"  Line {i+1}: {repr(new_lines[i][:80])}")

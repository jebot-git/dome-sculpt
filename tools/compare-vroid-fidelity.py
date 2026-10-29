#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Optional offline PNG comparison; requires Pillow, no application dependency."""
import json
from pathlib import Path
import sys
from PIL import Image

folder = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('artifacts')
rows = []
for gender in ['female', 'male']:
    for view in ['face', 'body']:
        source = Image.open(folder / f'vroid-{gender}-source-{view}.png').convert('RGB')
        native = Image.open(folder / f'vroid-{gender}-native-{view}.png').convert('RGB')
        if source.size != native.size:
            raise ValueError('Comparison dimensions differ')
        differences = [abs(a-b) for a, b in zip(source.tobytes(), native.tobytes())]
        above = sum(max(differences[i:i+3]) > 2 for i in range(0, len(differences), 3))
        row = dict(gender=gender, view=view,
                   mean_absolute_channel_error_8bit=sum(differences)/len(differences),
                   max_channel_error_8bit=max(differences),
                   pixel_fraction_any_channel_over2=above/(len(differences)/3))
        rows.append(row)
        print(json.dumps(row))
(folder / 'vroid-render-comparison.json').write_text(json.dumps(rows, indent=2)+'\n')

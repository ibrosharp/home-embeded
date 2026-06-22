import os
import gzip

data_dir = r'c:\Users\abdul\OneDrive\Documents\Arduino\remote\data'
out_file = r'c:\Users\abdul\OneDrive\Documents\Arduino\remote\BootstrapFS.hpp'

files_to_pack = ['index.html', 'style.css', 'app.js']

with open(out_file, 'w', encoding='utf-8') as out:
    out.write('#pragma once\n')
    out.write('#include <Arduino.h>\n')
    out.write('#include <LittleFS.h>\n\n')

    for fname in files_to_pack:
        fpath = os.path.join(data_dir, fname)
        with open(fpath, 'rb') as f:
            raw_data = f.read()
            
        data = gzip.compress(raw_data)
        
        var_name = fname.replace('.', '_') + '_gz'
        out.write(f'const uint8_t {var_name}_data[] PROGMEM = {{\n')
        
        # Write bytes
        for i, b in enumerate(data):
            out.write(f'0x{b:02x}, ')
            if (i + 1) % 16 == 0:
                out.write('\n')
        out.write('\n};\n')
        out.write(f'const size_t {var_name}_len = {len(data)};\n\n')

    # Write bootstrap function with version tracking
    out.write('void bootstrapLittleFS() {\n')
    out.write('  String currentVersion = String(__DATE__) + " " + String(__TIME__);\n')
    out.write('  String fsVersion = "";\n')
    out.write('  if (LittleFS.exists("/version.txt")) {\n')
    out.write('    File vFile = LittleFS.open("/version.txt", "r");\n')
    out.write('    if (vFile) {\n')
    out.write('      fsVersion = vFile.readString();\n')
    out.write('      vFile.close();\n')
    out.write('    }\n')
    out.write('  }\n\n')
    out.write('  if (fsVersion != currentVersion) {\n')
    out.write('    Serial.println("Firmware updated! Overwriting LittleFS web files...");\n')
    
    for fname in files_to_pack:
        var_name = fname.replace('.', '_') + '_gz'
        out.write(f'    Serial.println("Writing /{fname}.gz...");\n')
        out.write(f'    File f_{var_name} = LittleFS.open(\"/{fname}.gz\", \"w\");\n')
        out.write(f'    if (f_{var_name}) {{\n')
        out.write(f'      f_{var_name}.write({var_name}_data, {var_name}_len);\n')
        out.write(f'      f_{var_name}.close();\n')
        out.write(f'    }}\n')
        
    out.write('    File vFile = LittleFS.open("/version.txt", "w");\n')
    out.write('    if (vFile) {\n')
    out.write('      vFile.print(currentVersion);\n')
    out.write('      vFile.close();\n')
    out.write('    }\n')
    out.write('    Serial.println("Web files updated successfully!");\n')
    out.write('  } else {\n')
    out.write('    Serial.println("Web files are up to date.");\n')
    out.write('  }\n')
    out.write('}\n')

print('BootstrapFS.hpp generated successfully with version tracking!')

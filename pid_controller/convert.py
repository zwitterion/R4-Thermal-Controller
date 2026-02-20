import gzip
import argparse
import minify_html


def create_header():
    parser = argparse.ArgumentParser(description='Generate web_assets.h')
    parser.add_argument('--mode', choices=['dev', 'prod'], default='dev', help='Generation mode')
    args = parser.parse_args()

    try:
        with open("index.html", "rb") as f:
            content = f.read()
        
        if args.mode == 'prod':
            zipped = gzip.compress(content)
            hex_array = ", ".join(f"0x{b:02x}" for b in zipped)
            
            raw_decl = 'const char index_html_raw[] = "";'
            raw_len_decl = 'const size_t index_html_raw_len = 0;'
            
            gz_decl = f'const uint8_t index_html_gz[] = {{ {hex_array} }};'
            gz_len_decl = f'const size_t index_html_gz_len = {len(zipped)};'
        else:
            content_str = content.decode('utf-8')
            content_str = minify_html.minify(content_str, minify_js=True, minify_css=True, remove_processing_instructions=True)
            raw_decl = f'const char index_html_raw[] = R"rawliteral({content_str})rawliteral";'
            raw_len_decl = 'const size_t index_html_raw_len = sizeof(index_html_raw) - 1;'
            
            gz_decl = 'const uint8_t index_html_gz[] = { 0 };'
            gz_len_decl = 'const size_t index_html_gz_len = 0;'
        
        favicon_hex = "0x00"
        favicon_len = 1
        try:
            with open("favicon.ico", "rb") as f:
                fav_content = f.read()
                fav_zipped = gzip.compress(fav_content)
                favicon_hex = ", ".join(f"0x{b:02x}" for b in fav_zipped)
                favicon_len = len(fav_zipped)
        except FileNotFoundError:
            print("Warning: favicon.ico not found. Using placeholder.")

        header_content = f"""#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

// Mode: {args.mode}
{raw_decl}
{raw_len_decl}

{gz_decl}
{gz_len_decl}

const unsigned char favicon_gz[] = {{ {favicon_hex} }};
const size_t favicon_gz_len = {favicon_len};
#endif
"""
        with open("web_assets.h", "w", encoding='utf-8') as f:
            f.write(header_content)
        print(f"web_assets.h created successfully in {args.mode} mode.")
        
    except FileNotFoundError:
        print("Error: index.html not found.")

if __name__ == "__main__":
    create_header()

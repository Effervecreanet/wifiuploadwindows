@echo off
REM This script should be run in x64 Native Tools Command prompt
REM You have to run "rc resource.rc" to have "resource.res" file
echo cl wu_msg.c wu_content.c wu_http_loop.c wu_http_nv.c wu_http_receive.c wu_http_theme.c wu_socket.c wu_http.c wu_available_address.c wu_log.c wu_main.c resource.res /Fe:wifiupload_en.exe
cl wu_msg.c wu_content.c wu_http_loop.c wu_http_nv.c wu_http_receive.c wu_http_theme.c wu_socket.c wu_http.c wu_available_address.c wu_log.c wu_main.c resource.res /Fe:wifiupload_en.exe

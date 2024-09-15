@echo off

pushd "%~dp0"

copy build\esp32-thermostat-display.bin \\srvmain\wwwroot\esp32\ota

popd
